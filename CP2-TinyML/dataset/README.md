# Dataset — HC-SR04 (simulado)

> ⚠️ **Estes dados são simulados.** Eles não foram medidos com o sensor do carrinho: foram gerados pelo script [`../simulacao/gerar_dataset.py`](../simulacao/gerar_dataset.py), com semente fixa (2026), e são reproduzíveis. O formato é idêntico ao que o firmware de coleta ([`../coleta/`](../coleta)) produz, então uma coleta real pode substituir estes arquivos sem nenhuma outra mudança no projeto.

| Item | Valor |
|---|---|
| Sensor simulado | HC-SR04 frontal do carrinho, alimentado em 3,3 V |
| Formato | CSV `timestamp,distancia` (ms, cm) |
| Período | 60 ms (16,67 Hz), o mesmo do firmware |
| Amostra | 3 s = 50 leituras |
| Sem eco | Gravado como 200 cm, o mesmo tratamento do firmware |
| Quantidade | 50 amostras por classe, **200 no total** (10 min de sinal) |
| Nome do arquivo | `<classe>.sim<NNN>.csv`: o Edge Impulse usa o texto antes do primeiro ponto como rótulo |

## Classes

| Classe | Cenário simulado | Variações sorteadas por amostra |
|---|---|---|
| `vazio` | Nada à frente do sensor | Ambiente aberto (60%) ou parede distante, de 140 a 260 cm, no limite do alcance |
| `parado` | Objeto fixo à frente | Distância de 10 a 85 cm; superfície (parede, caixa, mão, roupa); 30% com leve balanço, como uma pessoa parada |
| `aproximando` | Algo vindo na direção do robô | Início de 70 a 190 cm ou fora do alcance (25%); fim de 8 a 30 cm; atraso no início e parada antes do fim; perfil linear, acelerando ou freando |
| `aceno` | Mão passando na frente do sensor | Mão de 10 a 45 cm; fundo aberto ou parede de 60 a 180 cm; período de 0,35 a 0,9 s; tempo da mão no feixe; leituras ambíguas quando a mão está na borda do feixe |

## Modelo do sensor

Cada leitura passa por:

- **Ruído gaussiano** com desvio de `0,3 + 0,01 × d` cm, maior quanto mais longe o alvo;
- **Falha de eco** (vira 200 cm) com probabilidade `(1% + 0,04% × d) × fator da superfície`. O fator vai de 1 (parede) a 4 (roupa e ambiente aberto) e representa superfícies macias ou inclinadas, que refletem pior;
- **Eco espúrio** (multicaminho) em 0,3% das leituras: um valor aleatório entre 5 e 200 cm.

Para gerar de novo:

```bash
python CP2-TinyML/simulacao/gerar_dataset.py
```
