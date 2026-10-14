/*
  CP2 | TinyML -- firmware do carrinho WALL-E com modelo embarcado (v3)

  Evolucao do firmware do CP1 (Codigos/PS5_ESP32_Bluepad32.ino). Continua
  igual: controle PS5, rampa, zona morta, freio de 20 cm, calibracao web e OTA.

  O que entra no CP2:
  - o HC-SR04 e lido em ritmo fixo (o mesmo do treino) e as ultimas leituras
    ficam numa janela deslizante de 1,8 s;
  - com algo a menos de PERTO_CM (50 cm), a cada leitura nova (60 ms)
    o modelo do Edge Impulse classifica a janela em: seguro / inseguro
    (inseguro = a distancia esta diminuindo rapido);
  - o estado muda o comportamento do carrinho (ver mudaEstado()):
      longe     -> nada a menos de 50 cm: modelo em espera, luz verde, anda normal
      seguro    -> perto, aproximacao lenta ou parado: luz amarela, anda normal
      inseguro  -> perto e aproximando rapido: luz vermelha, vibra e FREIA
                   (corta o avanco na hora; re e giro continuam liberados)
*/

#include <carrinho_walle_tinyml_v3_inferencing.h>   // biblioteca exportada do Edge Impulse (modelo v3)
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
unsigned long totalLeituras = 0;

// ---- TinyML ----
const int   JANELA            = EI_CLASSIFIER_RAW_SAMPLE_COUNT;  // leituras por inferencia
const float PERTO_CM          = 50.0f;   // o modelo so e consultado com algo mais perto que isso
const int   INFERENCIA_PASSO  = 1;       // perto de algo, infere a cada leitura nova (60 ms; leva ~1 ms)
const float CONFIANCA_MIN     = 0.80f;   // abaixo disso, a classificacao e ignorada
const int   CONFIRMA_INSEGURO = 3;       // inferencias seguidas para frear (180 ms)
const int   CONFIRMA_SEGURO   = 5;       // inferencias seguidas para liberar (300 ms)
const unsigned long FREIO_MIN_MS = 1000; // freio segura pelo menos isso depois do ultimo "inseguro"

float janela[JANELA];        // buffer circular com as ultimas leituras
int   posJanela = 0;
int   leiturasNaJanela = 0;
int   leiturasDesdeInferencia = INFERENCIA_PASSO;
bool  perto = false;

// seguro/inseguro vem do modelo; incerto e longe sao estados do firmware
enum Classe { C_SEGURO, C_INSEGURO, C_INCERTO, C_LONGE };
const char* NOME_CLASSE[] = {"seguro", "inseguro", "incerto", "longe"};

Classe estadoAtual   = C_LONGE;     // o que esta valendo no carrinho
Classe ultimaClasse  = C_INCERTO;   // saida da ultima inferencia
Classe candidata     = C_INCERTO;
int    repeticoes    = 0;
float  confiancaAtual = 0;
float  probs[EI_CLASSIFIER_LABEL_COUNT];
int    tempoInferenciaMs = 0;
unsigned long totalInferencias = 0;
unsigned long ultimoInseguro = 0;   // ultima vez que "inseguro" foi confirmado

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

// Corta o avanco na hora, sem rampa (re e giro continuam liberados)
void freiaAgora() {
  if (atualA + atualB <= 0) return;   // nao esta indo pra frente
  alvoA = 0; alvoB = 0;
  atualA = 0; atualB = 0;
  aplicaMotorA(0);
  aplicaMotorB(0);
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
  totalLeituras++;
  return true;
}

// Mediana das 3 ultimas leituras: uma falha de eco isolada (200 cm) nao
// tira o carrinho do modo "perto".
float distanciaFiltrada() {
  float a = janela[(posJanela + JANELA - 1) % JANELA];
  float b = janela[(posJanela + JANELA - 2) % JANELA];
  float c = janela[(posJanela + JANELA - 3) % JANELA];
  return max(min(a, b), min(max(a, b), c));
}

// ---------------- Controle ----------------

// Cor da barra de luz do DualSense para cada estado
void mostraEstadoNoControle(Classe c) {
  if (meuControle == nullptr || !meuControle->isConnected()) return;
  switch (c) {
    case C_LONGE:    meuControle->setColorLED(0, 255, 0);   break;
    case C_SEGURO:   meuControle->setColorLED(255, 160, 0); break;
    case C_INSEGURO: meuControle->setColorLED(255, 0, 0);   break;
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
    mostraEstadoNoControle(estadoAtual);
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

// ---------------- Inferencia ----------------

bool freioAtivo() { return estadoAtual == C_INSEGURO; }

// O que cada estado faz no carrinho
void mudaEstado(Classe c) {
  if (c == C_INSEGURO) {
    ultimoInseguro = millis();            // renova o freio
    freiaAgora();
  } else if (freioAtivo() && millis() - ultimoInseguro < FREIO_MIN_MS) {
    return;                               // freio segura o tempo minimo
  }
  if (c == estadoAtual) return;
  estadoAtual = c;
  mostraEstadoNoControle(c);
  Serial.print("TinyML | estado: ");
  Serial.println(NOME_CLASSE[c]);
  if (c == C_INSEGURO) {
    Serial.println("TinyML | acao: freio (aproximacao rapida)");
    vibraControle();
  }
}

Classe classeDoRotulo(const char* rotulo) {
  for (int c = C_SEGURO; c < C_INCERTO; c++) {
    if (strcmp(rotulo, NOME_CLASSE[c]) == 0) return (Classe)c;
  }
  return C_INCERTO;
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
  ultimaClasse = c;
  confiancaAtual = probs[melhor];

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

  // Confirmacao: a mesma classe em inferencias seguidas (frear e mais rapido que liberar)
  if (c == C_INCERTO) { repeticoes = 0; candidata = C_INCERTO; return; }
  if (c == candidata) repeticoes++;
  else { candidata = c; repeticoes = 1; }
  if (repeticoes < (c == C_INSEGURO ? CONFIRMA_INSEGURO : CONFIRMA_SEGURO)) return;

  mudaEstado(c);
}

// Chamado a cada leitura nova do sensor
void atualizaTinyML() {
  if (leiturasNaJanela < JANELA) return;   // janela ainda enchendo (logo apos ligar)

  perto = distanciaFiltrada() < PERTO_CM;
  if (!perto) {
    // Nada a menos de 50 cm: o modelo fica em espera (o freio ainda segura o tempo minimo)
    candidata = C_INCERTO;
    repeticoes = 0;
    leiturasDesdeInferencia = INFERENCIA_PASSO;   // ao chegar perto, infere na hora
    mudaEstado(C_LONGE);
    return;
  }
  if (++leiturasDesdeInferencia >= INFERENCIA_PASSO) {
    leiturasDesdeInferencia = 0;
    rodaInferencia();
  }
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
const CORES = { longe:'#4caf50', seguro:'#ffb300', inseguro:'#f44336' };
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
    el.innerText = d.estado + (d.freio ? ' - FREIO' : '');
    el.style.color = CORES[d.estado] || '#eee';
    document.getElementById('detalhe').innerText = d.perto
      ? 'modelo: ' + d.modelo + ' ' + Math.round(d.conf * 100) + '% | inferencia ' + d.ms + ' ms | ' + d.n + ' inferencias'
      : 'nada a menos de 50 cm: modelo em espera | ' + d.n + ' inferencias';
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
                ",\"l\":" + String(totalLeituras) +
                ",\"perto\":" + (perto ? "true" : "false") +
                ",\"estado\":\"" + NOME_CLASSE[estadoAtual] + "\"" +
                ",\"modelo\":\"" + NOME_CLASSE[ultimaClasse] + "\"" +
                ",\"conf\":" + String(confiancaAtual, 2) +
                ",\"ms\":" + String(tempoInferenciaMs) +
                ",\"n\":" + String(totalInferencias) +
                ",\"freio\":" + (freioAtivo() ? "true" : "false") +
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
  if (atualizaSensorDistancia()) atualizaTinyML();

  atualizaRampa();

  // Busca dados novos do controle (precisa ser chamado sempre no loop)
  BP32.update();
  bool controleOk = meuControle != nullptr && meuControle->isConnected() && meuControle->isGamepad();

  if (!controleOk) {
    alvoA = 0; alvoB = 0;
    return;
  }

  // Leitura crua dos eixos (-511 a 512).
  // axisY: empurrar pra CIMA da valor NEGATIVO, entao invertemos o sinal.
  int frente = -meuControle->axisY();   // stick esquerdo, eixo Y
  int giro   =  meuControle->axisRX();  // stick direito, eixo X
  if (abs(frente) < ZONA_MORTA) frente = 0;
  if (abs(giro)   < ZONA_MORTA) giro   = 0;

  int fr = map(frente, -511, 512, -velMax, velMax);
  int gr = map(giro,   -511, 512, -velMax, velMax);

  // TinyML: aproximacao rapida perto de algo -> freio (so o avanco; re e giro continuam)
  if (freioAtivo() && fr > 0) fr = 0;

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
