# Carrinho Robótico WALL-E — 4ESPY | CP1

<img src="imagens/Wall-e.png" alt="WALL-E" width="180">

Carrinho robótico físico e funcional, construído sobre um corpo impresso em 3D inspirado no personagem WALL-E (Pixar). O robô se movimenta por **duas esteiras** tracionadas por motores DC independentes, é pilotado à distância por um **controle PS5 DualSense** via Bluetooth e detecta obstáculos com um **sensor ultrassônico HC-SR04**, que bloqueia automaticamente o avanço diante de uma colisão iminente.

- **Turma:** 4ESPY
- **Avaliação:** CP1
- **Alunos:**
  - Julia Azevedo Lins — RM 98690
  - Luis Gustavo Barreto Garrido — RM 99210
  - Victor Hugo Aranda Forte — RM 99667
  - Felipe Cortez — RM 99750
  - Guilherme Akio — RM 98582

## Objetivo

Desenvolver um carrinho robótico funcional que integre projeto mecânico, fabricação digital, eletrônica e programação — capaz de se deslocar para frente e para trás, fazer curvas, receber comandos sem fio e reagir a obstáculos detectados por sensor.

## Principais funcionalidades

| Funcionalidade | Como funciona |
|---|---|
| **Locomoção diferencial** | Dois motores DC independentes tracionam duas esteiras em TPU. Frente, ré, curvas e giro sobre o próprio eixo |
| **Controle sem fio** | Controle PS5 DualSense por Bluetooth clássico (biblioteca Bluepad32). Stick esquerdo: avanço/ré. Stick direito: giro |
| **Velocidade proporcional** | PWM de 1 kHz com rampa de aceleração suave. Os gatilhos **R2/L2** ajustam a velocidade máxima em tempo real |
| **Freio de segurança** | O HC-SR04 bloqueia o avanço a menos de 20 cm de um obstáculo — mantendo ré e giro liberados, para o robô nunca ficar preso |
| **Calibração pelo celular** | O ESP32 serve uma página web com sliders que compensam a diferença de rotação entre os motores. Valores salvos na memória interna |
| **Atualização OTA** | Depois da primeira gravação por cabo, o firmware é atualizado pela rede Wi-Fi — sem abrir o corpo impresso |

## O carrinho

> 📷 **A inserir:** foto do carrinho finalizado.

Enquanto isso, os renders das chapas de impressão dão a forma do robô:

| | | |
|---|---|---|
| ![Chapa 1](<Modelo 3d/renders/chapa-01.png>) | ![Chapa 2](<Modelo 3d/renders/chapa-02.png>) | ![Chapa 4](<Modelo 3d/renders/chapa-04.png>) |

## Especificações

| Item | Valor |
|---|---|
| Microcontrolador | ESP32 Dev Module (Bluetooth clássico) |
| Driver de motores | Ponte H dupla, 4 entradas em PWM |
| Motores | 2 × DC TT com caixa de redução |
| Tração | 2WD diferencial, esteiras em TPU 95A |
| Sensor | HC-SR04 frontal, detecção a 20 cm |
| Controle | PS5 DualSense (Bluepad32) |
| Alimentação | 4 × pilhas AA com chave liga/desliga |
| Estrutura | Corpo impresso em PLA — 204,4 × 126,0 × 77,7 mm |

## Como usar

1. Ligue a chave do suporte de pilhas.
2. Coloque o DualSense em pareamento: segure **PS + Create** até o LED piscar rápido.
3. Pilote: **stick esquerdo** anda e dá ré, **stick direito** gira, **R2/L2** ajustam a velocidade.
4. A menos de 20 cm de um obstáculo o avanço trava sozinho — use ré ou giro para sair.

Instruções completas (calibração, gravação do firmware e solução de problemas) na [seção 27 da documentação](Documentacao.md).

## Organização do repositório

```
├── README.md                     # este arquivo
├── Documentacao.md               # documentação técnica completa (28 seções)
│
├── Códigos/                      # firmware
│   ├── PS5_ESP32_Bluepad32.ino        # ⭐ firmware oficial do robô (ESP32)
│   ├── secrets.h.example              # modelo das credenciais de Wi-Fi
│   ├── README.md                      # setup, compilação e uso
│   └── testes-aula16/                 # códigos históricos dos testes de bancada
│
├── Modelo 3d/                    # projeto mecânico
│   ├── Wall-E_Articulated(3).3mf      # projeto do slicer (16 chapas)
│   ├── stl/                           # STL das peças modeladas pela equipe
│   ├── renders/                       # renders das 16 chapas de impressão
│   └── README.md                      # modelagem, adaptações e parâmetros
│
├── Organização/                  # planejamento e gestão
│   ├── MVP.html                       # produto mínimo viável e critérios de aceite
│   ├── MoSCoW.html                    # priorização Must/Should/Could/Won't
│   ├── Backlog.html                   # tarefas, responsáveis, prazos e critérios
│   ├── Dependências.html              # precedências e caminho crítico
│   ├── Kanban.html                    # situação das atividades
│   ├── Planilha-de-custos.xlsx        # custos (Excel)
│   ├── Resumo-Planilha-de-custos.html # resumo visual dos custos
│   └── README.md                      # índice dos entregáveis e equipe
│
├── croqui/                       # croqui da concepção inicial
├── imagens/                      # fotos da montagem e dos testes
└── vídeo/                        # vídeos dos testes
```

## Documentação

A [documentação técnica completa](Documentacao.md) é **cumulativa** e registra todo o processo:

| Seções | Conteúdo |
|---|---|
| 1 – 19 | Concepção, requisitos, especificações, chassi, carenagem e croqui |
| 20 – 21 | Montagem e testes de bancada (Arduino Uno) — registro histórico |
| 22 | Planejamento e gestão |
| **23** | **Projeto mecânico e fabricação** — adaptações, peças próprias, impressão |
| **24** | **Hardware, eletrônica e firmware** — pinagem, alimentação, funcionamento do código |
| **25** | **Testes, problemas e correções** — 10 testes e 12 problemas resolvidos |
| **26** | **Evolução e decisões de projeto** |
| **27** | **Instruções de uso** |
| **28** | **Evidências finais** |

Os artefatos de planejamento (MVP, MoSCoW, backlog, dependências, kanban e custos) estão na pasta [`Organização/`](<Organização/README.md>).

## Status do projeto

- [x] Ficha de requisitos, requisitos funcionais e físicos, especificações e lista de componentes
- [x] Arquitetura do sistema e distribuição dos componentes
- [x] Croqui do chassi (concepção)
- [x] Diagrama de blocos e de alimentação, validados em bancada
- [x] Testes de bancada: motores, sensor de aproximação e controle sem fio
- [x] Modelagem 3D: adaptação do corpo, extensor de eixo, roda boba e travessa interna
- [x] Impressão 3D de todas as peças (16 chapas, PLA + TPU)
- [x] Migração para ESP32 com controle PS5, calibração web e OTA
- [x] Integração final: mecânica + eletrônica + firmware
- [ ] Fotos do robô finalizado e vídeos da demonstração final
