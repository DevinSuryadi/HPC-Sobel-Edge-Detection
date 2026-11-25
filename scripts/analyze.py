#!/usr/bin/env python3
import csv
import os
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

RES_ORDER = ["720p", "1080p", "4k"]
RES_LABEL = {"720p": "720p", "1080p": "1080p", "4k": "4K"}
TARGET_FPS = 30.0

# Palet referensi (mode terang), tervalidasi: warna = resolusi di semua grafik.
COLOR = {"720p": "#2a78d6", "1080p": "#eb6834", "4k": "#1baf7a"}
SURFACE, INK, INK2, MUTED = "#fcfcfb", "#0b0b0b", "#52514e", "#898781"
GRID, AXIS = "#e1e0d9", "#c3c2b7"

plt.rcParams.update({
    "figure.facecolor": SURFACE, "axes.facecolor": SURFACE, "savefig.facecolor": SURFACE,
    "axes.edgecolor": AXIS, "axes.linewidth": 0.8,
    "axes.labelcolor": INK2, "text.color": INK,
    "xtick.color": MUTED, "ytick.color": MUTED,
    "xtick.labelcolor": INK2, "ytick.labelcolor": INK2,
    "axes.grid": True, "axes.grid.axis": "y", "grid.color": GRID, "grid.linewidth": 0.6,
    "axes.spines.top": False, "axes.spines.right": False,
    "axes.titlesize": 11, "axes.titleweight": "bold", "axes.titlelocation": "left",
    "font.size": 9.5, "legend.frameon": False,
})


def load(path):
    with open(path, newline="") as f:
        rows = list(csv.DictReader(f))
    for r in rows:
        for k in ("width", "height", "frames", "nodes", "threads_per_node", "runs"):
            r[k] = int(r[k])
        for k in ("median_s", "min_s", "max_s", "ms_per_frame", "fps"):
            r[k] = float(r[k])
        r["p"] = r["nodes"] * r["threads_per_node"]
    return rows


def compute_metrics(rows):
    t_s = {r["resolution"]: r["median_s"] for r in rows if r["version"] == "V0"}
    ref_sum = {r["resolution"]: r["checksum"] for r in rows if r["version"] == "V0"}
    for r in rows:
        ts = t_s.get(r["resolution"])
        r["speedup"] = ts / r["median_s"] if ts else float("nan")
        r["efficiency"] = r["speedup"] / r["p"] * 100.0
        r["cost"] = r["median_s"] * r["p"]
        r["realtime"] = r["fps"] >= TARGET_FPS
        if r["version"] == "V1":
            r["checksum_ok"] = r["checksum"] == ref_sum.get(r["resolution"])
    return rows


def config_label(r):
    if r["version"] == "V2":
        return f"{r['nodes']} node x {r['threads_per_node']} thread"
    return f"{r['threads_per_node']} thread"


def write_tables(rows, outdir):
    order = {"V0": 0, "V1": 1, "V2": 2}
    rows = sorted(rows, key=lambda r: (RES_ORDER.index(r["resolution"]),
                                       order[r["version"]], r["nodes"], r["threads_per_node"]))
    cols = ["resolution", "version", "nodes", "threads_per_node", "p", "frames",
            "median_s", "ms_per_frame", "fps", "realtime", "speedup", "efficiency", "cost"]
    with open(os.path.join(outdir, "summary.csv"), "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(cols)
        for r in rows:
            w.writerow([r[c] if not isinstance(r[c], float) else f"{r[c]:.4f}" for c in cols])

    lines = [
        "| Resolusi | Versi | Konfigurasi | p | Waktu total (s) | Waktu/frame (ms) | FPS "
        "| Real-time 30 FPS | Speedup | Efisiensi (%) | Computation cost (s) |",
        "|---|---|---|---:|---:|---:|---:|---|---:|---:|---:|",
    ]
    for r in rows:
        lines.append(
            f"| {RES_LABEL[r['resolution']]} | {r['version']} | {config_label(r)} | {r['p']} "
            f"| {r['median_s']:.3f} | {r['ms_per_frame']:.2f} | {r['fps']:.1f} "
            f"| {'ya' if r['realtime'] else 'tidak'} | {r['speedup']:.2f} "
            f"| {r['efficiency']:.1f} | {r['cost']:.3f} |")

    checks = [r for r in rows if r["version"] == "V1"]
    lines.append("")
    if checks:
        ok = all(r["checksum_ok"] for r in checks)
        lines.append(f"Checksum V1 = V0 pada semua konfigurasi: **{'ya' if ok else 'TIDAK'}**")
    with open(os.path.join(outdir, "summary.md"), "w") as f:
        f.write("\n".join(lines) + "\n")
    return rows


def label_line_ends(ax, items, xpos):
    """Label langsung di ujung garis (teks berwarna tinta, bukan warna seri),
    digeser vertikal agar tidak bertumpuk."""
    if not items:
        return
    lo, hi = ax.get_ylim()
    gap = (hi - lo) * 0.065
    items = sorted(items, key=lambda t: t[0])
    pos = [y for y, _ in items]
    # Dorong label yang terlalu dekat ke dua arah secara simetris, sehingga setiap
    # label tetap sedekat mungkin dengan titiknya sendiri dan urutannya terjaga.
    for _ in range(100):
        moved = False
        for i in range(1, len(pos)):
            overlap = gap - (pos[i] - pos[i - 1])
            if overlap > 1e-9:
                pos[i - 1] -= overlap / 2
                pos[i] += overlap / 2
                moved = True
        if not moved:
            break
    for y, (_, text) in zip(pos, items):
        ax.annotate(text, (xpos, y), xytext=(6, 0), textcoords="offset points",
                    va="center", ha="left", color=INK2, fontsize=8.5, annotation_clip=False)


def plot_series(ax, xs, ys, res):
    ax.plot(xs, ys, color=COLOR[res], linewidth=2, marker="o", markersize=6,
            markeredgecolor=SURFACE, markeredgewidth=1.5, label=RES_LABEL[res], zorder=3)


def fig_v1(rows, outdir):
    fig, axes = plt.subplots(1, 3, figsize=(12, 3.9))
    threads = sorted({r["threads_per_node"] for r in rows if r["version"] == "V1"})
    panels = [("median_s", "Waktu pemrosesan (s)"), ("speedup", "Speedup S(n)"),
              ("efficiency", "Efisiensi E(n) (%)")]
    for ax, (key, title) in zip(axes, panels):
        ends = []
        for res in RES_ORDER:
            pts = sorted((r["threads_per_node"], r[key]) for r in rows
                         if r["version"] == "V1" and r["resolution"] == res)
            if not pts:
                continue
            xs, ys = zip(*pts)
            plot_series(ax, xs, ys, res)
            ends.append((ys[-1], RES_LABEL[res]))
        if key == "speedup":
            ax.plot(threads, threads, color=MUTED, linewidth=1.2, linestyle="--",
                    label="ideal (S = n)", zorder=2)
        if key == "efficiency":
            ax.axhline(100, color=MUTED, linewidth=1.2, linestyle="--", zorder=2)
            ax.set_ylim(0, max(110, ax.get_ylim()[1]))
        else:
            ax.set_ylim(bottom=0)
        ax.set_title(title)
        ax.set_xlabel("Jumlah thread OpenMP (n)")
        ax.set_xticks(threads)
        label_line_ends(ax, ends, threads[-1])
        ax.set_xlim(threads[0] - 0.4, threads[-1] + 1.2)
    handles, labels = axes[1].get_legend_handles_labels()
    fig.legend(handles, labels, loc="upper right", ncol=4, bbox_to_anchor=(0.995, 1.02))
    fig.suptitle("V1 OpenMP single-node terhadap V0", x=0.01, ha="left",
                 fontsize=12, fontweight="bold", y=1.0)
    fig.tight_layout(rect=(0, 0, 1, 0.94))
    fig.savefig(os.path.join(outdir, "fig_v1_scaling.png"), dpi=200)
    plt.close(fig)


def fig_v2(rows, outdir):
    v2 = [r for r in rows if r["version"] == "V2"]
    if not v2:
        return
    fig, axes = plt.subplots(1, 2, figsize=(9.5, 3.9))
    nodes = sorted({r["nodes"] for r in v2})
    tpn = v2[0]["threads_per_node"]
    for ax, (key, title) in zip(axes, [("median_s", "Waktu pemrosesan (s)"),
                                       ("speedup", "Speedup S(p) terhadap V0")]):
        ends = []
        for res in RES_ORDER:
            pts = sorted((r["nodes"], r[key]) for r in v2 if r["resolution"] == res)
            if not pts:
                continue
            xs, ys = zip(*pts)
            plot_series(ax, xs, ys, res)
            ends.append((ys[-1], RES_LABEL[res]))
        if key == "speedup":
            ax.plot(nodes, [n * tpn for n in nodes], color=MUTED, linewidth=1.2,
                    linestyle="--", label=f"ideal (S = p = node x {tpn})", zorder=2)
        ax.set_ylim(bottom=0)
        ax.set_title(title)
        ax.set_xlabel(f"Jumlah compute node ({tpn} thread OpenMP per node)")
        ax.set_xticks(nodes)
        label_line_ends(ax, ends, nodes[-1])
        ax.set_xlim(nodes[0] - 0.3, nodes[-1] + 0.6)
    handles, labels = axes[1].get_legend_handles_labels()
    fig.legend(handles, labels, loc="upper right", ncol=4, bbox_to_anchor=(0.995, 1.02))
    fig.suptitle("V2 hybrid MPI-OpenMP", x=0.01, ha="left", fontsize=12,
                 fontweight="bold", y=1.0)
    fig.tight_layout(rect=(0, 0, 1, 0.94))
    fig.savefig(os.path.join(outdir, "fig_v2_scaling.png"), dpi=200)
    plt.close(fig)


def best_configs(rows):
    """Konfigurasi terbaik (median waktu terkecil) tiap versi per resolusi."""
    best = {}
    for r in rows:
        key = (r["version"], r["resolution"])
        if key not in best or r["median_s"] < best[key]["median_s"]:
            best[key] = r
    return best


def grouped_bars(ax, best, key):
    versions = [v for v in ("V0", "V1", "V2") if any(k[0] == v for k in best)]
    width = 0.26
    for i, res in enumerate(RES_ORDER):
        xs, ys = [], []
        for j, v in enumerate(versions):
            if (v, res) in best:
                xs.append(j + (i - 1) * width)
                ys.append(best[(v, res)][key])
        ax.bar(xs, ys, width=width, color=COLOR[res], edgecolor=SURFACE,
               linewidth=1.5, label=RES_LABEL[res], zorder=3)
    ticks = []
    for v in versions:
        cfgs = sorted({config_label(best[(v, res)]) for res in RES_ORDER if (v, res) in best})
        ticks.append(f"{v}\n" + " / ".join(cfgs))
    ax.set_xticks(range(len(versions)))
    ax.set_xticklabels(ticks)
    ax.tick_params(axis="x", length=0)


def fig_fps(best, outdir):
    """Satu panel per resolusi (skala sendiri) agar posisi tiap versi terhadap
    garis target 30 FPS terbaca jelas, termasuk pada 4K yang FPS-nya kecil."""
    versions = [v for v in ("V0", "V1", "V2") if any(k[0] == v for k in best)]
    fig, axes = plt.subplots(1, len(RES_ORDER), figsize=(11, 3.9))
    for ax, res in zip(axes, RES_ORDER):
        xs = [i for i, v in enumerate(versions) if (v, res) in best]
        ys = [best[(versions[i], res)]["fps"] for i in xs]
        ax.bar(xs, ys, width=0.6, color=COLOR[res], edgecolor=SURFACE, linewidth=1.5, zorder=3)
        target = ax.axhline(TARGET_FPS, color=INK, linewidth=1.2, zorder=4,
                            label="target real-time 30 FPS")
        ax.set_xticks(range(len(versions)))
        ax.set_xticklabels([f"{v}\n{config_label(best[(v, res)])}" for v in versions],
                           fontsize=8.5)
        ax.tick_params(axis="x", length=0)
        ax.set_title(RES_LABEL[res])
        ax.set_ylim(0, max(ys) * 1.1)
    axes[0].set_ylabel("Laju frame (FPS)")
    fig.legend(handles=[target], loc="upper right", bbox_to_anchor=(0.995, 1.02))
    fig.suptitle("Laju frame per resolusi (konfigurasi terbaik tiap versi)", x=0.01,
                 ha="left", fontsize=12, fontweight="bold", y=1.0)
    fig.tight_layout(rect=(0, 0, 1, 0.94))
    fig.savefig(os.path.join(outdir, "fig_fps.png"), dpi=200)
    plt.close(fig)


def fig_final(best, outdir):
    fig, ax = plt.subplots(figsize=(8, 4.2))
    grouped_bars(ax, best, "speedup")
    ax.axhline(1, color=INK, linewidth=1, zorder=4)
    ax.set_ylabel("Speedup terhadap V0")
    ax.set_title("Perbandingan akhir V0, V1, V2 (konfigurasi terbaik)")
    ax.legend(loc="upper left")
    fig.tight_layout()
    fig.savefig(os.path.join(outdir, "fig_final.png"), dpi=200)
    plt.close(fig)


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "results/results.csv"
    outdir = os.path.dirname(path) or "."
    rows = compute_metrics(load(path))
    rows = write_tables(rows, outdir)
    fig_v1(rows, outdir)
    fig_v2(rows, outdir)
    best = best_configs(rows)
    fig_fps(best, outdir)
    fig_final(best, outdir)
    print(open(os.path.join(outdir, "summary.md")).read())
    print("Grafik:", ", ".join(sorted(f for f in os.listdir(outdir) if f.startswith("fig_"))))


if __name__ == "__main__":
    main()
