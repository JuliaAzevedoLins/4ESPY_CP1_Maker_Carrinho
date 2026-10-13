"""
CP2 | TinyML -- gerador do dataset SIMULADO do HC-SR04.

O dataset deste CP2 NAO foi medido com o sensor: ele e gerado por este
script, que simula as leituras do HC-SR04 do carrinho no mesmo formato que o
firmware de coleta (coleta/coleta_hcsr04) produziria:

    timestamp,distancia      (uma leitura a cada 60 ms, 50 leituras = 3 s)

O modelo do sensor reproduz o que a montagem tem de particular:
- alimentado em 3,3 V: ruido que cresce com a distancia e falhas de eco
  (leitura sem eco vira 200 cm, como no firmware);
- superficies macias ou inclinadas (mao, roupa) falham mais que parede;
- ecos espurios ocasionais (multicaminho).

Uso:
    python gerar_dataset.py                 # 50 amostras por classe, semente 2026
    python gerar_dataset.py --amostras 80 --semente 7

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
    p_falha = (0.01 + 0.0004 * d_real) * falha_extra
    if rng.random() < p_falha:                     # pulso sem eco
        return DIST_MAX_CM
    d = d_real + rng.gauss(0, 0.3 + 0.01 * d_real)
    return round(min(max(d, 2.0), DIST_MAX_CM), 1)


def vazio(rng: random.Random) -> list[float]:
    # Ambiente aberto, ou parede distante no limite do alcance
    fundo = None if rng.random() < 0.6 else rng.uniform(140, 260)
    return [leitura(rng, fundo, falha_extra=4.0) for _ in range(N_LEITURAS)]


def parado(rng: random.Random) -> list[float]:
    d0 = rng.uniform(10, 85)
    falha = rng.choice([1.0, 1.0, 2.0, 4.0])       # parede, caixa, mao, roupa
    oscila = rng.uniform(0, 1.5) if rng.random() < 0.3 else 0.0   # pessoa parada balanca
    fase = rng.uniform(0, 2 * math.pi)
    return [leitura(rng, d0 + oscila * math.sin(fase + k * 0.25), falha) for k in range(N_LEITURAS)]


def aproximando(rng: random.Random) -> list[float]:
    ini = rng.uniform(200, 240) if rng.random() < 0.25 else rng.uniform(70, 190)
    fim = rng.uniform(8, 30)
    k0 = rng.randint(0, 8)                          # demora para comecar (ate ~0,5 s)
    k1 = N_LEITURAS - 1 - rng.randint(0, 8)         # termina antes do fim da amostra
    perfil = rng.choice(["linear", "acelera", "freia"])
    falha = rng.choice([1.0, 2.0, 3.0])
    valores = []
    for k in range(N_LEITURAS):
        x = min(max((k - k0) / (k1 - k0), 0.0), 1.0)
        if perfil == "acelera":
            x = x * x
        elif perfil == "freia":
            x = 1 - (1 - x) * (1 - x)
        valores.append(leitura(rng, ini + (fim - ini) * x, falha))
    return valores


def aceno(rng: random.Random) -> list[float]:
    mao = rng.uniform(10, 45)
    fundo = None if rng.random() < 0.7 else rng.uniform(60, 180)
    periodo = rng.uniform(0.35, 0.9) * 1000 / INTERVALO_MS   # em leituras
    ciclo = rng.uniform(0.35, 0.65)                          # fracao do tempo com a mao na frente
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
    ap.add_argument("--amostras", type=int, default=50, help="amostras por classe (padrao 50)")
    ap.add_argument("--semente", type=int, default=2026, help="semente do gerador (padrao 2026)")
    ap.add_argument("--pasta", type=Path, default=PASTA_SAIDA)
    args = ap.parse_args()

    rng = random.Random(args.semente)
    args.pasta.mkdir(parents=True, exist_ok=True)
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
