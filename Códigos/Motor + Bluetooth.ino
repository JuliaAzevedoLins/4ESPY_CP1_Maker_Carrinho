/*
  Carrinho WALL-E — teste de bancada: Arduino Uno + L298N + HC-SR04 + HC-05 (Bluetooth clássico)

  Controle pelo app "Arduino Bluetooth Controller" (Giumig / com.giumig.apps.bluetoothserialmonitor)
  ou qualquer outro app que envie os caracteres F / B / L / R / S por Bluetooth clássico (SPP).

  IMPORTANTE: o app "BLE Controller" (que vocês baixaram antes) NÃO funciona com o HC-05 —
  o HC-05 comum usa Bluetooth clássico (SPP), não BLE. Por isso o app certo aqui é o
  "Arduino Bluetooth Controller" (funciona com HC-05/HC-06).

  Comandos recebidos via Bluetooth:
    F = frente   B = trás   L = esquerda   R = direita   S = parar (padrão quando solta o botão)

  A parada por obstáculo (HC-SR04) continua ativa: mesmo que o comando seja "F" (frente),
  se detectar algo perto, o carrinho para sozinho.
*/

#include <SoftwareSerial.h>

// HC-05: usamos SoftwareSerial em vez dos pinos 0/1, assim não precisa desconectar
// nada na hora de gravar o código pelo USB.
SoftwareSerial BT(11, 12); // pino 11 = RX do Arduino (liga no TXD do HC-05)
                            // pino 12 = TX do Arduino (liga no RXD do HC-05, via divisor resistivo)
                            // NOTA: pino 12 é normalmente IN4, mas desativado pra este teste

// ---------- Pinos do L298N (motores) ----------
const int IN1 = 4;
const int IN2 = 7;
const int IN3 = 8;
// const int IN4 = 12;  // DESATIVADO NESTE TESTE: pino 12 está sendo usado pelo HC-05 (TX)
const int ENA = 5; // PWM
const int ENB = 6; // PWM

// ---------- Pinos do HC-SR04 ----------
const int TRIG = 2;
const int ECHO = 3;

// ---------- Parâmetros ----------
const int VELOCIDADE = 200;
const float DISTANCIA_MINIMA_CM = 15.0;

char comando = 'S';

void setup() {
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  // pinMode(IN4, OUTPUT);  // desativado neste teste (pino 12 = HC-05 TX)
  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);

  Serial.begin(9600);  // Serial Monitor (USB) - continua livre, não conflita com o BT
  BT.begin(9600);       // baud padrão da maioria dos módulos HC-05 (confirme com AT+UART?)

  parar();
  Serial.println("Pronto. Aguardando comandos via Bluetooth (F/B/L/R/S)...");
}

void loop() {
  if (BT.available()) {
    comando = BT.read();
    Serial.print("Comando recebido: ");
    Serial.println(comando);
  }

  float distancia = lerDistanciaCM();
  bool obstaculo = (distancia > 0 && distancia < DISTANCIA_MINIMA_CM);

  switch (comando) {
    case 'F':
      if (obstaculo) {
        parar();
        Serial.println("Obstaculo detectado - avanco bloqueado");
      } else {
        frente();
      }
      break;
    case 'B':
      tras();
      break;
    case 'L':
      esquerda();
      break;
    case 'R':
      direita();
      break;
    case 'S':
    default:
      parar();
      break;
  }

  delay(50);
}

// =====================================================================
// Funções de movimento
// =====================================================================

void frente() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  // digitalWrite(IN4, LOW);  // desativado neste teste (pino 12 = HC-05 TX)
  analogWrite(ENA, VELOCIDADE);
  analogWrite(ENB, VELOCIDADE);
}

void tras() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  // digitalWrite(IN4, HIGH);  // desativado neste teste (pino 12 = HC-05 TX)
  analogWrite(ENA, VELOCIDADE);
  analogWrite(ENB, VELOCIDADE);
}

void esquerda() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH);
  // digitalWrite(IN4, LOW);  // desativado neste teste (pino 12 = HC-05 TX)
  analogWrite(ENA, VELOCIDADE);
  analogWrite(ENB, VELOCIDADE);
}

void direita() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  // digitalWrite(IN4, HIGH);  // desativado neste teste (pino 12 = HC-05 TX)
  analogWrite(ENA, VELOCIDADE);
  analogWrite(ENB, VELOCIDADE);
}

void parar() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  // digitalWrite(IN4, LOW);  // desativado neste teste (pino 12 = HC-05 TX)
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);
}

// =====================================================================
// Sensor ultrassônico HC-SR04
// =====================================================================

float lerDistanciaCM() {
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);

  long duracao = pulseIn(ECHO, HIGH, 30000);
  if (duracao == 0) {
    return -1;
  }
  return duracao * 0.0343 / 2.0;
}