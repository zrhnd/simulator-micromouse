"""Perbandingan dua model *_best: Q-learning (continuous_qlearning) vs DQN (continuous_dqn).

Membaca episode_log_best.csv dan eval_result_best.json (serta eval_result_rintangan_best.json bila ada) dari kedua
controller, lalu menulis grafik perbandingan ke hasil_perbandingan_best/ (bernama berakhiran _best):
    01_success_rate_best          kurva success rate (rata-rata 50 episode) kedua algoritma dalam satu grafik
    02_keputusan_per_goal_best    keputusan per episode sukses (rata-rata bergerak 20 episode)
    03_jalur_evaluasi_best        jalur evaluasi greedy, Q-learning (kiri) dan DQN (kanan)
    04_jalur_peta_rintangan_best  jalur evaluasi di peta + rintangan tambahan, Q-learning (kiri) dan DQN (kanan)
    ringkasan_best.txt            angka-angka pembanding

Pakai: python analisis_best.py        (dari folder ini, di luar Webots)
"""
import json
import math
import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
QDIR = HERE.parent / "continuous_qlearning"
sys.path.insert(0, str(QDIR))
import analisis as qa                                    # noqa: E402

plt = qa.plt
ALGS = (("Q-learning", QDIR, "#d9822b"), ("DQN", HERE, "#3b6fd4"))
OUT = HERE / "hasil_perbandingan_best"


def path_len(p):
    return sum(math.dist(a, b) for a, b in zip(p, p[1:]))


def save(name, fig):
    OUT.mkdir(exist_ok=True)
    fig.tight_layout()
    fig.savefig(OUT / f"{name}.png", dpi=110)
    plt.close(fig)
    print("ditulis:", OUT / f"{name}.png")


def main():
    D = {}
    for name, d, col in ALGS:
        D[name] = {"log": qa.load_log(d / "episode_log_best.csv"),
                   "ev": json.loads((d / "eval_result_best.json").read_text(encoding="utf-8"))}
        r = d / "eval_result_rintangan_best.json"
        D[name]["rint"] = json.loads(r.read_text(encoding="utf-8")) if r.exists() else None

    # ---- 01 success rate
    fig, ax = plt.subplots(figsize=(9, 5))
    for name, _, col in ALGS:
        lg = D[name]["log"]
        ax.plot(lg["ep"], qa.moving((lg["out"] == "goal").astype(float), 50), color=col, lw=2, label=name)
        ax.plot(lg["ep"][-1], qa.moving((lg["out"] == "goal").astype(float), 50)[-1], "o", color=col, mec="k", ms=7)
    ax.axhline(0.95, color="gray", ls="--", lw=1)
    ax.set(xlabel="episode (titik = konvergen)", ylabel="success rate (rata-rata 50 episode)", ylim=(-0.02, 1.02),
           title="Success rate selama training")
    ax.grid(alpha=.3); ax.legend(loc="lower right")
    save("01_success_rate_best", fig)

    # ---- 02 keputusan per goal
    fig, ax = plt.subplots(figsize=(9, 5))
    for name, _, col in ALGS:
        lg = D[name]["log"]
        m = lg["out"] == "goal"
        ax.plot(lg["ep"][m], qa.moving(lg["dec"][m], 20), color=col, lw=2, label=name)
    ax.set(xlabel="episode", ylabel="keputusan per episode sukses (rata-rata bergerak 20)",
           title="Efisiensi episode sukses selama training")
    ax.grid(alpha=.3); ax.legend()
    save("02_keputusan_per_goal_best", fig)

    # ---- 03 jalur evaluasi, 04 peta rintangan
    for fname, key, title in (("03_jalur_evaluasi_best", "ev", "Jalur evaluasi greedy"),
                              ("04_jalur_peta_rintangan_best", "rint", "Jalur evaluasi di peta + rintangan tambahan")):
        if any(D[n][key] is None for n, _, _ in ALGS):
            continue
        fig, axes = plt.subplots(1, 2, figsize=(14, 7))
        for ax, (name, _, col) in zip(axes, ALGS):
            lay = D[name][key]
            qa.panel_paths(ax, lay, f"{name}: {title} ({lay['success']}/{lay['n']} sampai target)")
        save(fname, fig)

    # ---- ringkasan
    L = ["PERBANDINGAN MODEL BEST", ""]
    for name, _, _ in ALGS:
        lg, ev = D[name]["log"], D[name]["ev"]
        goal = (lg["out"] == "goal").astype(float)
        rate = qa.moving(goal, 50)
        conv = next((int(e) for e, r in zip(lg["ep"], rate) if r >= 0.95), None)
        eg = [e for e in ev["episodes"] if e["outcome"] == "goal"]
        L.append(f"{name}: episode {len(goal)}, konvergen {conv}, goal pertama {int(lg['ep'][np.argmax(goal > 0)])}, "
                 f"evaluasi {ev['success']}/{ev['n']}, keputusan {np.mean([e['decisions'] for e in eg]):.1f}, "
                 f"jalur {np.mean([path_len(e['path']) for e in eg]):.2f} m")
        r = D[name]["rint"]
        if r:
            L.append(f"   peta + rintangan: {r['success']}/{r['n']} sampai target")
    OUT.mkdir(exist_ok=True)
    (OUT / "ringkasan_best.txt").write_text("\n".join(L), encoding="utf-8")
    print("\n".join(L))


if __name__ == "__main__":
    main()
