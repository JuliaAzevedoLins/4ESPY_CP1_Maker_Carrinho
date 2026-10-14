# Dataset — HC-SR04 (simulado)

> ⚠️ **Estes dados são simulados.** Eles não foram medidos com o sensor do carrinho: foram gerados pelo script [`../simulacao/gerar_dataset.py`](../simulacao/gerar_dataset.py), com semente fixa (2026), e são reproduzíveis. O formato é idêntico ao que o firmware de coleta ([`../coleta/`](../coleta)) produz, então uma coleta real pode substituir estes arquivos sem nenhuma outra mudança no projeto.

| Item | Valor |
|---|---|
| Sensor simulado | HC-SR04 frontal do carrinho, alimentado em 3,3 V |
| Formato | CSV `timestamp,distancia` (ms, cm) |
| Período | 60 ms (16,67 Hz), o mesmo do firmware |
| Amostra | 3 s = 50 leituras |
| Sem eco | Gravado como 200 cm, o mesmo tratamento do firmware |
| Quantidade (v2) | 80 amostras por classe, **320 no total** (16 min de sinal) |
| Nome do arquivo | `<classe>.sim<NNN>.csv`: o Edge Impulse usa o texto antes do primeiro ponto como rótulo |

## Classes (v2)

| Classe | Cenário simulado | Variações sorteadas por amostra |
|---|---|---|
| `vazio` | Nada à frente do sensor | Ambiente aberto (60%) ou parede distante, de 140 a 260 cm, no limite do alcance |
| `parado` | Objeto fixo à frente | Distância de 5 a 90 cm; superfície (parede, caixa, mão, roupa); 30% com leve balanço, como uma pessoa parada |
| `aproximando` | Algo vindo na direção do robô | **Metade lenta (4 a 15 cm/s), metade rápida (15 a 60 cm/s)**; fim de 5 a 30 cm; 20% entrando no alcance (a leitura aparece perto de 200 cm e desce suavemente) |
| `aceno` | Mão passando na frente do sensor | Mão de **8 a 70 cm**; fundo aberto ou parede atrás da mão; período de 0,3 a 1 s; tempo da mão no feixe; leituras ambíguas na borda do feixe |

## Modelo do sensor

Cada leitura passa por:

- **Ruído gaussiano** com desvio de `0,5 + 0,008 × d` cm, cerca de 0,9 cm a 50 cm;
- **Falha de eco** (vira 200 cm) com probabilidade `(0,8% + 0,02% × d) × fator da superfície`, cerca de 2% a 50 cm numa parede. O fator vai de 1 (parede) a 2 (mão e roupa), e chega a 8 no ambiente aberto;
- **Eco espúrio** (multicaminho) em 0,3% das leituras: um valor aleatório entre 5 e 200 cm.

## Versões

| Versão | O que mudou | Por quê |
|---|---|---|
| v1 (commit `c7cbf68`) | 50 por classe; aproximações só rápidas; falhas de eco de 1% a 17% | Primeira versão do pipeline |
| **v2** | 80 por classe; aproximações lentas; ruído e falhas recalibrados; entrada suave no alcance; mão até 70 cm | No primeiro teste no carrinho, o log ([`../evidencias/inferencias-testes-no-carrinho.csv`](../evidencias/inferencias-testes-no-carrinho.csv)) mostrou o aceno confundido com aproximação e a aproximação lenta classificada como "parado". O mesmo log mediu o ruído real (cerca de 0,9 cm) e as falhas de eco isoladas (cerca de 2%) |

Para gerar de novo:

```bash
python CP2-TinyML/simulacao/gerar_dataset.py
```
