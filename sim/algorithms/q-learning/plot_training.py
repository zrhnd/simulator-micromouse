#!/usr/bin/env python3
"""
Gambar grafik reward dan jumlah langkah per episode dari hasil training
main.py (dibaca dari episode_log.csv).

Cara pakai (dari folder ini, SETELAH training selesai/berhenti):
    python plot_training.py                  # pakai episode_log.csv di folder ini
    python plot_training.py path/lain.csv     # atau tunjuk file lain

Menghasilkan training_plot.png (2 grafik dalam 1 gambar) di folder yang sama
dengan CSV yang dibaca:
- Grafik 1: reward per episode, + garis rata-rata bergerak (moving average)
  supaya tren-nya kelihatan meski angka mentahnya naik-turun (noisy) karena
  epsilon-greedy -- lihat penjelasan soal ini di riwayat chat.
- Grafik 2: jumlah langkah per episode (titik oranye = episode yang gagal
  kehabisan langkah, titik biru = episode yang sampai goal), + garis
  "jalur terbaik" sejauh itu (best_len, cuma turun/mendatar, tidak pernah naik).
"""

import csv
import sys
from pathlib import Path

import matplotlib
matplotlib.use("Agg")  # simpan ke file, tidak perlu jendela GUI
import matplotlib.pyplot as plt


def load_log(path):
    episodes, steps, success, reward, epsilon, best_len = [], [], [], [], [], []
    with open(path, newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            episodes.append(int(row["episode"]))
            steps.append(int(row["steps"]))
            success.append(row["success"] == "1")
            reward.append(float(row["reward"]))
            epsilon.append(float(row["epsilon"]))
            best_len.append(int(row["best_len"]) if row["best_len"] else None)
    return episodes, steps, success, reward, epsilon, best_len


def moving_average(values, window=10):
    if len(values) < window:
        return [], []
    out_x, out_y = [], []
    for i in range(window - 1, len(values)):
        out_x.append(i + 1)  # episode number (1-indexed) at the end of the window
        out_y.append(sum(values[i - window + 1: i + 1]) / window)
    return out_x, out_y


def main():
    csv_path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).parent / "episode_log.csv"
    if not csv_path.exists():
        print(f"File tidak ditemukan: {csv_path}")
        print("Belum ada training yang menghasilkan episode_log.csv (atau tunjuk path lain sebagai argumen).")
        sys.exit(1)

    episodes, steps, success, reward, epsilon, best_len = load_log(csv_path)
    if not episodes:
        print(f"{csv_path} kosong -- tidak ada yang bisa digambar.")
        sys.exit(1)

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(10, 8), sharex=True)

    # --- Grafik 1: reward per episode ---
    ax1.plot(episodes, reward, color="tab:blue", alpha=0.35, linewidth=1, label="Reward per episode")
    ma_x, ma_y = moving_average(reward, window=10)
    if ma_x:
        ax1.plot(ma_x, ma_y, color="tab:red", linewidth=2, label="Rata-rata bergerak (10 episode)")
    ax1.axhline(0, color="gray", linewidth=0.8, linestyle="--")
    ax1.set_ylabel("Reward")
    ax1.set_title(f"Training Q-learning -- {csv_path.name}")
    ax1.legend(loc="lower right")
    ax1.grid(True, alpha=0.3)

    # --- Grafik 2: langkah per episode ---
    succ_x = [e for e, s in zip(episodes, success) if s]
    succ_y = [st for st, s in zip(steps, success) if s]
    fail_x = [e for e, s in zip(episodes, success) if not s]
    fail_y = [st for st, s in zip(steps, success) if not s]
    ax2.scatter(succ_x, succ_y, color="tab:blue", s=12, label="Sampai goal")
    ax2.scatter(fail_x, fail_y, color="tab:orange", s=12, label="Kehabisan langkah")

    best_x = [e for e, b in zip(episodes, best_len) if b is not None]
    best_y = [b for b in best_len if b is not None]
    if best_x:
        ax2.plot(best_x, best_y, color="tab:green", linewidth=2, label="Jalur terbaik sejauh ini")

    ax2.set_xlabel("Episode")
    ax2.set_ylabel("Jumlah langkah")
    ax2.legend(loc="upper right")
    ax2.grid(True, alpha=0.3)

    fig.tight_layout()
    out_path = csv_path.parent / "training_plot.png"
    fig.savefig(out_path, dpi=150)
    print(f"Grafik disimpan ke: {out_path}")
    print(f"Total episode: {len(episodes)}, berhasil: {sum(success)}, gagal: {len(episodes) - sum(success)}")
    if best_y:
        print(f"Jalur terbaik akhir: {best_y[-1]} langkah")


if __name__ == "__main__":
    main()
