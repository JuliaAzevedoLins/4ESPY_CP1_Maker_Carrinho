# CP2 — TinyML aplicado ao Carrinho WALL-E

Evolução do carrinho do CP1: o mesmo robô, o mesmo sensor (HC-SR04) e o mesmo ESP32 agora rodam um **modelo de Machine Learning treinado no Edge Impulse**, e o resultado da classificação muda o comportamento do carrinho: **ele freia sozinho quando algo se aproxima rápido demais**.

```
HC-SR04 → CSV → Edge Impulse → treino → biblioteca Arduino → ESP32 → inferência → freio no carrinho
```

> ⚠️ **O dataset deste CP2 é simulado.** Ele foi gerado por [`simulacao/gerar_dataset.py`](simulacao/gerar_dataset.py) e não medido com o sensor (ver [`dataset/README.md`](dataset/README.md)). O ruído e as falhas de eco da simulação foram calibrados com o log do primeiro teste no carrinho. As ferramentas de coleta real estão em [`coleta/`](coleta) e produzem o mesmo formato.

## Resultados (modelo v3)

| Etapa | Resultado |
|---|---|
| Dataset | 288 amostras de 3 s (231 treino / 57 teste), 1155 janelas de treino |
| Validação | **96,5%** de acurácia (int8): `inseguro` 94,6%, `seguro` 97,8% |
| Teste | **94,04%** de acurácia: `inseguro` 98,4%, `seguro` 90,6% |
| No ESP32 (estimativa do Edge Impulse) | 1 ms por inferência, 1,4 KB de RAM, 16,4 KB de flash |
| Firmware completo | Compila; 98% da memória de programa |
| Execução no carrinho (v3) | ✅ 7 freadas a 21–32 cm/s (`inseguro` 82–96%), quase todas a 34–42 cm. Aproximações lentas, de 6–9 cm/s, sem freio. Logs em [`evidencias/`](evidencias) |
| Alimentação | Com pilhas gastas, o ESP32 reiniciava por brownout. Com pilhas novas, nenhum reinício (seção 32.6 da documentação) |
| Vídeo | 🎥 [`../vídeo/cp2-demonstracao-tinyml.mp4`](<../vídeo/cp2-demonstracao-tinyml.mp4>): painel web e luz do controle durante o freio |

Detalhes, matrizes de confusão, histórico das versões e prints estão nas seções 29 a 33 da [documentação técnica](<../Documentacao.md>). Os prints ficam em [`edge-impulse/prints/`](edge-impulse/prints), um subdiretório por versão.

## O que o modelo faz

O modelo não olha uma leitura isolada, e sim **como a distância varia** numa janela de **1,8 s** (30 leituras, uma a cada 60 ms). Ele responde a uma pergunta: **a distância está diminuindo rápido demais?** E só é consultado quando há algo a **menos de 50 cm**.

| Estado | Quando | Luz do controle | Ação no carrinho |
|---|---|---|---|
| `longe` | Nada a menos de 50 cm (modelo em espera) | **Verde** | Anda normalmente |
| `seguro` | Perto, com aproximação lenta, parado, ré ou giro | **Amarela** | Anda normalmente; é permitido chegar devagar |
| `inseguro` | Perto, com aproximação **rápida** (acima de cerca de 20 cm/s) | **Vermelha** + vibração | **Freia**: corta o avanço na hora, por pelo menos 1 s. Ré e giro continuam |

- Indo **rápido** contra uma parede, o carrinho para **sozinho, longe dela**. Indo **devagar**, ele chega até o freio de 20 cm do CP1, que continua ativo como camada determinística independente do modelo.
- Para frear bastam **3 inferências `inseguro` seguidas (180 ms)**; para liberar são precisas **5 `seguro` (300 ms)**. Perto de algo, o modelo roda a **cada leitura** (60 ms, cerca de 1 ms por inferência).
- O estado, a saída do modelo e as probabilidades aparecem **ao vivo** no painel web do ESP32 (`http://<ip-do-esp32>/`), em `GET /get` e no Serial Monitor. [`ferramentas/monitorar.py`](ferramentas/monitorar.py) grava tudo em CSV pela rede.

## Versões

| | v1 | v2 | **v3** |
|---|---|---|---|
| Classes | vazio, parado, aproximando, aceno | As mesmas 4 | **seguro, inseguro** |
| Teste no Edge Impulse | 85,0% | 87,3% | **94,04%** |
| No carrinho | Aceno × aproximação se confundiam | Manobras funcionaram, mas as fronteiras entre 4 classes seguiram instáveis | ✅ Freia nas aproximações rápidas e deixa chegar devagar |

Os testes no carrinho mostraram que as confusões estavam em fronteiras finas entre classes parecidas. Por isso a v3 trocou as quatro classes por uma decisão de segurança: frear quando algo se aproxima rápido demais, só perto de algo.

## Organização da pasta

```
CP2-TinyML/
├── README.md                          # este arquivo
├── simulacao/gerar_dataset.py         # gerador do dataset simulado (sementes fixas)
├── dataset/                           # 288 CSVs simulados (v3) + descrição
├── coleta/
│   ├── coleta_hcsr04/coleta_hcsr04.ino  # firmware de coleta (só sensor, motores desligados)
│   └── coletar.py                       # serial → CSV rotulado
├── edge-impulse/
│   ├── prints/v1, v2, v3/               # evidências do processo no Edge Impulse
│   └── biblioteca/                      # bibliotecas Arduino exportadas (.zip)
├── firmware/PS5_ESP32_TinyML/         # firmware do CP1 + modelo embarcado
├── ferramentas/monitorar.py           # monitor das inferências pela rede → CSV
└── evidencias/                        # logs dos testes no carrinho
```

O firmware do CP1 continua intacto em [`../Códigos/`](<../Códigos/README.md>).

## Como reproduzir

### 1. Dataset

**Simulado (o que foi usado):**

```bash
python simulacao/gerar_dataset.py   # 120 por classe + 48 de giro em dataset/, sementes 2026/2027
```

**Coleta real com o sensor (mesmo formato):**

1. Grave `coleta/coleta_hcsr04/coleta_hcsr04.ino` no ESP32 do carrinho (placa `ESP32 Bluepad32 Arduino > ESP32 Dev Module`). Ele só lê o sensor; os motores ficam desligados.
2. No PC:
   ```bash
   pip install pyserial
   python coleta/coletar.py --listar
   python coleta/coletar.py --porta COM5 --classe inseguro
   ```
3. A cada Enter, o script grava uma amostra de **3 s** (50 leituras) em `dataset/<classe>.<NNN>.csv`, com as colunas `timestamp,distancia`, e mostra um gráfico em texto para conferir o movimento. `d` descarta a última amostra.

Leituras sem eco ou acima de 200 cm são gravadas como **200 cm**. O firmware final aplica exatamente o mesmo tratamento, então o modelo aprende o formato que vai receber.

### 2. Edge Impulse

Projeto: **`carrinho_walle_tinyml_v3`**.

1. *Data acquisition → Upload data*: os CSVs de `dataset/`, com **Automatically split** (80/20) e **Infer from filename** (o rótulo é o texto antes do primeiro ponto).
2. *Create impulse*: **Time series data** (janela 1800 ms, passo 300 ms, 16,67 Hz, zero-pad) → **Raw Data** + **Flatten** (escala 0,01 nos dois) → **Classification**.
3. *Raw data* e *Flatten → Generate features*; *Classifier*: rede densa 40/20, 200 ciclos, taxa 0,005 → *Start training*.
4. *Model testing → Classify all*.
5. *Deployment → Arduino library* (quantizado int8).

### 3. Firmware

1. Arduino IDE: *Sketch → Include Library → Add .ZIP Library* com `edge-impulse/biblioteca/ei-carrinho_walle_tinyml_v3-arduino-1.0.1.zip`.
2. Copie `firmware/PS5_ESP32_TinyML/secrets.h.example` para `secrets.h` e preencha o Wi-Fi.
3. Placa `ESP32 Bluepad32 Arduino > ESP32 Dev Module` (pacote **esp32_bluepad32 3.10.x**, o mesmo do CP1), partição padrão com OTA e **File → Preferences → Compiler warnings: Default**.
4. Grave por OTA (`Tools > Port > carrinho-esp32`) ou por cabo. Se o firewall do Windows bloquear o `espota.exe` usado pelo IDE, grave com o script Python do mesmo pacote:
   ```bash
   python <Arduino15>/packages/esp32-bluepad32/hardware/esp32/3.10.2/tools/espota.py -r -i <ip-do-carrinho> -p 3232 --auth= -f PS5_ESP32_TinyML.ino.bin
   ```

O firmware completo ocupa **98% da memória de programa** (1,29 MB de 1,31 MB; o do CP1 ocupava 90%). Se um modelo maior não couber, use *Tools → Partition Scheme → Minimal SPIFFS (1.9MB APP with OTA)* e grave **por cabo** uma vez, porque a troca de partição não pode ser feita por OTA.

> - A primeira compilação da biblioteca do Edge Impulse demora mais de 10 minutos. As seguintes usam cache.
> - Com *Compiler warnings: All*, o core do ESP32 trata como erro um aviso de dentro da SDK do Edge Impulse (`tflite_micro.h`, `unused-but-set-parameter`) e a compilação falha.
> - O Windows limita caminhos a 260 caracteres. Instale a biblioteca pelo *Add .ZIP Library* (vai para `Documentos\Arduino\libraries`) e não a extraia em pastas profundas.

### 4. Registrar um teste

Com o carrinho ligado na mesma rede:

```bash
python ferramentas/monitorar.py --host <ip-do-carrinho> --saida evidencias/meu-teste.csv
```

## Evolução CP1 → CP2

| | CP1 | CP2 |
|---|---|---|
| Sensor | HC-SR04, leitura a cada 60 ms | O mesmo, em ritmo fixo e janela deslizante de 1,8 s |
| Decisão | Regra fixa: avanço bloqueado a menos de 20 cm | Regra de 20 cm mantida **+** modelo TinyML que freia diante de aproximação rápida, a menos de 50 cm |
| Saída visível | Nenhuma além do movimento | Freio, luz e vibração do DualSense, e painel web com as probabilidades |
| Firmware | `Códigos/PS5_ESP32_Bluepad32.ino` | `CP2-TinyML/firmware/PS5_ESP32_TinyML/` |
