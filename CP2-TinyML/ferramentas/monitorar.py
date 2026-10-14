"""
CP2 | TinyML -- monitor das inferencias do carrinho pela rede.

Le o painel do ESP32 (http://<ip>/get) a cada 150 ms, mostra o estado no
terminal e grava tudo em CSV -- registro de que o modelo esta rodando no
microcontrolador, sem precisar de cabo serial.

Firmware v3: uma linha por leitura nova do sensor, com a distancia, se ha
algo perto (< 1 m), o estado (longe / seguro / inseguro), a saida do modelo,
as probabilidades e se o freio esta ativo.

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


def main() -> None:
    ap = argparse.ArgumentParser(description="Monitor das inferencias TinyML do carrinho")
    ap.add_argument("--host", default="carrinho-esp32.local")
    ap.add_argument("--intervalo", type=float, default=0.15, help="segundos entre leituras (padrao 0,15)")
    ap.add_argument("--saida", type=Path,
                    default=Path(__file__).resolve().parent.parent / "evidencias" /
                    f"inferencias-{datetime.now():%Y%m%d-%H%M%S}.csv")
    args = ap.parse_args()

    url = f"http://{args.host}/get"
    args.saida.parent.mkdir(parents=True, exist_ok=True)
    print(f"Lendo {url} -> {args.saida}  (Ctrl+C encerra)\n")

    classes = None
    ultima = None
    with args.saida.open("w", newline="") as f:
        w = csv.writer(f)
        try:
            while True:
                try:
                    with urllib.request.urlopen(url, timeout=2) as r:
                        d = json.load(r)
                except OSError as e:
                    print(f"  sem resposta do carrinho ({e})")
                    time.sleep(1)
                    continue
                probs = d.get("probs", {})
                if classes is None:                 # rotulos vem do proprio modelo
                    classes = sorted(probs)
                    w.writerow(["hora", "leituras", "inferencias", "distancia_cm", "perto", "estado", "modelo",
                                "confianca", *[f"p_{c}" for c in classes], "tempo_inferencia_ms", "freio",
                                "motivo_ultimo_reinicio"])
                chave = (d.get("l"), d.get("n"))
                if chave != ultima:                 # so grava quando ha leitura ou inferencia nova
                    ultima = chave
                    w.writerow([datetime.now().strftime("%H:%M:%S.%f")[:-3], d.get("l"), d.get("n"), d.get("dist"),
                                d.get("perto"), d.get("estado"), d.get("modelo"), d.get("conf"),
                                *[probs.get(c) for c in classes], d.get("ms"), d.get("freio"), d.get("reset")])
                    f.flush()
                    barras = "  ".join(f"{c} {probs.get(c, 0):4.2f}" for c in classes)
                    print(f"#{d.get('n'):>6}  {d.get('dist'):6.1f} cm  {d.get('estado'):>8}  | {barras}"
                          f"{'  [FREIO]' if d.get('freio') else ''}")
                time.sleep(args.intervalo)
        except KeyboardInterrupt:
            print(f"\nLog salvo em {args.saida}")


if __name__ == "__main__":
    main()
