"""
CP2 | TinyML -- gerador do dataset SIMULADO do HC-SR04 (v3).

O dataset deste CP2 NAO foi medido com o sensor: ele e gerado por este
script, que simula as leituras do HC-SR04 do carrinho no mesmo formato que o
firmware de coleta (coleta/coleta_hcsr04) produziria:

    timestamp,distancia      (uma leitura a cada 60 ms, 50 leituras = 3 s)

O modelo do sensor reproduz o que a montagem tem de particular:
- alimentado em 3,3 V: ruido que cresce com a distancia e falhas de eco
  (leitura sem eco vira 200 cm, como no firmware);
- superficies macias ou inclinadas (mao, roupa) falham mais que parede;
- ecos espurios ocasionais (multicaminho);
- com o carrinho andando, a vibracao aumenta o ruido e as falhas.

v3 -- duas classes, pela VELOCIDADE com que a distancia diminui:
- seguro:   obstaculo parado, aproximacao lenta (ate 12 cm/s), afastando
            (re), giro/aceno (a distancia muda aos saltos, sem aproximacao
            continua);
- inseguro: aproximacao rapida (25 a 90 cm/s) -- o carrinho indo rapido
            contra um obstaculo, ou algo vindo rapido contra ele.
Entre 12 e 25 cm/s fica a fronteira; o modelo decide (~20 cm/s).
O firmware so consulta o modelo com algo a menos de 50 cm (PERTO_CM), entao
toda amostra termina com o obstaculo perto; "nada a frente" nao entra.

v2 (commit 42be2e3) tinha 4 classes (vazio, parado, aproximando, aceno);
v1 (commit c7cbf68) era a primeira versao dessas 4 classes.

Uso:
    python gerar_dataset.py                 # 120 por classe + 48 de giro, sementes 2026/2027
    python gerar_dataset.py --amostras 80 --semente 7

Os arquivos saem em ../dataset/<classe>.sim<NNN>.csv (e seguro.giro<NNN>.csv,
o reforco). O Edge Impulse usa o texto antes do primeiro ponto como rotulo.
"""

import argparse
import csv
import math
import random
from pathlib import Path

INTERVALO_MS = 60
DT = INTERVALO_MS / 1000
N_LEITURAS = 50          # 3 s por amostra
DIST_MAX_CM = 200.0      # sem eco ou acima disso -> 200 (igual ao firmware)
PASTA_SAIDA = Path(__file__).resolve().parent.parent / "dataset"

PERTO_CM = 50.0          # o modelo so e consultado abaixo disso (igual ao firmware)
V_LENTO = (1.5, 12.0)    # cm/s -- seguro
V_RAPIDO = (25.0, 90.0)  # cm/s -- inseguro


def leitura(rng: random.Random, d_real: float | None, falha_extra: float = 1.0,
            ruido_extra: float = 0.0) -> float:
    """Uma leitura do HC-SR04 para um alvo a d_real cm (None = nada a frente)."""
    if rng.random() < 0.003:                       # eco espurio (multicaminho)
        return round(rng.uniform(5, DIST_MAX_CM), 1)
    if d_real is None or d_real >= DIST_MAX_CM:
        return DIST_MAX_CM
    p_falha = (0.008 + 0.0002 * d_real) * falha_extra   # ~2% a 50 cm (medido no carrinho)
    if rng.random() < p_falha:                     # pulso sem eco
        return DIST_MAX_CM
    d = d_real + rng.gauss(0, 0.5 + 0.008 * d_real + ruido_extra)   # ~0,9 cm a 50 cm (medido)
    return round(min(max(d, 2.0), DIST_MAX_CM), 1)


def vibracao(rng: random.Random) -> tuple[float, float]:
    """(falha_extra, ruido_extra) de uma amostra: superficie e, as vezes, o carrinho andando."""
    falha = rng.choice([1.0, 1.0, 1.5, 2.0])       # parede, caixa, mao, roupa
    if rng.random() < 0.6:                         # carrinho em movimento
        return falha * 1.5, rng.uniform(0.3, 0.8)
    return falha, 0.0


def trajeto(rng: random.Random, ini: float, v: float, k0: int, fim: float | None = None) -> list[float]:
    """Distancia real a cada leitura: parada ate k0 e depois variando a v cm/s
    (v > 0 aproxima, v < 0 afasta), com pequenas variacoes de velocidade."""
    d, pos = [], ini
    for k in range(N_LEITURAS):
        if k >= k0:
            pos -= v * DT * (1 + rng.gauss(0, 0.08))
            if fim is not None and v > 0:
                pos = max(pos, fim)
        d.append(pos)
    return d


# ---------------- seguro (sempre com algo perto no fim) ----------------

def parado(rng):
    d0 = rng.uniform(5, PERTO_CM)
    falha, ruido = vibracao(rng)
    oscila = rng.uniform(0, 1.5) if rng.random() < 0.3 else 0.0   # pessoa parada balanca
    fase = rng.uniform(0, 2 * math.pi)
    return [leitura(rng, d0 + oscila * math.sin(fase + k * 0.25), falha, ruido) for k in range(N_LEITURAS)]


def aproximando_devagar(rng):
    v = rng.uniform(*V_LENTO)
    fim = rng.uniform(5, PERTO_CM - 5)
    k0 = rng.randint(0, 8)
    ini = min(fim + v * (N_LEITURAS - k0) * DT * rng.uniform(0.9, 1.3), 195)
    falha, ruido = vibracao(rng)
    return [leitura(rng, d, falha, ruido) for d in trajeto(rng, ini, v, k0, fim)]


def afastando(rng):
    ini = rng.uniform(5, 40)                       # carrinho dando re, ou objeto se afastando
    k0 = rng.randint(0, 8)
    v = rng.uniform(5, 50)
    falha, ruido = vibracao(rng)
    return [leitura(rng, d, falha, ruido) for d in trajeto(rng, ini, -v, k0)]


def girando(rng):
    # Carrinho girando no lugar: o feixe passa por superficies diferentes e a
    # distancia muda aos saltos, sem uma aproximacao continua.
    niveis = [None if rng.random() < 0.3 else rng.uniform(10, 180) for _ in range(rng.randint(1, 3))]
    niveis.append(rng.uniform(10, PERTO_CM))       # termina virado para algo perto
    cortes = sorted(rng.sample(range(5, N_LEITURAS - 5), len(niveis) - 1)) + [N_LEITURAS]
    falha, ruido = 1.5, rng.uniform(0.3, 0.8)
    valores, i = [], 0
    for k in range(N_LEITURAS):
        if k >= cortes[i]:
            i += 1
        valores.append(leitura(rng, niveis[i], falha, ruido))
    return valores


def aceno(rng):
    # Mao passando na frente do sensor (era uma classe na v2; aqui e "seguro")
    mao = rng.uniform(8, PERTO_CM - 5)
    fundo = None if rng.random() < 0.5 else rng.uniform(mao + 15, 180)
    periodo = rng.uniform(0.3, 1.0) / DT
    ciclo = rng.uniform(0.3, 0.65)
    fase = rng.uniform(0, 1)
    valores = []
    for k in range(N_LEITURAS):
        pos = (fase + k / (periodo * rng.uniform(0.85, 1.15))) % 1.0
        borda = min(abs(pos - ciclo), pos, 1 - pos) < 0.06
        alvo = rng.choice([mao, fundo]) if borda and rng.random() < 0.5 else (mao if pos < ciclo else fundo)
        valores.append(leitura(rng, alvo, falha_extra=2.0))
    return valores


CENARIOS_SEGURO = [
    (parado, 0.28),
    (aproximando_devagar, 0.34),
    (afastando, 0.14),
    (girando, 0.14),
    (aceno, 0.10),
]


def seguro(rng):
    gerador = rng.choices([c for c, _ in CENARIOS_SEGURO], weights=[p for _, p in CENARIOS_SEGURO])[0]
    return gerador(rng)


# ---------------- inseguro ----------------

def inseguro(rng):
    # Aproximacao rapida durante a amostra inteira. As vezes o carrinho sai do
    # repouso (k0 leituras paradas) ou o obstaculo entra no alcance ja vindo
    # rapido; em toda janela de 1,8 s ha pelo menos ~0,8 s de aproximacao.
    v = rng.uniform(*V_RAPIDO)
    k0 = rng.randint(0, 10)
    t_mov = (N_LEITURAS - k0) * DT
    fim = rng.uniform(5, 35)
    ini_max = DIST_MAX_CM + v * DT * (16 - k0)     # a aproximacao aparece ate a leitura 16
    fim = max(5.0, min(fim, ini_max - v * t_mov))
    ini = fim + v * t_mov
    falha, ruido = vibracao(rng)
    return [leitura(rng, d, falha, ruido) for d in trajeto(rng, ini, v, k0, fim)]


GERADORES = {"seguro": seguro, "inseguro": inseguro}

# Reforco: no primeiro teste do modelo v3 no Edge Impulse, quase todos os
# erros foram giros com a distancia caindo em degraus (lidos como aproximacao).
# Amostras extras so desse cenario, com semente propria, para nao mudar as
# amostras ja enviadas.
SEMENTE_REFORCO = 2027


def grava(arquivo: Path, valores: list[float]) -> None:
    with arquivo.open("w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["timestamp", "distancia"])
        w.writerows((k * INTERVALO_MS, f"{d:.1f}") for k, d in enumerate(valores))


def main() -> None:
    ap = argparse.ArgumentParser(description="Gera o dataset simulado do HC-SR04")
    ap.add_argument("--amostras", type=int, default=120, help="amostras por classe (padrao 120)")
    ap.add_argument("--reforco", type=int, default=48, help="amostras extras de giro, rotulo seguro (padrao 48)")
    ap.add_argument("--semente", type=int, default=2026, help="semente do gerador (padrao 2026)")
    ap.add_argument("--pasta", type=Path, default=PASTA_SAIDA)
    args = ap.parse_args()

    rng = random.Random(args.semente)
    args.pasta.mkdir(parents=True, exist_ok=True)
    for antigo in args.pasta.glob("*.csv"):
        antigo.unlink()
    for classe, gerador in GERADORES.items():
        for i in range(1, args.amostras + 1):
            grava(args.pasta / f"{classe}.sim{i:03d}.csv", gerador(rng))
    rng = random.Random(SEMENTE_REFORCO)
    for i in range(1, args.reforco + 1):
        grava(args.pasta / f"seguro.giro{i:03d}.csv", girando(rng))
    print(f"{args.amostras * len(GERADORES) + args.reforco} amostras simuladas em {args.pasta} "
          f"({args.amostras} por classe + {args.reforco} de giro, sementes {args.semente}/{SEMENTE_REFORCO})")


if __name__ == "__main__":
    main()
