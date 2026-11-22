"""Perbandingan beberapa seed: Q-learning vs DQN (continuous_qlearning vs continuous_dqn).

Membaca subfolder per run bernama <algoritma>_s<seed> (keluaran controller dengan MM_OUT):
    continuous_qlearning/seed/<kumpulan>/qlearning_s7 ... qlearning_s9
    continuous_dqn/seed/<kumpulan>/dqn_s7 ... dqn_s9
Tiap subfolder memuat episode_log.csv dan eval_result.json. <kumpulan> = seleksi (pemilihan model lewat
evaluasi greedy, early stopping) atau tanpa_es (3000 episode penuh, tanpa early stopping).

    python analisis_seed.py                       # kumpulan seleksi, keluaran continuous_dqn/seed/hasil_perbandingan/
    python analisis_seed.py --set tanpa_es        # kumpulan tanpa_es, keluaran .../hasil_perbandingan_tanpa_es/
    python analisis_seed.py <folder> [<folder>..] # folder lain berisi subfolder <algoritma>_s<seed>
    python analisis_seed.py --outdir <folder>     # folder keluaran lain
    python analisis_seed.py --best                # nama keluaran berakhiran _best
    python analisis_seed.py --per-seed            # tambah: analisis.py detail (grafik 01-14) untuk SETIAP run,
                                                  # keluaran <folder_run>/hasil_analisis/ (--gif: tambah animasi)

Keluaran: grafik 01-10 (png; 08-10 hanya bila ada seed/evaluasi: evaluasi dua peta; 06 jalur evaluasi semua seed + jalur terpendek, 07 heatmap tabrakan gabungan), ringkasan_seed.csv dan ringkasan_seed.txt.
Label grafik: "seed 1..N" (urut menurut nilai seed); nilai seed asli (7, 8, ...) ada di kolom "seed" pada tabel ringkasan.
Konvergen = episode pertama saat success rate WINDOW episode terakhir >= --target (bawaan 0,95).
"""
import argparse
import csv
import json
import math
import re
import subprocess
import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "continuous_qlearning"))
import analisis as qa                                    # noqa: E402  (load_log, moving, plt)

plt = qa.plt
ALGS = (("qlearning", "Q-learning", "#d9822b"), ("dqn", "DQN", "#3b6fd4"))
OUTS = ("goal", "collision", "stuck", "timeout")
OUT_NAME = {"goal": "goal", "collision": "tabrakan", "stuck": "macet", "timeout": "timeout"}


def find_runs(roots):
    runs = {}
    for root in roots:
        for d in sorted(Path(root).iterdir()):
            m = re.fullmatch(r"(qlearning|dqn)_s(\d+)", d.name)
            if m and (d / "episode_log.csv").exists() and (d / "eval_result.json").exists():
                runs.setdefault(m.group(1), {})[int(m.group(2))] = d
    return runs


def path_len(p):
    return sum(math.dist(a, b) for a, b in zip(p, p[1:]))


def summarize(d, window, target):
    log = qa.load_log(d / "episode_log.csv")
    goal = (log["out"] == "goal").astype(float)
    rate = qa.moving(goal, window)
    full = np.arange(len(goal)) + 1 >= window
    conv = next((int(e) for e, r, f in zip(log["ep"], rate, full) if f and r >= target), None)
    first = next((int(e) for e, g in zip(log["ep"], goal) if g), None)
    ev = json.loads((d / "eval_result.json").read_text(encoding="utf-8"))
    eps = ev["episodes"]
    g = [e for e in eps if e["outcome"] == "goal"]
    n = len(log["out"])
    return {
        "log": log, "eps": eps, "lay": ev, "rate": rate, "n_ep": n, "first_goal": first, "conv": conv,
        "eval_ok": ev["success"], "eval_n": ev["n"],
        "dec": [e["decisions"] for e in g], "path": [path_len(e["path"]) for e in g if e.get("path")],
        "frac": {o: float((log["out"] == o).mean()) for o in OUTS},
        "cnt": {o: int((log["out"] == o).sum()) for o in OUTS},
        "haz": float(log["haz"].mean()),
        "model_ep": (ev.get("train") or {}).get("model_episode"),
        "wall": (ev.get("train") or {}).get("wall_seconds"),
    }


def mean_sd(x):
    x = [v for v in x if v is not None]
    return (float(np.mean(x)), float(np.std(x))) if x else (float("nan"), float("nan"))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("folder", nargs="*", help="folder berisi subfolder <algoritma>_s<seed> (bawaan: seed/<--set> di kedua controller)")
    ap.add_argument("--set", default="seleksi", help="kumpulan run di continuous_*/seed/ (seleksi atau tanpa_es)")
    ap.add_argument("--outdir", default=None)
    ap.add_argument("--best", action="store_true", help="nama keluaran berakhiran _best")
    ap.add_argument("--per-seed", action="store_true", help="jalankan analisis.py detail untuk setiap run")
    ap.add_argument("--gif", action="store_true", help="dengan --per-seed: tambah animasi jalur evaluasi")
    ap.add_argument("--window", type=int, default=50)
    ap.add_argument("--target", type=float, default=0.95, help="success rate untuk konvergen")
    a = ap.parse_args()
    if a.folder:
        roots = [Path(f) for f in a.folder]
        default_out = roots[0] / "hasil_perbandingan"
    else:
        roots = [HERE.parent / "continuous_qlearning" / "seed" / a.set, HERE / "seed" / a.set]
        default_out = HERE / "seed" / ("hasil_perbandingan" if a.set == "seleksi" else f"hasil_perbandingan_{a.set}")
    root = ", ".join(str(r) for r in roots)
    outdir = Path(a.outdir) if a.outdir else default_out
    outdir.mkdir(parents=True, exist_ok=True)
    sfx = "_best" if a.best else ""
    runs = find_runs(roots)
    if not runs:
        raise SystemExit(f"tidak ada subfolder <algoritma>_s<seed> lengkap di {root}")
    S = {alg: {s: summarize(d, a.window, a.target) for s, d in sorted(r.items())} for alg, r in runs.items()}
    present = [x for x in ALGS if x[0] in S]
    seeds = sorted({s for alg in S for s in S[alg]})
    LABEL = {s: i + 1 for i, s in enumerate(seeds)}      # label grafik: seed 1..N (nilai seed asli tetap di tabel)

    if a.per_seed:
        scripts = {"qlearning": HERE.parent / "continuous_qlearning" / "analisis.py", "dqn": HERE / "analisis.py"}
        for alg, rr in runs.items():
            for s, d in sorted(rr.items()):
                cmd = [sys.executable, str(scripts[alg]), "--csv", str(d / "episode_log.csv"),
                       "--eval", str(d / "eval_result.json"), "--outdir", str(d / "hasil_analisis")]
                rint = (HERE.parent / ("continuous_qlearning" if alg == "qlearning" else "continuous_dqn") / "seed" / "evaluasi"
                        / "rintangan" / f"{alg}_s{s}" / "eval_result.json")
                if rint.exists():
                    cmd += ["--rintangan", str(rint)]
                if a.best:
                    cmd.append("--best")
                if a.gif:
                    cmd.append("--gif")
                (d / "hasil_analisis").mkdir(exist_ok=True)
                print(f"== analisis per seed: {alg} seed {s}")
                r = subprocess.run(cmd, capture_output=True, text=True)
                if r.returncode != 0:
                    print("  GAGAL:", (r.stderr or r.stdout).strip().splitlines()[-1:])

    # ---- 01 kurva success rate
    def g01(ax):
        for alg, name, col in present:
            for k, (s, v) in enumerate(S[alg].items()):
                ax.plot(np.arange(1, len(v["rate"]) + 1), v["rate"], color=col, lw=1.6, alpha=0.8,
                        label=name if k == 0 else None)
                ax.plot(len(v["rate"]), v["rate"][-1], "o", color=col, ms=6, mec="k")
                ax.annotate(f"s{LABEL[s]}", (len(v["rate"]), v["rate"][-1]), xytext=(4, -10), textcoords="offset points",
                            fontsize=8, color=col)
        ax.axhline(a.target, color="gray", ls="--", lw=1)
        ax.set_xlabel("episode (titik = akhir training / konvergen)")
        ax.set_ylabel(f"success rate (rata-rata {a.window} episode)")
        ax.set_ylim(-0.02, 1.02); ax.grid(alpha=.3); ax.legend(loc="lower right", fontsize=9)
        ax.set_title("Kurva success rate per seed")
    qa.save_one(outdir, "01_kurva_success_rate_seed", sfx, (9, 5), g01)

    # ---- helper batang per seed
    def bars(ax, key, ylabel, title, fmt="%.0f"):
        w = 0.8 / len(present)
        for i, (alg, name, col) in enumerate(present):
            xs, ys = [], []
            for j, s in enumerate(seeds):
                v = S[alg].get(s)
                xs.append(j + (i - (len(present) - 1) / 2) * w)
                ys.append(np.nan if v is None or v[key] is None else v[key])
            ax.bar(xs, ys, w * 0.92, color=col, label=name)
            for x, y in zip(xs, ys):
                if not np.isnan(y):
                    ax.text(x, y, fmt % y, ha="center", va="bottom", fontsize=8)
            mu = np.nanmean(ys)
            ax.axhline(mu, color=col, ls="--", lw=1)
        ax.set_xticks(range(len(seeds))); ax.set_xticklabels([f"seed {LABEL[s]}" for s in seeds])
        ax.set_ylabel(ylabel); ax.set_title(title); ax.legend(fontsize=8); ax.grid(axis="y", alpha=.3)

    qa.save_one(outdir, "02_episode_konvergen_seed", sfx, (8, 5),
                lambda ax: bars(ax, "conv", f"episode (rate {a.window}-episode >= {a.target})",
                                "Episode konvergen per seed (garis putus = rata-rata)"))

    # ---- 03/04 keputusan & jalur evaluasi (rata-rata +- simpangan antar uji goal)
    def bars_err(ax, key, ylabel, title, fmt):
        w = 0.8 / len(present)
        for i, (alg, name, col) in enumerate(present):
            for j, s in enumerate(seeds):
                v = S[alg].get(s)
                if v is None or not v[key]:
                    continue
                x = j + (i - (len(present) - 1) / 2) * w
                mu, sd = float(np.mean(v[key])), float(np.std(v[key]))
                ax.bar(x, mu, w * 0.92, color=col, label=name if j == 0 else None)
                ax.errorbar(x, mu, yerr=sd, color="k", capsize=3, lw=1)
                ax.text(x, mu + sd, fmt % mu, ha="center", va="bottom", fontsize=8)
        ax.set_xticks(range(len(seeds))); ax.set_xticklabels([f"seed {LABEL[s]}" for s in seeds])
        ax.set_ylabel(ylabel); ax.set_title(title); ax.legend(fontsize=8); ax.grid(axis="y", alpha=.3)

    qa.save_one(outdir, "03_keputusan_evaluasi_seed", sfx, (8, 5),
                lambda ax: bars_err(ax, "dec", "keputusan per goal", "Evaluasi greedy: keputusan per goal (batang = rata-rata, garis = simpangan)", "%.0f"))
    qa.save_one(outdir, "04_jalur_evaluasi_seed", sfx, (8, 5),
                lambda ax: bars_err(ax, "path", "panjang jalur (m)", "Evaluasi greedy: panjang jalur per goal", "%.2f"))

    # ---- 05 hasil episode training (porsi, sampai konvergen/berhenti)
    def g05(ax):
        w = 0.8 / len(present)
        for i, (alg, name, col) in enumerate(present):
            for j, s in enumerate(seeds):
                v = S[alg].get(s)
                if v is None:
                    continue
                x = j + (i - (len(present) - 1) / 2) * w
                bot = 0.0
                for o in OUTS:
                    f = v["frac"][o] * 100
                    ax.bar(x, f, w * 0.92, bottom=bot, color=qa.COL[o], alpha=0.95 if alg == "dqn" else 0.6,
                           edgecolor=col, linewidth=1.4, label=OUT_NAME[o] if (i == 0 and j == 0) else None)
                    bot += f
                ax.text(x, 101, name[:1] if alg == "dqn" else "Q", ha="center", fontsize=8, color=col)
        ax.set_xticks(range(len(seeds))); ax.set_xticklabels([f"seed {LABEL[s]}" for s in seeds])
        ax.set_ylabel("% episode training"); ax.set_ylim(0, 108)
        ax.set_title("Hasil episode training per seed (kiri = Q-learning, kanan = DQN)")
        ax.legend(fontsize=8, loc="upper left", bbox_to_anchor=(1.01, 1)); ax.grid(axis="y", alpha=.3)
    qa.save_one(outdir, "05_hasil_training_seed", sfx, (10, 5), g05)

    # ---- 06 jalur evaluasi semua seed (satu panel per metode) + 07 heatmap tabrakan gabungan
    lay0 = next(iter(next(iter(S.values())).values()))["lay"]

    def save_fig(name, figsize, draw):
        fig, axes = plt.subplots(1, len(present), figsize=figsize, squeeze=False, layout="constrained")
        draw(fig, axes[0])
        path = outdir / f"{name}{sfx}.png"
        fig.savefig(path, dpi=110)
        plt.close(fig)
        print("ditulis:", path)

    def g06(fig, axes):
        for ax, (alg, name, col) in zip(axes, present):
            qa.draw_map(ax, lay0)
            best = None
            for s, v in S[alg].items():
                c = plt.cm.tab10(LABEL[s] - 1)
                for e in v["eps"]:
                    if e["outcome"] == "goal" and e.get("path"):
                        xs, ys = zip(*e["path"])
                        ax.plot(xs, ys, color=c, lw=1.0, alpha=0.55)
                        L = path_len(e["path"])
                        if best is None or L < best[0]:
                            best = (L, xs, ys, s)
            if best:
                ax.plot(best[1], best[2], color="k", lw=2.6, zorder=8)
                ax.plot([], [], color="k", lw=2.6, label=f"jalur terpendek: {best[0]:.2f} m (seed {LABEL[best[3]]})")
            for s in S[alg]:
                ax.plot([], [], color=plt.cm.tab10(LABEL[s] - 1), lw=2, label=f"seed {LABEL[s]}")
            ax.legend(loc="upper left", fontsize=7, framealpha=.9)
            ok = sum(v["eval_ok"] for v in S[alg].values()); tot = sum(v["eval_n"] for v in S[alg].values())
            ax.set_title(f"{name}: jalur evaluasi greedy ({ok}/{tot} sampai target)")

    save_fig("06_jalur_evaluasi_semua_seed", (7 * len(present), 7), g06)

    def g07(fig, axes):
        H, M = {}, {}
        for alg, name, col in present:
            logs = [v["log"] for v in S[alg].values() if v["log"]["x"] is not None]
            M[alg] = {k: np.concatenate([l[k] for l in logs]) for k in ("out", "x", "y")}
            sel = M[alg]["out"] == "collision"
            H[alg] = qa.heat_hist(M[alg], lay0, 240, sel, 0.065) / max(1, len(logs))     # rata-rata per seed
        vmax = max(1e-9, max(h.max() for h in H.values()))
        im = None
        for ax, (alg, name, col) in zip(axes, present):
            n = int((M[alg]["out"] == "collision").sum()) / max(1, len(S[alg]))
            im = qa.panel_heat(ax, M[alg], lay0, H[alg], vmax, f"{name}: tabrakan rata-rata {n:.0f} per seed",
                               np.ones(len(M[alg]["out"]), bool))
        fig.colorbar(im, ax=list(axes), fraction=.03, pad=.02, shrink=.85,
                     label="tabrakan per 0,1 m × 0,1 m, rata-rata per seed (skala sama)")

    save_fig("07_heatmap_tabrakan_semua_seed", (7 * len(present) + 1.5, 7), g07)

    # ---- 08-10 + ringkasan_evaluasi: evaluasi di luar training pada dua peta (sama / dengan rintangan tambahan)
    ev_roots = {"qlearning": HERE.parent / "continuous_qlearning" / "seed" / "evaluasi", "dqn": HERE / "seed" / "evaluasi"}
    PETA = (("sama", "Peta yang sama"), ("rintangan", "Peta + rintangan tambahan"))
    E = {}      # E[alg][peta][seed] = isi eval_result.json
    if not a.folder:
        for alg, _, _ in present:
            for peta, _ in PETA:
                for s in seeds:
                    f = ev_roots[alg] / peta / f"{alg}_s{s}" / "eval_result.json"
                    if f.exists():
                        E.setdefault(alg, {}).setdefault(peta, {})[s] = json.loads(f.read_text(encoding="utf-8"))
    if E:
        def ev_stats(j):
            g = [e for e in j["episodes"] if e["outcome"] == "goal"]
            return {"ok": j["success"], "n": j["n"], "dec": [e["decisions"] for e in g],
                    "path": [path_len(e["path"]) for e in g if e.get("path")],
                    "cnt": {o: sum(e["outcome"] == o for e in j["episodes"]) for o in OUTS}}
        ES = {alg: {peta: {s: ev_stats(j) for s, j in d.items()} for peta, d in E[alg].items()} for alg in E}

        def g08(fig, axes):
            for ax, (peta, ptitle) in zip(axes, PETA):
                w = 0.8 / len(present)
                for i, (alg, name, col) in enumerate(present):
                    ys = []
                    for j, s in enumerate(seeds):
                        v = ES.get(alg, {}).get(peta, {}).get(s)
                        y = np.nan if v is None else 100.0 * v["ok"] / v["n"]
                        x = j + (i - (len(present) - 1) / 2) * w
                        ys.append(y)
                        ax.bar(x, y, w * 0.92, color=col, label=name if j == 0 else None)
                        if not np.isnan(y):
                            ax.text(x, y + 1, f"{y:.0f}", ha="center", fontsize=8)
                    ax.axhline(np.nanmean(ys), color=col, ls="--", lw=1)
                ax.set_xticks(range(len(seeds))); ax.set_xticklabels([f"seed {LABEL[s]}" for s in seeds])
                ax.set_ylim(0, 125); ax.set_ylabel("% uji sampai target"); ax.set_title(ptitle)
                ax.grid(axis="y", alpha=.3); ax.legend(fontsize=8, loc="upper center", ncol=2)
        save_fig("08_evaluasi_dua_peta_seed", (7 * len(PETA), 5), g08)

        have_obs = any("rintangan" in E[alg] for alg in E)
        if have_obs:
            lay_obs = next(j for alg in E for j in E[alg].get("rintangan", {}).values())

            def g09(fig, axes):
                for ax, (alg, name, col) in zip(axes, present):
                    qa.draw_map(ax, lay_obs)
                    cnt = {o: 0 for o in OUTS}
                    for s, j in E.get(alg, {}).get("rintangan", {}).items():
                        for e in j["episodes"]:
                            cnt[e["outcome"]] += 1
                            if e.get("path"):
                                xs, ys = zip(*e["path"])
                                ax.plot(xs, ys, color=qa.COL[e["outcome"]], lw=0.9, alpha=0.45)
                                if e["outcome"] != "goal":
                                    ax.plot(xs[-1], ys[-1], "x", color=qa.COL[e["outcome"]], ms=6, mew=1.6)
                    for o in OUTS:
                        if cnt[o]:
                            ax.plot([], [], color=qa.COL[o], lw=2, label=f"{OUT_NAME[o]} ({cnt[o]})")
                    ax.legend(loc="upper left", fontsize=8, framealpha=.9)
                    ax.set_title(f"{name}: jalur evaluasi di peta + rintangan tambahan")
            save_fig("09_jalur_peta_rintangan", (7 * len(present), 7), g09)

            def g10(fig, axes):
                H, M = {}, {}
                for alg, name, col in present:
                    xs, ys, outs = [], [], []
                    for j in E.get(alg, {}).get("rintangan", {}).values():
                        for e in j["episodes"]:
                            if e.get("path"):
                                xs.append(e["path"][-1][0]); ys.append(e["path"][-1][1]); outs.append(e["outcome"])
                    M[alg] = {"x": np.array(xs), "y": np.array(ys), "out": np.array(outs)}
                    H[alg] = qa.heat_hist(M[alg], lay_obs, 240, M[alg]["out"] == "collision", 0.065)
                vmax = max(1e-9, max(h.max() for h in H.values()))
                im = None
                for ax, (alg, name, col) in zip(axes, present):
                    im = qa.panel_heat(ax, M[alg], lay_obs, H[alg], vmax,
                                       f"{name}: tabrakan evaluasi ({int((M[alg]['out'] == 'collision').sum())} dari {len(M[alg]['out'])} uji)",
                                       np.ones(len(M[alg]["out"]), bool))
                fig.colorbar(im, ax=list(axes), fraction=.03, pad=.02, shrink=.85,
                             label="tabrakan per 0,1 m × 0,1 m (semua seed, semua uji; skala sama)")
            save_fig("10_heatmap_tabrakan_peta_rintangan", (7 * len(present) + 1.5, 7), g10)

        cols_e = ["algoritma", "peta", "label", "seed", "sukses", "n_uji", "keputusan_rata", "jalur_rata_m", "tabrakan", "macet", "timeout"]
        rows_e = []
        for alg, name, _ in present:
            for peta, _pt in PETA:
                for s, v in sorted(ES.get(alg, {}).get(peta, {}).items()):
                    rows_e.append([name, peta, f"seed {LABEL[s]}", s, v["ok"], v["n"],
                                   round(float(np.mean(v["dec"])), 1) if v["dec"] else "",
                                   round(float(np.mean(v["path"])), 3) if v["path"] else "",
                                   v["cnt"]["collision"], v["cnt"]["stuck"], v["cnt"]["timeout"]])
        with open(outdir / f"ringkasan_evaluasi{sfx}.csv", "w", newline="", encoding="utf-8") as f:
            w = csv.writer(f); w.writerow(cols_e); w.writerows(rows_e)
        LE = ["Evaluasi di luar training (greedy, tanpa belajar; model terpilih tiap seed)", ""]
        LE.append(" | ".join(cols_e)); LE += [" | ".join(str(x) for x in r) for r in rows_e]
        LE += ["", "Total semua seed:"]
        for alg, name, _ in present:
            for peta, ptitle in PETA:
                V = list(ES.get(alg, {}).get(peta, {}).values())
                if not V:
                    continue
                ok, n = sum(v["ok"] for v in V), sum(v["n"] for v in V)
                dec = [x for v in V for x in v["dec"]]; pl = [x for v in V for x in v["path"]]
                LE.append(f"{name} | {ptitle}: {ok}/{n} ({100 * ok / n:.0f}%) | keputusan {np.mean(dec):.0f} | jalur {np.mean(pl):.2f} m | "
                          f"tabrakan {sum(v['cnt']['collision'] for v in V)} macet {sum(v['cnt']['stuck'] for v in V)} timeout {sum(v['cnt']['timeout'] for v in V)}"
                          if dec else f"{name} | {ptitle}: {ok}/{n} ({100 * ok / n:.0f}%) | tidak ada yang sampai target")
        (outdir / f"ringkasan_evaluasi{sfx}.txt").write_text(chr(10).join(LE), encoding="utf-8")
        print(chr(10).join(LE[-6:]))

    # ---- ringkasan
    cols = ["algoritma", "label", "seed", "episode_training", "goal_pertama", "konvergen", "model_episode", "eval_goal",
            "eval_n", "keputusan_rata", "jalur_rata_m", "tabrakan", "macet", "timeout", "hazard_per_episode", "detik_nyata"]
    rows = []
    for alg, name, _ in present:
        for s, v in S[alg].items():
            rows.append([name, f"seed {LABEL[s]}", s, v["n_ep"], v["first_goal"], v["conv"], v["model_ep"], v["eval_ok"], v["eval_n"],
                         round(float(np.mean(v["dec"])), 1) if v["dec"] else "",
                         round(float(np.mean(v["path"])), 3) if v["path"] else "",
                         v["cnt"]["collision"], v["cnt"]["stuck"], v["cnt"]["timeout"], round(v["haz"], 2), v["wall"] or ""])
    with open(outdir / f"ringkasan_seed{sfx}.csv", "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f); w.writerow(cols); w.writerows(rows)
    L = [f"Perbandingan seed: {root}", f"konvergen = rate {a.window}-episode >= {a.target}", ""]
    L.append(" | ".join(f"{c}" for c in cols))
    L += [" | ".join(str(x) for x in r) for r in rows]
    L += ["", "Rata-rata +- simpangan antar seed:"]
    for alg, name, _ in present:
        V = list(S[alg].values())
        def ms(vals, fmt):
            m, sd = mean_sd(vals)
            return "-" if np.isnan(m) else (fmt % m) + " +- " + (fmt % sd)
        L.append(f"{name} ({len(V)} seed): konvergen {ms([v['conv'] for v in V], '%.0f')} | "
                 f"goal pertama {ms([v['first_goal'] for v in V], '%.0f')} | "
                 f"eval goal {sum(v['eval_ok'] for v in V)}/{sum(v['eval_n'] for v in V)} | "
                 f"keputusan {ms([np.mean(v['dec']) for v in V if v['dec']], '%.1f')} | "
                 f"jalur {ms([np.mean(v['path']) for v in V if v['path']], '%.2f')} m | "
                 f"tabrakan {ms([v['cnt']['collision'] for v in V], '%.0f')} | macet {ms([v['cnt']['stuck'] for v in V], '%.0f')} | "
                 f"timeout {ms([v['cnt']['timeout'] for v in V], '%.0f')}")
    txt = "\n".join(L)
    (outdir / f"ringkasan_seed{sfx}.txt").write_text(txt, encoding="utf-8")
    print(txt)


if __name__ == "__main__":
    main()
