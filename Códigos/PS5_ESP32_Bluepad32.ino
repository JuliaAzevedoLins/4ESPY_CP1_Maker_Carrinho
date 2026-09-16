#include <Bluepad32.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <WebServer.h>
#include <Preferences.h>
#include "secrets.h"   // define WIFI_SSID e WIFI_PASS aqui (nao vai pro git)

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

const int  DISTANCIA_MIN_CM    = 20;      // abaixo disso, bloqueia o avanco
const long SENSOR_INTERVALO    = 60;      // ms entre leituras
const unsigned long SENSOR_TIMEOUT_US = 15000UL; // ~2.5m, evita travar muito tempo sem eco

long distanciaCm = -1;   // -1 = sem leitura valida (fora de alcance ou nada na frente)
unsigned long ultimoSensor = 0;

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

// Dispara o pulso, mede o eco e atualiza distanciaCm (em cm).
// So roda a cada SENSOR_INTERVALO ms pra nao travar o loop toda hora com o pulseIn.
void atualizaSensorDistancia() {
  if (millis() - ultimoSensor < SENSOR_INTERVALO) return;
  ultimoSensor = millis();

  digitalWrite(PINO_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PINO_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PINO_TRIG, LOW);

  unsigned long duracao = pulseIn(PINO_ECHO, HIGH, SENSOR_TIMEOUT_US);
  if (duracao == 0) {
    distanciaCm = -1;  // sem eco: nada na frente dentro do alcance
  } else {
    distanciaCm = duracao / 58;  // formula padrao do HC-SR04
  }

  Serial.print("Distancia: ");
  if (distanciaCm < 0) Serial.println("sem leitura");
  else { Serial.print(distanciaCm); Serial.println(" cm"); }
}

// Chamado automaticamente quando um controle conecta
void onConnectedController(ControllerPtr ctl) {
  if (meuControle == nullptr) {
    Serial.println("Controle conectado!");
    meuControle = ctl;
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

// ---------------- Tela web de calibracao ----------------

const char PAGINA_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Calibracao do carrinho</title>
<style>
  body { font-family: sans-serif; background:#111; color:#eee; text-align:center; padding:20px; }
  h1 { font-size: 1.3em; }
  .bloco { margin: 30px 0; }
  input[type=range] { width: 90%; height: 30px; }
  .valor { font-size: 1.5em; margin: 8px 0; }
  .aviso { font-size: 0.85em; color:#aaa; margin-top:30px; }
</style>
</head>
<body>
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

  <div class="bloco">
    <div>Sensor de distancia (HC-SR04)</div>
    <div class="valor" id="valDist">--</div>
  </div>

  <p class="aviso">Os valores sao salvos automaticamente no ESP32.</p>

<script>
function carregar() {
  fetch('/get').then(r => r.json()).then(d => {
    document.getElementById('sliderA').value = Math.round(d.a * 100);
    document.getElementById('sliderB').value = Math.round(d.b * 100);
    document.getElementById('valA').innerText = Math.round(d.a * 100) + '%';
    document.getElementById('valB').innerText = Math.round(d.b * 100) + '%';
    document.getElementById('valDist').innerText = d.dist < 0 ? 'sem leitura' : d.dist + ' cm';
  });
}
setInterval(carregar, 500);
let tId = null;
function mudou(motor, val) {
  document.getElementById('val' + motor.toUpperCase()).innerText = val + '%';
  clearTimeout(tId);
  tId = setTimeout(() => enviar(), 150);
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
  String json = "{\"a\":" + String(trimA, 2) + ",\"b\":" + String(trimB, 2) + ",\"dist\":" + String(distanciaCm) + "}";
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

    // ---- Tela de calibracao ----
    prefs.begin("carrinho", false);
    trimA = prefs.getFloat("trimA", 1.0f);
    trimB = prefs.getFloat("trimB", 1.0f);
    server.on("/", handleRoot);
    server.on("/get", handleGet);
    server.on("/set", handleSet);
    server.begin();
    Serial.print("Calibracao: abra http://");
    Serial.print(WiFi.localIP());
    Serial.println(" no navegador do celular (mesmo wifi).");
  } else {
    Serial.println();
    Serial.println("Nao conectou no wifi. Seguindo sem OTA/calibracao web.");
  }

  Serial.println("Inicializando Bluepad32...");
  BP32.setup(&onConnectedController, &onDisconnectedController);

  // So descomente a linha abaixo, grave, teste, e comente de novo depois.
  // Ela apaga pareamentos antigos guardados no ESP32 -- util se o
  // controle ficar "preso" tentando reconectar em algo que nao existe mais.
  // BP32.forgetBluetoothKeys();

  Serial.println("Pronto. Coloque o controle em modo pareamento (PS + Create ate piscar rapido).");
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();

  atualizaRampa();
  atualizaSensorDistancia();  // roda mesmo sem controle conectado, bom pra testar so com o Serial Monitor

  // Busca dados novos do controle (precisa ser chamado sempre no loop)
  BP32.update();

  if (meuControle == nullptr || !meuControle->isConnected() || !meuControle->isGamepad()) {
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

  // Freio de seguranca: se tem objeto perto, bloqueia so o avanco (re e giro continuam liberados)
  bool objetoPerto = (distanciaCm >= 0 && distanciaCm < DISTANCIA_MIN_CM);
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
