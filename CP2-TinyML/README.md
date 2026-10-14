# CP2 — TinyML aplicado ao Carrinho WALL-E

Evolução do carrinho do CP1: o mesmo robô, o mesmo sensor (HC-SR04) e o mesmo ESP32 agora rodam um **modelo de Machine Learning treinado no Edge Impulse**, e o resultado da classificação muda o comportamento do carrinho.

```
HC-SR04 → CSV → Edge Impulse → treino → biblioteca Arduino → ESP32 → inferência → ação no carrinho
```

> ⚠️ **O dataset deste CP2 é simulado.** Ele foi gerado por [`simulacao/gerar_dataset.py`](simulacao/gerar_dataset.py) e não medido com o sensor (ver [`dataset/README.md`](dataset/README.md)). As ferramentas de coleta real estão em [`coleta/`](coleta) e produzem o mesmo formato.

## Resultados

| Etapa | Resultado |
|---|---|
| Dataset | 168 amostras únicas de 3 s (136 treino / 32 teste), 680 janelas de treino |
| Validação | **91,2%** de acurácia (int8 = float32) |
| Teste | **85,0%** de acurácia: `vazio` 100%, `aproximando` 97,8%, `parado` 90%, `aceno` 67,3% |
| No ESP32 (estimativa do Edge Impulse) | 1 ms por inferência, 1,4 KB de RAM, 14,9 KB de flash |
| Firmware completo | Compila; 98% da memória de programa |
| Execução no carrinho | ⏳ Pendente (seção 33.2 da documentação) |

Detalhes, matrizes de confusão e prints: seções 29 a 33 da [documentação técnica](<../Documentacao.md>). Os prints estão em [`edge-impulse/prints/`](edge-impulse/prints).

## O que o modelo faz

O modelo não olha uma leitura isolada, e sim **como a distância varia** numa janela de **1,8 s** (30 leituras, uma a cada 60 ms). Com isso ele reconhece padrões que um simples `if (distancia < 20)` não distingue:

| Classe | O que o sensor vê | Ação no carrinho |
|---|---|---|
| `vazio` | Nada à frente até ~2 m | Luz do controle **verde**; velocidade normal |
| `parado` | Objeto fixo à frente (parede, caixa) | Luz **amarela**; avanço limitado a **50%** |
| `aproximando` | Algo vindo na direção do robô | Luz **vermelha**; avanço limitado a 50%. Com o robô parado, WALL-E **"se assusta"**: recua e o controle vibra |
| `aceno` | Mão acenando na frente do sensor | Luz **azul**. Com o robô parado, WALL-E **"cumprimenta"**: balança para a esquerda e para a direita |

- As manobras (`aproximando` e `aceno`) só disparam com o **robô parado** e os sticks soltos. Com o robô andando para a frente, a distância até uma parede também diminui — o mesmo padrão de "algo se aproximando". O piloto sempre tem prioridade: mexer no stick interrompe a manobra.
- O **freio de segurança do CP1** (bloqueio do avanço a menos de 20 cm) continua ativo, como camada determinística independente do modelo.
- A classe atual, a confiança e as probabilidades aparecem **ao vivo** no painel web do ESP32 (`http://<ip-do-esp32>/`) e no Serial Monitor.

## Organização da pasta

```
CP2-TinyML/
├── README.md                          # este arquivo
├── simulacao/gerar_dataset.py         # gerador do dataset simulado (semente fixa)
├── dataset/                           # 200 CSVs simulados + descrição
├── coleta/
│   ├── coleta_hcsr04/coleta_hcsr04.ino  # firmware de coleta (só sensor, motores desligados)
│   └── coletar.py                       # serial → CSV rotulado
├── edge-impulse/
│   ├── prints/                          # evidências do processo no Edge Impulse
│   └── biblioteca/                      # biblioteca Arduino exportada (.zip)
├── firmware/PS5_ESP32_TinyML/         # firmware do CP1 + modelo embarcado
└── evidencias/                        # fotos e vídeos da demonstração
```

O firmware do CP1 continua intacto em [`../Códigos/`](<../Códigos/README.md>).

## Como reproduzir

### 1. Dataset

**Simulado (o que foi usado):**

```bash
python simulacao/gerar_dataset.py   # 50 amostras por classe em dataset/, semente 2026
```

**Coleta real com o sensor (mesmo formato):**

1. Grave `coleta/coleta_hcsr04/coleta_hcsr04.ino` no ESP32 do carrinho (placa `ESP32 Bluepad32 Arduino > ESP32 Dev Module`). Ele só lê o sensor; os motores ficam desligados.
2. No PC:
   ```bash
   pip install pyserial
   python coleta/coletar.py --listar
   python coleta/coletar.py --porta COM5 --classe aceno
   ```
3. A cada Enter, o script grava uma amostra de **3 s** (50 leituras) em `dataset/<classe>.<NNN>.csv`, com as colunas `timestamp,distancia`, e mostra um gráfico em texto para conferir o gesto. `d` descarta a última amostra.

Leituras sem eco ou acima de 200 cm são gravadas como **200 cm**. O firmware final aplica exatamente o mesmo tratamento — o modelo aprende o formato que vai receber.

### 2. Edge Impulse

Projeto: **`carrinho_walle_tinyml`**.

1. *Data acquisition → Upload data*: os CSVs de `dataset/`, com **Automatically split** (80/20) e **Infer from filename** (o rótulo é o texto antes do primeiro ponto).
2. *Create impulse*: **Time series data** (janela 1800 ms, passo 300 ms, 16,67 Hz, zero-pad) → **Raw Data** (escala 0,01) → **Classification**.
3. *Raw data → Generate features* e *Classifier → Start training*.
4. *Model testing → Classify all*.
5. *Deployment → Arduino library* (quantizado int8).

### 3. Firmware

1. Arduino IDE: *Sketch → Include Library → Add .ZIP Library* com o zip de `edge-impulse/biblioteca/`.
2. Copie `firmware/PS5_ESP32_TinyML/secrets.h.example` para `secrets.h` e preencha o Wi-Fi.
3. Placa `ESP32 Bluepad32 Arduino > ESP32 Dev Module` (pacote **esp32_bluepad32 3.10.x**, o mesmo do CP1), partição padrão com OTA e **File → Preferences → Compiler warnings: Default**.
4. Grave por OTA (`Tools > Port > carrinho-esp32`) ou por cabo.

O firmware completo ocupa **98% da memória de programa** (1,29 MB de 1,31 MB; o do CP1 ocupava 90%). Se um modelo maior não couber, use *Tools → Partition Scheme → Minimal SPIFFS (1.9MB APP with OTA)* e grave **por cabo** uma vez, porque a troca de partição não pode ser feita por OTA.

> - A primeira compilação da biblioteca do Edge Impulse demora mais de 10 minutos. As seguintes usam cache.
> - Com *Compiler warnings: All*, o core do ESP32 trata como erro um aviso de dentro da SDK do Edge Impulse (`tflite_micro.h`, `unused-but-set-parameter`) e a compilação falha.
> - O Windows limita caminhos a 260 caracteres. Instale a biblioteca pelo *Add .ZIP Library* (vai para `Documentos\Arduino\libraries`) e não a extraia em pastas profundas.

## Evolução CP1 → CP2

| | CP1 | CP2 |
|---|---|---|
| Sensor | HC-SR04, leitura a cada 60 ms | O mesmo, em ritmo fixo e janela deslizante de 1,8 s |
| Decisão | Regra fixa: avanço bloqueado a menos de 20 cm | Regra de 20 cm mantida **+** modelo TinyML com 4 classes |
| Saída visível | Nenhuma além do movimento | Luz e vibração do DualSense, manobras e painel web com as probabilidades |
| Movimento sem comando | Nunca | Manobras curtas disparadas pelo modelo, só com o robô parado |
| Firmware | `Códigos/PS5_ESP32_Bluepad32.ino` | `CP2-TinyML/firmware/PS5_ESP32_TinyML/` |
