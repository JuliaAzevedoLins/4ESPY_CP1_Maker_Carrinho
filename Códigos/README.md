# Firmware — Carrinho Robótico WALL-E

Carrinho de tração diferencial controlado por um **controle PS5 DualSense** via Bluetooth, usando um **ESP32**.

| Arquivo | O que é |
|---|---|
| [`PS5_ESP32_Bluepad32.ino`](PS5_ESP32_Bluepad32.ino) | ⭐ **Firmware oficial** — o que roda no robô entregue |
| [`secrets.h.example`](secrets.h.example) | Modelo das credenciais de Wi-Fi (copiar para `secrets.h`) |
| [`testes-aula16/`](testes-aula16) | Códigos históricos dos testes de bancada em Arduino Uno |

## Recursos

- Conexão com o controle DualSense via [Bluepad32](https://github.com/ricardoquesada/bluepad32) (mais estável que as libs baseadas em spoof de MAC).
- Controle diferencial: stick esquerdo (frente/ré) + stick direito (giro).
- Rampa de aceleração suave e zona morta nos sticks.
- R2/L2 ajustam a velocidade máxima em tempo real.
- **Freio de segurança por sensor**: o HC-SR04 bloqueia o avanço a menos de 20 cm de um obstáculo, mantendo ré e giro liberados.
- **OTA**: depois da primeira gravação por cabo, novas versões do firmware podem ser enviadas pela rede Wi-Fi (`Tools > Port` no Arduino IDE mostra o carrinho como porta de rede).
- **Calibração pelo celular**: o ESP32 sobe uma página web simples (`http://<ip-do-esp32>/`) com sliders para compensar motores desbalanceados, salvos na memória interna (NVS).

## Hardware

- ESP32 Dev Module (precisa de Bluetooth clássico — não funciona em chips só-BLE como S2/S3/C3/C6)
- Driver de motor duplo (ponte H) — 4 pinos de controle: `IN1`, `IN2` (motor A), `IN3`, `IN4` (motor B)
- 2 motores DC TT com caixa de redução
- Sensor ultrassônico HC-SR04
- Controle PS5 DualSense
- 4 × pilhas AA

### Pinagem

| Sinal | GPIO | Canal LEDC |
|---|---|---|
| IN1 — motor A | 26 | 0 |
| IN2 — motor A | 27 | 1 |
| IN3 — motor B | 32 | 2 |
| IN4 — motor B | 33 | 3 |
| TRIG — HC-SR04 | 4 | — |
| ECHO — HC-SR04 | 18 | — |

> ⚠️ **Alimente o HC-SR04 em 3,3 V**, não em 5 V. Em 5 V o pino ECHO devolve 5 V e danifica o GPIO do ESP32, que tolera apenas 3,3 V. Em 3,3 V o sensor dispensa divisor resistivo.

> O motor B tem a polaridade invertida **em software** (`aplicaMotorB()`), porque os dois motores ficam montados espelhados no corpo. Trocar os fios do motor B na ponte H quebra esse pareamento.

## Configuração antes de compilar

1. Instale o pacote de placas do Bluepad32 (ver [instruções do projeto](https://github.com/ricardoquesada/bluepad32)).
2. No Arduino IDE, selecione a placa `ESP32 Bluepad32 Arduino / ESP32 + Bluepad32 Arduino > ESP32 Dev Module`.
3. Em `Tools > Partition Scheme`, escolha uma opção **com OTA** (não use nada com "No OTA" no nome).
4. Copie `secrets.h.example` para `secrets.h` e preencha com o nome e senha da sua rede wifi:
   ```cpp
   const char* WIFI_SSID = "sua-rede";
   const char* WIFI_PASS = "sua-senha";
   ```
   `secrets.h` está no `.gitignore` e nunca deve ser commitado.
5. Grave por cabo USB a primeira vez.

## Uso

- Ao ligar, o ESP32 conecta no wifi e imprime o IP no Serial Monitor (115200 baud).
- Coloque o DualSense em modo pareamento (segure PS + Create até o LED piscar rápido).
- Depois da primeira gravação, novas atualizações podem ser feitas por OTA (mesma rede wifi, sem cabo).
- Para calibrar os motores, acesse `http://<ip-do-esp32>/` pelo celular (mesma rede) e ajuste os sliders enquanto anda com o carrinho pra frente.

### Se o controle não conectar

Descomente `BP32.forgetBluetoothKeys();` no `setup()`, grave, ligue uma vez e comente de novo. Isso apaga pareamentos antigos guardados no ESP32.

## Explicação do funcionamento

O detalhamento da lógica do firmware — mistura diferencial, zona morta, rampa de aceleração, freio de segurança, calibração web e OTA — está na **seção 24** da [documentação técnica](../Documentacao.md).
