"""
CP2 | TinyML -- monitor das inferencias do carrinho pela rede.

Le o painel do ESP32 (http://<ip>/get) a cada 300 ms, mostra a classe no
terminal e grava tudo em CSV -- registro de que o modelo esta rodando no
microcontrolador, sem precisar de cabo serial.

Uso:
    python monitorar.py                              # carrinho-esp32.local
    python monitorar.py --host 192.168.0.21 --saida ../evidencias/log.csv
Ctrl+C encerra.
"""

import argparse
import csv
import json
import time
import urllib.request
from datetime import datetime
from pathlib import Path

CLASSES = ["vazio", "parado", "aproximando", "aceno"]


def main() -> None:
    ap = argparse.ArgumentParser(description="Monitor das inferencias TinyML do carrinho")
    ap.add_argument("--host", default="carrinho-esp32.local")
    ap.add_argument("--intervalo", type=float, default=0.3, help="segundos entre leituras (padrao 0,3)")
    ap.add_argument("--saida", type=Path,
                    default=Path(__file__).resolve().parent.parent / "evidencias" /
                    f"inferencias-{datetime.now():%Y%m%d-%H%M%S}.csv")
    args = ap.parse_args()

    url = f"http://{args.host}/get"
    args.saida.parent.mkdir(parents=True, exist_ok=True)
    print(f"Lendo {url} -> {args.saida}  (Ctrl+C encerra)\n")

    ultima_n = None
    with args.saida.open("w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["hora", "inferencias", "distancia_cm", "classe", "confianca",
                    *[f"p_{c}" for c in CLASSES], "tempo_inferencia_ms", "manobra"])
        try:
            while True:
                try:
                    with urllib.request.urlopen(url, timeout=2) as r:
                        d = json.load(r)
                except OSError as e:
                    print(f"  sem resposta do carrinho ({e})")
                    time.sleep(1)
                    continue
                if d.get("n") != ultima_n:      # so grava quando houve inferencia nova
                    ultima_n = d.get("n")
                    probs = d.get("probs", {})
                    w.writerow([datetime.now().strftime("%H:%M:%S.%f")[:-3], d.get("n"), d.get("dist"),
                                d.get("classe"), d.get("conf"), *[probs.get(c) for c in CLASSES],
                                d.get("ms"), d.get("manobra")])
                    f.flush()
                    barras = "  ".join(f"{c} {probs.get(c, 0):4.2f}" for c in CLASSES)
                    print(f"#{d.get('n'):>6}  {d.get('dist'):6.1f} cm  {d.get('classe'):>11}  | {barras}"
                          f"{'  [manobra]' if d.get('manobra') else ''}")
                time.sleep(args.intervalo)
        except KeyboardInterrupt:
            print(f"\nLog salvo em {args.saida}")


if __name__ == "__main__":
    main()
