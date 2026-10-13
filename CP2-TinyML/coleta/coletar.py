"""
CP2 | TinyML -- coleta do HC-SR04 pela serial e gravacao em CSV rotulado.

Grave antes o firmware coleta_hcsr04/coleta_hcsr04.ino no ESP32 do carrinho.

Uso:
    pip install pyserial
    python coletar.py --listar                     # mostra as portas seriais
    python coletar.py --porta COM5 --classe aceno  # coleta da classe "aceno"

Cada amostra tem 3 s (50 leituras, uma a cada 60 ms) e vira o arquivo
dataset/<classe>.<NNN>.csv, com as colunas timestamp,distancia.
O Edge Impulse usa o texto antes do primeiro ponto do nome como rotulo.
"""

import argparse
import csv
import statistics
import sys
from pathlib import Path

import serial
from serial.tools import list_ports

CLASSES = {
    "vazio": "Nada a frente do sensor ate ~2 m. Varie o lugar: corredor, mesa "
             "grande, apontando para longe.",
    "parado": "Um objeto PARADO de 10 a 80 cm (parede, caixa, livro, pe de "
              "cadeira, pessoa parada). Mude o objeto e a distancia a cada amostra.",
    "aproximando": "Comece com a mao/objeto LONGE (60 a 150 cm) e aproxime de "
                   "forma continua ate ~10 cm durante os 3 s inteiros. Varie a "
                   "velocidade e o objeto.",
    "aceno": "Acene a mao na frente do sensor (10 a 40 cm), passando pela frente "
             "dele e saindo, durante os 3 s inteiros. Varie o ritmo e a mao.",
}

INTERVALO_MS = 60
PASTA_DATASET = Path(__file__).resolve().parent.parent / "dataset"
BARRAS = " .:-=+*#%@"


def proximo_indice(pasta: Path, classe: str) -> int:
    usados = []
    for arq in pasta.glob(f"{classe}.*.csv"):
        meio = arq.name.split(".")[1]
        if meio.isdigit():
            usados.append(int(meio))
    return max(usados, default=0) + 1


def le_amostra(porta: serial.Serial, n: int) -> list[tuple[int, float]]:
    """Le n leituras consecutivas, com timestamp relativo ao inicio da amostra."""
    porta.reset_input_buffer()
    porta.readline()  # descarta uma eventual linha pela metade
    leituras = []
    while len(leituras) < n:
        bruto = porta.readline().decode("ascii", errors="ignore").strip()
        if not bruto:
            raise TimeoutError("o ESP32 parou de enviar dados (firmware de coleta gravado?)")
        if bruto.startswith("#"):
            continue
        partes = bruto.split(",")
        if len(partes) != 2:
            continue
        try:
            leituras.append((int(partes[0]), float(partes[1])))
        except ValueError:
            continue

    inicio = leituras[0][0]
    return [(t - inicio, d) for t, d in leituras]


def resumo(leituras: list[tuple[int, float]]) -> str:
    dist = [d for _, d in leituras]
    sem_eco = sum(1 for d in dist if d >= 200.0)
    grafico = "".join(BARRAS[min(int(d / 200.0 * (len(BARRAS) - 1)), len(BARRAS) - 1)] for d in dist)
    return (f"min {min(dist):5.1f}  max {max(dist):5.1f}  media {statistics.mean(dist):5.1f} cm"
            f"  sem eco {sem_eco}/{len(dist)}\n    tempo -> |{grafico}|  (espaco = perto, @ = longe)")


def verifica_ritmo(leituras: list[tuple[int, float]]) -> bool:
    return all(b[0] - a[0] == INTERVALO_MS for a, b in zip(leituras, leituras[1:]))


def main() -> None:
    ap = argparse.ArgumentParser(description="Coleta do HC-SR04 para o Edge Impulse")
    ap.add_argument("--porta", help="porta serial do ESP32 (ex.: COM5)")
    ap.add_argument("--classe", choices=CLASSES.keys())
    ap.add_argument("--amostras", type=int, default=40, help="quantas amostras gravar (padrao 40)")
    ap.add_argument("--duracao", type=float, default=3.0, help="segundos por amostra (padrao 3)")
    ap.add_argument("--pasta", type=Path, default=PASTA_DATASET)
    ap.add_argument("--listar", action="store_true", help="lista as portas seriais e sai")
    args = ap.parse_args()

    if args.listar:
        for p in list_ports.comports():
            print(f"{p.device}\t{p.description}")
        return
    if not args.porta or not args.classe:
        ap.error("informe --porta e --classe (ou use --listar)")

    n = round(args.duracao * 1000 / INTERVALO_MS)
    args.pasta.mkdir(parents=True, exist_ok=True)

    print(f"\nClasse: {args.classe}  ({n} leituras por amostra, {args.duracao:g} s)")
    print(f"Como fazer: {CLASSES[args.classe]}")
    print("Enter = grava amostra   d = descarta a ultima   q = sai\n")

    gravadas = []
    with serial.Serial(args.porta, 115200, timeout=2) as porta:
        while len(gravadas) < args.amostras:
            idx = proximo_indice(args.pasta, args.classe)
            cmd = input(f"[{args.classe}] {len(gravadas)}/{args.amostras} -> Enter grava a amostra {idx:03d}: ")
            cmd = cmd.strip().lower()
            if cmd == "q":
                break
            if cmd == "d":
                if gravadas:
                    ultima = gravadas.pop()
                    ultima.unlink()
                    print(f"    descartada {ultima.name}")
                else:
                    print("    nada para descartar")
                continue

            print("    GRAVANDO...", flush=True)
            leituras = le_amostra(porta, n)
            if not verifica_ritmo(leituras):
                print("    ! leituras fora do ritmo de 60 ms (linha perdida na serial). Grave de novo.")
                continue

            arquivo = args.pasta / f"{args.classe}.{idx:03d}.csv"
            with arquivo.open("w", newline="") as f:
                w = csv.writer(f)
                w.writerow(["timestamp", "distancia"])
                w.writerows((t, f"{d:.1f}") for t, d in leituras)
            gravadas.append(arquivo)
            print(f"    salvo {arquivo.name}  {resumo(leituras)}")

    total = len(list(args.pasta.glob(f"{args.classe}.*.csv")))
    print(f"\n{len(gravadas)} amostras gravadas nesta sessao; {total} no total para '{args.classe}'.")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        sys.exit("\ninterrompido")
