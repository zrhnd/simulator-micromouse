#!/usr/bin/env python3
"""
Lihat isi qtable_state.pkl (hasil training main.py) tanpa perlu buka mms.

Cara pakai (dari folder ini):
    python view_qtable.py                  # pakai qtable_state.pkl di folder ini
    python view_qtable.py path/lain.pkl     # atau tunjuk file lain

Menampilkan:
- Ringkasan: ukuran maze, jumlah sel yang sudah dikenal dindingnya, jumlah
  pasangan (state, aksi) yang sudah punya nilai Q.
- Grid ASCII: untuk tiap sel yang sudah dikunjungi, panah (^ > v <) menunjuk
  arah dengan nilai Q tertinggi DI ANTARA arah yang diketahui terbuka (sama
  seperti aturan best_action() di main.py -- tidak asal pilih arah dengan Q
  tertinggi mentah, karena arah yang belum pernah dicoba defaultnya 0.0 dan
  bisa menyesatkan). '.' = sel belum pernah dikunjungi sama sekali.
- Ekspor qtable_export.csv (x,y,arah,nilai_q) dan qtable_export.json (semua
  data mentah: q-table, peta dinding, ukuran maze) di folder yang sama
  dengan file .pkl yang dibaca.
"""

import csv
import json
import pickle
import sys
from pathlib import Path

DIRS = ["N", "E", "S", "W"]
ARROW = {"N": "^", "E": ">", "S": "v", "W": "<"}


def load(path):
    with open(path, "rb") as f:
        return pickle.load(f)


def open_dirs_of(walls, x, y):
    cell = walls.get((x, y))
    if not cell:
        return []
    return [d for d in DIRS if cell.get(d) is False]


def compute_goal_cells(w, h):
    xs = [w // 2 - 1, w // 2] if w % 2 == 0 else [w // 2]
    ys = [h // 2 - 1, h // 2] if h % 2 == 0 else [h // 2]
    return {(gx, gy) for gx in xs for gy in ys}


def best_arrow(q, walls, x, y):
    open_dirs = open_dirs_of(walls, x, y)
    if not open_dirs:
        return None
    best_d, best_v = None, float("-inf")
    for d in open_dirs:
        v = q.get((x, y, d), 0.0)
        if v > best_v:
            best_v, best_d = v, d
    return best_d


def print_grid(q, walls, w, h):
    goal_cells = compute_goal_cells(w, h)
    print(f"\nGrid {w}x{h} (baris atas = y={h - 1} / Utara, kolom kiri = x=0):")
    print("(S = start, G = goal, panah = arah terbaik yang sudah dipelajari, . = belum dikunjungi)\n")
    for y in range(h - 1, -1, -1):
        row = []
        for x in range(w):
            if (x, y) == (0, 0):
                row.append("S")
            elif (x, y) in goal_cells:
                row.append("G")
            else:
                d = best_arrow(q, walls, x, y)
                row.append(ARROW[d] if d else ".")
        print(" ".join(row))
    print()


def export_csv(q, path):
    with open(path, "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(["x", "y", "arah", "nilai_q"])
        for (x, y, d), v in sorted(q.items()):
            writer.writerow([x, y, d, v])


def export_json(data, path):
    serializable = {
        "w": data.get("w"),
        "h": data.get("h"),
        "q": [{"x": x, "y": y, "dir": d, "value": v} for (x, y, d), v in data.get("q", {}).items()],
        "walls": [
            {"x": x, "y": y, **{d: v for d, v in cell.items()}}
            for (x, y), cell in data.get("walls", {}).items()
        ],
    }
    with open(path, "w", encoding="utf-8") as f:
        json.dump(serializable, f, indent=2, ensure_ascii=False)


def main():
    pkl_path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).parent / "qtable_state.pkl"
    if not pkl_path.exists():
        print(f"File tidak ditemukan: {pkl_path}")
        print("Belum ada training yang selesai (atau tunjuk path .pkl lain sebagai argumen).")
        sys.exit(1)

    data = load(pkl_path)
    q = data.get("q", {})
    walls = data.get("walls", {})
    w, h = data.get("w"), data.get("h")

    print(f"File: {pkl_path}")
    print(f"Ukuran maze: {w}x{h}")
    print(f"Sel yang sudah dikenal (punya info dinding): {len(walls)} dari {w * h if w and h else '?'}")
    print(f"Pasangan (sel, arah) yang punya nilai Q: {len(q)}")

    if q:
        best_state, best_val = max(q.items(), key=lambda kv: kv[1])
        worst_state, worst_val = min(q.items(), key=lambda kv: kv[1])
        print(f"Nilai Q tertinggi : {best_state} = {best_val:.2f}")
        print(f"Nilai Q terendah  : {worst_state} = {worst_val:.2f}")

    if w and h:
        print_grid(q, walls, w, h)

    out_dir = pkl_path.parent
    csv_path = out_dir / "qtable_export.csv"
    json_path = out_dir / "qtable_export.json"
    export_csv(q, csv_path)
    export_json(data, json_path)
    print(f"Diekspor ke: {csv_path}")
    print(f"Diekspor ke: {json_path}")


if __name__ == "__main__":
    main()
