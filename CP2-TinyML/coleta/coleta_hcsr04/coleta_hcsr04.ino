/*
  CP2 | TinyML -- firmware de COLETA do HC-SR04

  Le o sensor de distancia do carrinho (mesmos pinos e mesma logica do
  firmware do CP1) em ritmo fixo e envia cada leitura pela serial, em CSV:

      timestamp,distancia
      0,200.0
      60,200.0
      120,57.3
      ...

  O script ../coletar.py, no PC, grava essas linhas em arquivos CSV
  rotulados (ex.: dataset/aceno.001.csv) para importar no Edge Impulse.

  IMPORTANTE: leDistanciaModelo() precisa ser IDENTICA a do firmware com o
  modelo embarcado (firmware/PS5_ESP32_TinyML). O modelo aprende exatamente
  o que esta funcao entrega -- se mudar aqui, mude la e colete de novo.
*/

// ---- Pinos (iguais ao CP1) ----
#define PINO_TRIG 4
#define PINO_ECHO 18
const int PINOS_MOTOR[] = {26, 27, 32, 33};   // IN1..IN4 da ponte H

// ---- Leitura ----
const unsigned long INTERVALO_MS      = 60;       // 16,67 Hz -- mesmo periodo do CP1
const unsigned long SENSOR_TIMEOUT_US = 15000UL;  // ~2,5 m, evita travar sem eco
const float         DIST_MAX_CM       = 200.0f;   // sem eco ou acima disso vira 200

unsigned long t0 = 0;
unsigned long proximaLeitura = 0;

// Dispara o pulso e devolve a distancia em cm, ja no formato que o modelo usa:
// sem eco (nada a frente dentro do alcance) ou acima de 200 cm -> 200 cm.
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

void setup() {
  Serial.begin(115200);

  // Motores desligados durante a coleta (entradas da ponte H nao ficam flutuando)
  for (int p : PINOS_MOTOR) {
    pinMode(p, OUTPUT);
    digitalWrite(p, LOW);
  }

  pinMode(PINO_TRIG, OUTPUT);
  pinMode(PINO_ECHO, INPUT);
  digitalWrite(PINO_TRIG, LOW);

  delay(500);
  Serial.println("# Coleta HC-SR04 pronta: timestamp_ms,distancia_cm");

  t0 = millis();
  proximaLeitura = t0;
}

void loop() {
  if ((long)(millis() - proximaLeitura) < 0) return;

  // O timestamp e o instante agendado da leitura: sempre multiplo de 60 ms
  unsigned long instante = proximaLeitura - t0;
  proximaLeitura += INTERVALO_MS;

  float d = leDistanciaModelo();
  Serial.print(instante);
  Serial.print(',');
  Serial.println(d, 1);
}
