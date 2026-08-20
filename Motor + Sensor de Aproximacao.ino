/*
  Carrinho Robótico WALL-E — teste de motores + sensor (Arduino Uno + L298N)

  Baseado na montagem: Arduino Uno + módulo L298N + 2 motores DC + 4x pilhas AA
  (mesma montagem da imagem de referência), com o sensor HC-SR04 adicionado.

  Comportamento: o carrinho anda para frente continuamente e PARA automaticamente
  quando o HC-SR04 detecta um objeto mais perto que DISTANCIA_MINIMA_CM.

  Este código NÃO tem controle remoto (Bluetooth/app) — é só o teste de
  motores + sensor. Dá pra somar o controle remoto depois, em cima disso.
*/

// ---------- Pinos do L298N (motores) ----------
const int IN1 = 4;   // direção motor esquerdo
const int IN2 = 7;   // direção motor esquerdo
const int IN3 = 8;   // direção motor direito
const int IN4 = 12;  // direção motor direito
const int ENA = 5;   // velocidade motor esquerdo (PWM)
const int ENB = 6;   // velocidade motor direito (PWM)

// ---------- Pinos do sensor HC-SR04 ----------
const int TRIG = 2;
const int ECHO = 3;
// No Arduino Uno NÃO precisa de divisor resistivo no ECHO — a lógica do Uno já é 5V,
// igual ao HC-SR04 (isso só é necessário no ESP32, que trabalha em 3.3V).

// ---------- Parâmetros ajustáveis ----------
const int VELOCIDADE = 200;              // 0-255
const float DISTANCIA_MINIMA_CM = 15.0;  // ajustar depois de testar na prática

void setup() {
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);

  Serial.begin(9600);
  parar();
  Serial.println("Pronto. Andando para frente ate encontrar um obstaculo.");
}

void loop() {
  float distancia = lerDistanciaCM();

  Serial.print("Distancia: ");
  Serial.print(distancia);
  Serial.println(" cm");

  if (distancia > 0 && distancia < DISTANCIA_MINIMA_CM) {
    parar();
    Serial.println("Objeto detectado - motores parados");
  } else {
    frente();
  }

  delay(100); // pequena pausa entre leituras
}

// =====================================================================
// Funções de movimento
// =====================================================================

void frente() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  analogWrite(ENA, VELOCIDADE);
  analogWrite(ENB, VELOCIDADE);
}

void tras() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
  analogWrite(ENA, VELOCIDADE);
  analogWrite(ENB, VELOCIDADE);
}

void esquerda() {
  // giro no proprio eixo
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  analogWrite(ENA, VELOCIDADE);
  analogWrite(ENB, VELOCIDADE);
}

void direita() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
  analogWrite(ENA, VELOCIDADE);
  analogWrite(ENB, VELOCIDADE);
}

void parar() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
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

  long duracao = pulseIn(ECHO, HIGH, 30000); // timeout 30ms

  if (duracao == 0) {
    return -1; // sem leitura valida
  }

  return duracao * 0.0343 / 2.0;
}