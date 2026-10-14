/*
  CP2 | TinyML -- firmware do carrinho WALL-E com modelo embarcado

  Evolucao do firmware do CP1 (Codigos/PS5_ESP32_Bluepad32.ino). Continua
  igual: controle PS5, rampa, zona morta, freio de 20 cm, calibracao web e OTA.

  O que entra no CP2:
  - o HC-SR04 e lido em ritmo fixo (o mesmo do treino) e as ultimas leituras
    ficam numa janela deslizante;
  - a cada INFERENCIA_PASSO leituras novas, o modelo do Edge Impulse classifica
    a janela em: vazio / parado / aproximando / aceno;
  - a classe muda o comportamento do carrinho (ver aplicaClasse()):
      vazio        -> luz verde no controle, velocidade normal
      parado       -> luz amarela, avanco limitado a 50%
      aproximando  -> luz vermelha; parado: WALL-E "se assusta" e da re
      aceno        -> luz azul;     parado: WALL-E "cumprimenta" (balanca)
*/

#include <carrinho_walle_tinyml_v2_inferencing.h>   // biblioteca exportada do Edge Impulse (modelo v2)
#include <Bluepad32.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <WebServer.h>
#include <Preferences.h>
#include "secrets.h"   // define WIFI_SSID e WIFI_PASS aqui (nao vai pro git)

#if EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME != 1
#error "O modelo deve ter um unico eixo de entrada (distancia)."
#endif

const char* OTA_HOSTNAME = "carrinho-esp32";

// ---- Pinos dos motores ----
#define IN1 26   // Motor A
#define IN2 27
#define IN3 32   // Motor B (polaridade corrigida no codigo)
#define IN4 33

// Canais do PWM (core antigo do Bluepad32 usa API por canal, nao por pino)
#define CH_IN1 0
#define CH_IN2 1
#define CH_IN3 2
#define CH_IN4 3

// ---- Velocidade maxima (0 a 255) ----
int velMax = 200;

// ---- Suavidade da rampa ----
const int  RAMP_PASSO     = 12;
const long RAMP_INTERVALO = 15;

// Zona morta em unidades cruas do stick (eixo vai de -511 a 512)
const int ZONA_MORTA = 80;   // ~15% de 512

// Limiar dos gatilhos analogicos (throttle/brake vao de 0 a 1023)
const int LIMIAR_GATILHO = 400;

// ---- Sensor de distancia HC-SR04 (alimentado em 3.3V, sem divisor de tensao) ----
#define PINO_TRIG 4
#define PINO_ECHO 18

const float DISTANCIA_MIN_CM = 20.0f;              // abaixo disso, bloqueia o avanco
const unsigned long SENSOR_TIMEOUT_US = 15000UL;   // ~2.5m, evita travar muito tempo sem eco
const float DIST_MAX_CM = 200.0f;                  // sem eco ou acima disso vira 200 (igual a coleta)

// Periodo de leitura = periodo do dataset (60 ms), lido da propria biblioteca
const unsigned long SENSOR_INTERVALO = (unsigned long)(EI_CLASSIFIER_INTERVAL_MS + 0.5f);

float distanciaCm = DIST_MAX_CM;
unsigned long proximaLeitura = 0;

// ---- TinyML ----
const int   JANELA            = EI_CLASSIFIER_RAW_SAMPLE_COUNT;  // leituras por inferencia
const int   INFERENCIA_PASSO  = 3;       // infere a cada 3 leituras novas (180 ms)
const float CONFIANCA_MIN     = 0.80f;   // abaixo disso, a classificacao e ignorada
const int   CONFIRMACOES      = 3;       // inferencias seguidas iguais para aceitar a classe (540 ms)
const unsigned long OCIOSO_MS = 500;     // sticks soltos por esse tempo = robo parado
const unsigned long PAUSA_POS_MANOBRA_MS = 2000;  // janela inteira sem o movimento do proprio robo

float janela[JANELA];        // buffer circular com as ultimas leituras
int   posJanela = 0;
int   leiturasNaJanela = 0;
int   leiturasDesdeInferencia = 0;

enum Classe { C_VAZIO, C_PARADO, C_APROXIMANDO, C_ACENO, C_INCERTO };
const char* NOME_CLASSE[] = {"vazio", "parado", "aproximando", "aceno", "incerto"};

Classe classeAtual   = C_VAZIO;   // classe confirmada que esta valendo
Classe candidata     = C_INCERTO;
int    repeticoes    = 0;
float  confiancaAtual = 0;
float  probs[EI_CLASSIFIER_LABEL_COUNT];
int    tempoInferenciaMs = 0;
unsigned long totalInferencias = 0;

// ---- Manobras disparadas pelo modelo ----
struct Passo { int a; int b; unsigned long ms; };

const Passo SUSTO[] = {            // "aproximando": recua
  {-190, -190, 600},
  {   0,    0, 200},
};
const Passo CUMPRIMENTO[] = {      // "aceno": balanca esquerda-direita
  { 170, -170, 300},
  {-170,  170, 300},
  { 170, -170, 300},
  {-170,  170, 300},
  {   0,    0, 200},
};

const Passo* manobra = nullptr;
int manobraPassos = 0;
int manobraIndice = 0;
unsigned long inicioPasso = 0;
unsigned long fimUltimaManobra = 0;
unsigned long ultimoComando = 0;   // ultima vez que um stick saiu da zona morta

// ---- Calibracao dos motores (0.30 a 1.00), ajustavel pela tela web ----
float trimA = 1.0f;
float trimB = 1.0f;

int alvoA = 0, alvoB = 0;
int atualA = 0, atualB = 0;
unsigned long ultimoRamp = 0;

ControllerPtr meuControle = nullptr;
Preferences prefs;
WebServer server(80);

int aproxima(int atual, int alvo) {
  if (atual < alvo) { atual += RAMP_PASSO; if (atual > alvo) atual = alvo; }
  else if (atual > alvo) { atual -= RAMP_PASSO; if (atual < alvo) atual = alvo; }
  return atual;
}

void aplicaMotorA(int s) {
  int sc = (int)(s * trimA);
  if (sc > 0) { ledcWrite(CH_IN1, sc);  ledcWrite(CH_IN2, 0); }
  else if (sc < 0) { ledcWrite(CH_IN1, 0); ledcWrite(CH_IN2, -sc); }
  else { ledcWrite(CH_IN1, 0); ledcWrite(CH_IN2, 0); }
}
void aplicaMotorB(int s) {           // polaridade invertida
  int sc = (int)(s * trimB);
  if (sc > 0) { ledcWrite(CH_IN3, 0); ledcWrite(CH_IN4, sc); }
  else if (sc < 0) { ledcWrite(CH_IN3, -sc); ledcWrite(CH_IN4, 0); }
  else { ledcWrite(CH_IN3, 0); ledcWrite(CH_IN4, 0); }
}

void atualizaRampa() {
  if (millis() - ultimoRamp < RAMP_INTERVALO) return;
  ultimoRamp = millis();
  atualA = aproxima(atualA, alvoA);
  atualB = aproxima(atualB, alvoB);
  aplicaMotorA(atualA);
  aplicaMotorB(atualB);
}

// ---------------- Sensor ----------------

// Mesma funcao do firmware de coleta (coleta/coleta_hcsr04): o modelo
// aprendeu exatamente este formato. Sem eco ou acima de 200 cm -> 200 cm.
float leDistanciaModelo() {
  digitalWrite(PINO_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PINO_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PINO_TRIG, LOW);

  unsigned long duracao = pulseIn(PINO_ECHO, HIGH, SENSOR_TIMEOUT_US);
  if (duracao == 0) return DIST_MAX_CM;
  float cm = duracao / 58.0f;   // formula padrao do HC-SR04
  if (cm > DIST_MAX_CM) cm = DIST_MAX_CM;
  return cm;
}

// Le o sensor no ritmo do dataset e guarda na janela. Devolve true se leu.
bool atualizaSensorDistancia() {
  if ((long)(millis() - proximaLeitura) < 0) return false;
  proximaLeitura += SENSOR_INTERVALO;
  // Se o loop atrasou mais de uma leitura (ex.: gravacao OTA), retoma o ritmo
  if ((long)(millis() - proximaLeitura) > (long)SENSOR_INTERVALO) proximaLeitura = millis() + SENSOR_INTERVALO;

  distanciaCm = leDistanciaModelo();
  janela[posJanela] = distanciaCm;
  posJanela = (posJanela + 1) % JANELA;
  if (leiturasNaJanela < JANELA) leiturasNaJanela++;
  return true;
}

// ---------------- Controle ----------------

// Cor da barra de luz do DualSense para cada classe
void mostraClasseNoControle(Classe c) {
  if (meuControle == nullptr || !meuControle->isConnected()) return;
  switch (c) {
    case C_VAZIO:       meuControle->setColorLED(0, 255, 0);   break;
    case C_PARADO:      meuControle->setColorLED(255, 160, 0); break;
    case C_APROXIMANDO: meuControle->setColorLED(255, 0, 0);   break;
    case C_ACENO:       meuControle->setColorLED(0, 60, 255);  break;
    default: break;
  }
}

void vibraControle() {
  if (meuControle == nullptr || !meuControle->isConnected()) return;
  meuControle->setRumble(0xC0, 0xC0);   // forca, duracao (API do Bluepad32 3.x)
}

// Chamado automaticamente quando um controle conecta
void onConnectedController(ControllerPtr ctl) {
  if (meuControle == nullptr) {
    Serial.println("Controle conectado!");
    meuControle = ctl;
    mostraClasseNoControle(classeAtual);
  } else {
    Serial.println("Outro controle tentou conectar, ignorado (ja tem um em uso).");
  }
}

// Chamado automaticamente quando o controle desconecta
void onDisconnectedController(ControllerPtr ctl) {
  if (meuControle == ctl) {
    Serial.println("Controle desconectado.");
    meuControle = nullptr;
  }
}

// ---------------- Manobras ----------------

bool manobraAtiva() { return manobra != nullptr; }

void iniciaManobra(const Passo* passos, int n) {
  manobra = passos;
  manobraPassos = n;
  manobraIndice = 0;
  inicioPasso = millis();
  alvoA = manobra[0].a;
  alvoB = manobra[0].b;
}

void encerraManobra() {
  manobra = nullptr;
  alvoA = 0; alvoB = 0;
  fimUltimaManobra = millis();
  candidata = C_INCERTO;   // a proxima classe sai de leituras novas
  repeticoes = 0;
}

void atualizaManobra() {
  if (!manobraAtiva()) return;
  if (millis() - inicioPasso < manobra[manobraIndice].ms) return;
  manobraIndice++;
  if (manobraIndice >= manobraPassos) { encerraManobra(); return; }
  inicioPasso = millis();
  alvoA = manobra[manobraIndice].a;
  alvoB = manobra[manobraIndice].b;
}

bool roboOcioso() {
  return !manobraAtiva()
      && millis() - ultimoComando > OCIOSO_MS
      && atualA == 0 && atualB == 0
      && millis() - fimUltimaManobra > PAUSA_POS_MANOBRA_MS;
}

// ---------------- Inferencia ----------------

Classe classeDoRotulo(const char* rotulo) {
  for (int c = C_VAZIO; c < C_INCERTO; c++) {
    if (strcmp(rotulo, NOME_CLASSE[c]) == 0) return (Classe)c;
  }
  return C_INCERTO;
}

// O que cada classe faz no carrinho
void aplicaClasse(Classe c) {
  if (!roboOcioso()) return;   // manobras so com o robo parado e sem piloto mexendo
  if (c == C_APROXIMANDO) {
    Serial.println("TinyML | acao: susto (re)");
    vibraControle();
    iniciaManobra(SUSTO, sizeof(SUSTO) / sizeof(SUSTO[0]));
  } else if (c == C_ACENO) {
    Serial.println("TinyML | acao: cumprimento (balanca)");
    iniciaManobra(CUMPRIMENTO, sizeof(CUMPRIMENTO) / sizeof(CUMPRIMENTO[0]));
  }
}

void rodaInferencia() {
  // Copia a janela circular em ordem cronologica (mais antiga primeiro)
  float entrada[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];
  for (int i = 0; i < JANELA; i++) entrada[i] = janela[(posJanela + i) % JANELA];

  signal_t sinal;
  if (numpy::signal_from_buffer(entrada, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &sinal) != 0) return;

  ei_impulse_result_t resultado = {};
  EI_IMPULSE_ERROR erro = run_classifier(&sinal, &resultado, false);
  if (erro != EI_IMPULSE_OK) {
    Serial.print("TinyML | erro no run_classifier: ");
    Serial.println(erro);
    return;
  }
  totalInferencias++;
  tempoInferenciaMs = resultado.timing.dsp + resultado.timing.classification;

  // Classe mais provavel
  int melhor = 0;
  for (int i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
    probs[i] = resultado.classification[i].value;
    if (probs[i] > probs[melhor]) melhor = i;
  }
  Classe c = probs[melhor] >= CONFIANCA_MIN ? classeDoRotulo(resultado.classification[melhor].label) : C_INCERTO;

  Serial.print("TinyML | dist ");
  Serial.print(distanciaCm, 1);
  Serial.print(" cm |");
  for (int i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
    Serial.print(' ');
    Serial.print(resultado.classification[i].label);
    Serial.print(' ');
    Serial.print(probs[i], 2);
  }
  Serial.print(" | ");
  Serial.print(tempoInferenciaMs);
  Serial.print(" ms -> ");
  Serial.println(NOME_CLASSE[c]);

  // Confirmacao: a mesma classe em inferencias seguidas
  if (c == C_INCERTO) { repeticoes = 0; candidata = C_INCERTO; return; }
  if (c == candidata) repeticoes++;
  else { candidata = c; repeticoes = 1; }
  if (repeticoes < CONFIRMACOES) return;

  if (c != classeAtual) {
    classeAtual = c;
    mostraClasseNoControle(classeAtual);
  }
  confiancaAtual = probs[melhor];
  aplicaClasse(classeAtual);
}

// ---------------- Tela web de calibracao ----------------

const char PAGINA_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Carrinho WALL-E</title>
<style>
  body { font-family: sans-serif; background:#111; color:#eee; text-align:center; padding:20px; }
  h1 { font-size: 1.3em; }
  .bloco { margin: 30px 0; }
  input[type=range] { width: 90%; height: 30px; }
  .valor { font-size: 1.5em; margin: 8px 0; }
  .classe { font-size: 2.2em; font-weight: bold; margin: 8px 0; }
  .prob { display:flex; align-items:center; gap:8px; max-width:420px; margin:6px auto; text-align:left; }
  .prob span { width:110px; }
  .prob div { flex:1; background:#333; height:16px; border-radius:8px; overflow:hidden; }
  .prob i { display:block; height:100%; background:#4caf50; }
  .aviso { font-size: 0.85em; color:#aaa; margin-top:30px; }
</style>
</head>
<body>
  <h1>TinyML no ESP32</h1>
  <div class="bloco">
    <div class="classe" id="classe">--</div>
    <div id="detalhe">--</div>
    <div id="probs"></div>
  </div>

  <div class="bloco">
    <div>Sensor de distancia (HC-SR04)</div>
    <div class="valor" id="valDist">--</div>
  </div>

  <h1>Calibracao dos motores</h1>
  <p>Ande pra frente com o controle e ajuste o motor mais forte ate o carrinho andar reto.</p>

  <div class="bloco">
    <div>Motor A (esquerdo)</div>
    <div class="valor" id="valA">--</div>
    <input type="range" min="30" max="100" id="sliderA" oninput="mudou('a', this.value)">
  </div>

  <div class="bloco">
    <div>Motor B (direito)</div>
    <div class="valor" id="valB">--</div>
    <input type="range" min="30" max="100" id="sliderB" oninput="mudou('b', this.value)">
  </div>

  <p class="aviso">Os valores sao salvos automaticamente no ESP32.</p>

<script>
const CORES = { vazio:'#4caf50', parado:'#ffb300', aproximando:'#f44336', aceno:'#2979ff' };
let editando = false;
function carregar() {
  fetch('/get').then(r => r.json()).then(d => {
    if (!editando) {
      document.getElementById('sliderA').value = Math.round(d.a * 100);
      document.getElementById('sliderB').value = Math.round(d.b * 100);
      document.getElementById('valA').innerText = Math.round(d.a * 100) + '%';
      document.getElementById('valB').innerText = Math.round(d.b * 100) + '%';
    }
    document.getElementById('valDist').innerText = d.dist >= 200 ? 'sem leitura' : d.dist.toFixed(1) + ' cm';
    const el = document.getElementById('classe');
    el.innerText = d.classe;
    el.style.color = CORES[d.classe] || '#eee';
    document.getElementById('detalhe').innerText =
      'confianca ' + Math.round(d.conf * 100) + '% | inferencia ' + d.ms + ' ms | ' + d.n + ' inferencias' +
      (d.manobra ? ' | manobra em curso' : '');
    let html = '';
    for (const k in d.probs) {
      html += '<div class="prob"><span>' + k + ' ' + Math.round(d.probs[k] * 100) + '%</span><div><i style="width:' +
              Math.round(d.probs[k] * 100) + '%;background:' + (CORES[k] || '#888') + '"></i></div></div>';
    }
    document.getElementById('probs').innerHTML = html;
  });
}
setInterval(carregar, 200);
let tId = null;
function mudou(motor, val) {
  editando = true;
  document.getElementById('val' + motor.toUpperCase()).innerText = val + '%';
  clearTimeout(tId);
  tId = setTimeout(() => { enviar(); editando = false; }, 150);
}
function enviar() {
  const a = document.getElementById('sliderA').value;
  const b = document.getElementById('sliderB').value;
  fetch('/set?a=' + a + '&b=' + b);
}
carregar();
</script>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.send_P(200, "text/html", PAGINA_HTML);
}

void handleGet() {
  String json = "{\"a\":" + String(trimA, 2) + ",\"b\":" + String(trimB, 2) +
                ",\"dist\":" + String(distanciaCm, 1) +
                ",\"classe\":\"" + NOME_CLASSE[classeAtual] + "\"" +
                ",\"conf\":" + String(confiancaAtual, 2) +
                ",\"ms\":" + String(tempoInferenciaMs) +
                ",\"n\":" + String(totalInferencias) +
                ",\"manobra\":" + (manobraAtiva() ? "true" : "false") +
                ",\"probs\":{";
  for (int i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
    if (i) json += ",";
    json += "\"" + String(ei_classifier_inferencing_categories[i]) + "\":" + String(probs[i], 3);
  }
  json += "}}";
  server.send(200, "application/json", json);
}

void handleSet() {
  if (server.hasArg("a")) {
    float v = server.arg("a").toFloat() / 100.0f;
    if (v < 0.30f) v = 0.30f;
    if (v > 1.00f) v = 1.00f;
    trimA = v;
    prefs.putFloat("trimA", trimA);
  }
  if (server.hasArg("b")) {
    float v = server.arg("b").toFloat() / 100.0f;
    if (v < 0.30f) v = 0.30f;
    if (v > 1.00f) v = 1.00f;
    trimB = v;
    prefs.putFloat("trimB", trimB);
  }
  server.send(200, "text/plain", "OK");
}

// ---------------- Setup / Loop ----------------

void setup() {
  Serial.begin(115200);
  ledcSetup(CH_IN1, 1000, 8);
  ledcAttachPin(IN1, CH_IN1);
  ledcSetup(CH_IN2, 1000, 8);
  ledcAttachPin(IN2, CH_IN2);
  ledcSetup(CH_IN3, 1000, 8);
  ledcAttachPin(IN3, CH_IN3);
  ledcSetup(CH_IN4, 1000, 8);
  ledcAttachPin(IN4, CH_IN4);

  pinMode(PINO_TRIG, OUTPUT);
  pinMode(PINO_ECHO, INPUT);
  digitalWrite(PINO_TRIG, LOW);

  // Calibracao salva: carregada antes do wifi, para valer mesmo sem rede
  prefs.begin("carrinho", false);
  trimA = prefs.getFloat("trimA", 1.0f);
  trimB = prefs.getFloat("trimB", 1.0f);

  Serial.print("TinyML | modelo: ");
  Serial.print(JANELA);
  Serial.print(" leituras a cada ");
  Serial.print(SENSOR_INTERVALO);
  Serial.print(" ms, ");
  Serial.print(EI_CLASSIFIER_LABEL_COUNT);
  Serial.println(" classes");
  if (SENSOR_INTERVALO != 60) {
    Serial.println("TinyML | ATENCAO: o modelo nao foi treinado a 60 ms -- confira o dataset no Edge Impulse.");
  }

  // ---- Wifi ----
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Conectando no wifi");
  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < 15000) {
    delay(300);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.print("Wifi conectado! IP: ");
    Serial.println(WiFi.localIP());

    // ---- OTA ----
    if (MDNS.begin(OTA_HOSTNAME)) {
      Serial.println("mDNS ok");
    }
    ArduinoOTA.setHostname(OTA_HOSTNAME);
    ArduinoOTA.begin();
    Serial.println("OTA pronto. No Arduino IDE: Tools > Port > deve aparecer 'carrinho-esp32' na rede.");

    // ---- Tela de calibracao + TinyML ----
    server.on("/", handleRoot);
    server.on("/get", handleGet);
    server.on("/set", handleSet);
    server.begin();
    Serial.print("Painel: abra http://");
    Serial.print(WiFi.localIP());
    Serial.println(" no navegador do celular (mesmo wifi).");
  } else {
    Serial.println();
    Serial.println("Nao conectou no wifi. Seguindo sem OTA/painel web.");
  }

  Serial.println("Inicializando Bluepad32...");
  BP32.setup(&onConnectedController, &onDisconnectedController);

  // So descomente a linha abaixo, grave, teste, e comente de novo depois.
  // Ela apaga pareamentos antigos guardados no ESP32 -- util se o
  // controle ficar "preso" tentando reconectar em algo que nao existe mais.
  // BP32.forgetBluetoothKeys();

  proximaLeitura = millis();
  Serial.println("Pronto. Coloque o controle em modo pareamento (PS + Create ate piscar rapido).");
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();

  // Sensor -> janela -> modelo (roda mesmo sem controle conectado)
  if (atualizaSensorDistancia() && leiturasNaJanela == JANELA) {
    if (++leiturasDesdeInferencia >= INFERENCIA_PASSO) {
      leiturasDesdeInferencia = 0;
      rodaInferencia();
    }
  }

  atualizaManobra();
  atualizaRampa();

  // Busca dados novos do controle (precisa ser chamado sempre no loop)
  BP32.update();
  bool controleOk = meuControle != nullptr && meuControle->isConnected() && meuControle->isGamepad();

  // Leitura crua dos eixos (-511 a 512).
  // axisY: empurrar pra CIMA da valor NEGATIVO, entao invertemos o sinal.
  int frente = controleOk ? -meuControle->axisY() : 0;   // stick esquerdo, eixo Y
  int giro   = controleOk ?  meuControle->axisRX() : 0;  // stick direito, eixo X
  if (abs(frente) < ZONA_MORTA) frente = 0;
  if (abs(giro)   < ZONA_MORTA) giro   = 0;
  bool pilotando = (frente != 0 || giro != 0);
  if (pilotando) ultimoComando = millis();

  // Manobra do modelo em curso: o piloto sempre tem prioridade e pode interromper
  if (manobraAtiva()) {
    if (!pilotando) return;
    encerraManobra();
  }

  if (!controleOk) {
    alvoA = 0; alvoB = 0;
    return;
  }

  int fr = map(frente, -511, 512, -velMax, velMax);
  int gr = map(giro,   -511, 512, -velMax, velMax);

  // TinyML: obstaculo parado ou algo vindo na direcao do robo -> avanco limitado a 50%
  if ((classeAtual == C_PARADO || classeAtual == C_APROXIMANDO) && fr > 0) fr /= 2;

  // Freio de seguranca: se tem objeto perto, bloqueia so o avanco (re e giro continuam liberados)
  bool objetoPerto = (distanciaCm < DISTANCIA_MIN_CM);
  if (objetoPerto && fr > 0) fr = 0;

  int a = fr + gr;   // motor A
  int b = fr - gr;   // motor B
  a = constrain(a, -velMax, velMax);
  b = constrain(b, -velMax, velMax);

  alvoA = a;
  alvoB = b;

  // Gatilhos analogicos: throttle() = R2, brake() = L2 (0 a 1023)
  if (meuControle->throttle() > LIMIAR_GATILHO) { velMax += 1; if (velMax > 255) velMax = 255; }
  if (meuControle->brake()    > LIMIAR_GATILHO) { velMax -= 1; if (velMax < 80)  velMax = 80; }

  delay(15);
}
