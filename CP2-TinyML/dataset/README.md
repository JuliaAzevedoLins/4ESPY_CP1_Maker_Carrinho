# Dataset — HC-SR04 (simulado)

> ⚠️ **Estes dados são simulados.** Eles não foram medidos com o sensor do carrinho: foram gerados pelo script [`../simulacao/gerar_dataset.py`](../simulacao/gerar_dataset.py), com semente fixa (2026), e são reproduzíveis. O formato é idêntico ao que o firmware de coleta ([`../coleta/`](../coleta)) produz, então uma coleta real pode substituir estes arquivos sem nenhuma outra mudança no projeto.

| Item | Valor |
|---|---|
| Sensor simulado | HC-SR04 frontal do carrinho, alimentado em 3,3 V |
| Formato | CSV `timestamp,distancia` (ms, cm) |
| Período | 60 ms (16,67 Hz), o mesmo do firmware |
| Amostra | 3 s = 50 leituras |
| Sem eco | Gravado como 200 cm, o mesmo tratamento do firmware |
| Quantidade (v3) | 120 amostras por classe, **240 no total** (12 min de sinal) |
| Nome do arquivo | `<classe>.sim<NNN>.csv`: o Edge Impulse usa o texto antes do primeiro ponto como rótulo |

## Classes (v3)

O modelo responde a uma pergunta só: **a distância está diminuindo rápido demais?** O firmware só consulta o modelo quando há algo a menos de **50 cm**. Por isso toda amostra termina com o obstáculo perto, e o caso "nada à frente" não entra no dataset.

| Classe | Cenários simulados | Variações sorteadas por amostra |
|---|---|---|
| `seguro` | Obstáculo parado (28%), aproximação lenta (34%), afastando/ré (14%), carrinho girando (14%), mão passando (10%) | Aproximação de **1,5 a 12 cm/s**; ré de 5 a 50 cm/s; giro com 2 a 4 superfícies; mão de 8 a 45 cm |
| `inseguro` | Aproximação rápida: o carrinho indo rápido contra um obstáculo, ou algo vindo rápido contra ele | **25 a 90 cm/s**; fim de 5 a 35 cm; às vezes sai do repouso no meio da amostra ou entra no alcance já vindo rápido |

Entre 12 e 25 cm/s fica a fronteira e o modelo decide, em torno de 20 cm/s. A janela de 1,8 s inclui leituras de antes do obstáculo chegar aos 50 cm. Por isso, quando o carrinho entra na zona, o modelo já tem o histórico da aproximação.

## Modelo do sensor

Cada leitura passa por:

- **Ruído gaussiano** com desvio de `0,5 + 0,008 × d` cm, cerca de 0,9 cm a 50 cm;
- **Falha de eco** (vira 200 cm) com probabilidade `(0,8% + 0,02% × d) × fator da superfície`, cerca de 2% a 50 cm numa parede. O fator vai de 1 (parede) a 2 (mão e roupa);
- **Vibração** em 60% das amostras, simulando o carrinho andando: +0,3 a 0,8 cm de ruído e falhas × 1,5;
- **Eco espúrio** (multicaminho) em 0,3% das leituras: um valor aleatório entre 5 e 200 cm.

Os valores de ruído e de falha de eco vêm do log do primeiro teste no carrinho.

## Versões

| Versão | Classes | O que mudou | Por quê |
|---|---|---|---|
| v1 (commit `c7cbf68`) | vazio, parado, aproximando, aceno | 50 por classe; aproximações só rápidas; falhas de eco de 1% a 17% | Primeira versão do pipeline |
| v2 (commit `42be2e3`) | vazio, parado, aproximando, aceno | 80 por classe; aproximações lentas; ruído e falhas recalibrados | No primeiro teste no carrinho ([log](../evidencias/inferencias-testes-no-carrinho.csv)), o aceno foi confundido com aproximação e a aproximação lenta virou "parado" |
| **v3** | **seguro, inseguro** | 120 por classe; separação pela velocidade de aproximação; só situações a menos de 50 cm; vibração do carrinho andando | No teste da v2 ([log](../evidencias/inferencias-teste-v2-no-carrinho.csv)), quatro classes com fronteiras próximas (parado × aproximação lenta, aceno × aproximação) continuaram se confundindo. O grupo simplificou para uma decisão com ação clara: frear quando algo se aproxima rápido demais |

Para gerar de novo:

```bash
python CP2-TinyML/simulacao/gerar_dataset.py
```
