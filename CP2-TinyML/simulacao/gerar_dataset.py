"""
CP2 | TinyML -- gerador do dataset SIMULADO do HC-SR04 (v2).

O dataset deste CP2 NAO foi medido com o sensor: ele e gerado por este
script, que simula as leituras do HC-SR04 do carrinho no mesmo formato que o
firmware de coleta (coleta/coleta_hcsr04) produziria:

    timestamp,distancia      (uma leitura a cada 60 ms, 50 leituras = 3 s)

O modelo do sensor reproduz o que a montagem tem de particular:
- alimentado em 3,3 V: ruido que cresce com a distancia e falhas de eco
  (leitura sem eco vira 200 cm, como no firmware);
- superficies macias ou inclinadas (mao, roupa) falham mais que parede;
- ecos espurios ocasionais (multicaminho).

v2 -- recalibrado com o log do primeiro teste no carrinho
(evidencias/inferencias-testes-no-carrinho.csv):
- ruido medido de ~0,9 cm e ~2% de falhas de eco isoladas a menos de 1 m:
  a v1 exagerava as falhas na aproximacao (ate ~17%) e o modelo passou a
  confundir aproximacao com aceno;
- aproximacoes lentas (4 a 15 cm/s), que a v1 nao tinha e o modelo
  classificava como "parado";
- objeto que entra no alcance aparece suavemente perto de 200 cm, enquanto a
  mao do aceno aparece de uma vez, ja perto;
- mao do aceno de 8 a 70 cm, com fundo aberto ou parede atras.

Uso:
    python gerar_dataset.py                 # 80 amostras por classe, semente 2026
    python gerar_dataset.py --amostras 50 --semente 7

Os arquivos saem em ../dataset/<classe>.sim<NNN>.csv. O Edge Impulse usa o
texto antes do primeiro ponto como rotulo.
"""

import argparse
import csv
import math
import random
from pathlib import Path

INTERVALO_MS = 60
N_LEITURAS = 50          # 3 s por amostra
DIST_MAX_CM = 200.0      # sem eco ou acima disso -> 200 (igual ao firmware)
PASTA_SAIDA = Path(__file__).resolve().parent.parent / "dataset"


def leitura(rng: random.Random, d_real: float | None, falha_extra: float = 1.0) -> float:
    """Uma leitura do HC-SR04 para um alvo a d_real cm (None = nada a frente)."""
    if rng.random() < 0.003:                       # eco espurio (multicaminho)
        return round(rng.uniform(5, DIST_MAX_CM), 1)
    if d_real is None or d_real >= DIST_MAX_CM:
        return DIST_MAX_CM
    p_falha = (0.008 + 0.0002 * d_real) * falha_extra   # ~2% a 50 cm (medido no carrinho)
    if rng.random() < p_falha:                     # pulso sem eco
        return DIST_MAX_CM
    d = d_real + rng.gauss(0, 0.5 + 0.008 * d_real)     # ~0,9 cm a 50 cm (medido)
    return round(min(max(d, 2.0), DIST_MAX_CM), 1)


def vazio(rng: random.Random) -> list[float]:
    # Ambiente aberto, ou parede distante no limite do alcance
    fundo = None if rng.random() < 0.6 else rng.uniform(140, 260)
    return [leitura(rng, fundo, falha_extra=8.0) for _ in range(N_LEITURAS)]


def parado(rng: random.Random) -> list[float]:
    d0 = rng.uniform(5, 90)
    falha = rng.choice([1.0, 1.0, 1.5, 2.0])       # parede, caixa, mao, roupa
    oscila = rng.uniform(0, 1.5) if rng.random() < 0.3 else 0.0   # pessoa parada balanca
    fase = rng.uniform(0, 2 * math.pi)
    return [leitura(rng, d0 + oscila * math.sin(fase + k * 0.25), falha) for k in range(N_LEITURAS)]


def aproximando(rng: random.Random) -> list[float]:
    # Metade lenta (4-15 cm/s), metade rapida (15-60 cm/s)
    v = rng.uniform(4, 15) if rng.random() < 0.5 else rng.uniform(15, 60)
    t_total = N_LEITURAS * INTERVALO_MS / 1000
    fim = rng.uniform(5, 30)
    # Comeca onde a aproximacao ainda dura a amostra quase toda; as vezes fora do alcance
    ini = fim + v * t_total * rng.uniform(0.8, 1.3)
    if rng.random() < 0.2:
        ini = rng.uniform(200, 230)
    k0 = rng.randint(0, 5)                          # pequeno atraso para comecar
    falha = rng.choice([1.0, 1.0, 1.5])
    valores = []
    for k in range(N_LEITURAS):
        t = max(k - k0, 0) * INTERVALO_MS / 1000
        valores.append(leitura(rng, max(ini - v * t, fim), falha))
    return valores


def aceno(rng: random.Random) -> list[float]:
    mao = rng.uniform(8, 70)
    fundo = None if rng.random() < 0.5 else rng.uniform(mao + 15, 180)
    periodo = rng.uniform(0.3, 1.0) * 1000 / INTERVALO_MS   # em leituras
    ciclo = rng.uniform(0.3, 0.65)                          # fracao do tempo com a mao na frente
    fase = rng.uniform(0, 1)
    valores = []
    for k in range(N_LEITURAS):
        pos = (fase + k / (periodo * rng.uniform(0.85, 1.15))) % 1.0
        borda = min(abs(pos - ciclo), pos, 1 - pos) < 0.06   # mao entrando/saindo do feixe
        if borda and rng.random() < 0.5:
            alvo = rng.choice([mao, fundo])
        else:
            alvo = mao if pos < ciclo else fundo
        valores.append(leitura(rng, alvo, falha_extra=2.0))
    return valores


GERADORES = {"vazio": vazio, "parado": parado, "aproximando": aproximando, "aceno": aceno}


def main() -> None:
    ap = argparse.ArgumentParser(description="Gera o dataset simulado do HC-SR04")
    ap.add_argument("--amostras", type=int, default=80, help="amostras por classe (padrao 80)")
    ap.add_argument("--semente", type=int, default=2026, help="semente do gerador (padrao 2026)")
    ap.add_argument("--pasta", type=Path, default=PASTA_SAIDA)
    args = ap.parse_args()

    rng = random.Random(args.semente)
    args.pasta.mkdir(parents=True, exist_ok=True)
    for antigo in args.pasta.glob("*.sim*.csv"):
        antigo.unlink()
    for classe, gerador in GERADORES.items():
        for i in range(1, args.amostras + 1):
            arquivo = args.pasta / f"{classe}.sim{i:03d}.csv"
            with arquivo.open("w", newline="") as f:
                w = csv.writer(f)
                w.writerow(["timestamp", "distancia"])
                w.writerows((k * INTERVALO_MS, f"{d:.1f}") for k, d in enumerate(gerador(rng)))
    print(f"{args.amostras * len(GERADORES)} amostras simuladas em {args.pasta} "
          f"({args.amostras} por classe, semente {args.semente})")


if __name__ == "__main__":
    main()
