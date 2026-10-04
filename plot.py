#!/usr/bin/env python3
"""Агрегація та візуалізація результатів. Використання: python3 plot.py [results_dir]
Створює: summary.md, throughput.png (3 панелі за групами), latency.png, latency_vs_size.png"""
import sys
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

d = sys.argv[1] if len(sys.argv) > 1 else "results"
lat = pd.read_csv(f"{d}/results_lat.csv")
thr = pd.read_csv(f"{d}/results_thr.csv")

def group(name):
    if name.startswith("file_"):
        return "File I/O"
    if name.startswith(("shm", "mmap")):
        return "Shared memory / mmap"
    return "Kernel IPC (pipe, socket, queue)"

COLORS = {"File I/O": "tab:green", "Shared memory / mmap": "tab:red",
          "Kernel IPC (pipe, socket, queue)": "tab:blue"}

def fmt_size(b):
    b = int(b)
    return f"{b // 1048576}M" if b >= 1048576 else f"{b // 1024}K" if b >= 1024 else str(b)

def md(df):
    cols = [df.index.name or ""] + [fmt_size(c) for c in df.columns]
    out = ["| " + " | ".join(cols) + " |", "|" + "---|" * len(cols)]
    for idx, row in df.iterrows():
        out.append("| " + " | ".join([str(idx)] + ["" if pd.isna(v) else f"{v:,.0f}" for v in row]) + " |")
    return "\n".join(out)

lat_med = lat.groupby(["name", "size"])["median_ns"].median().unstack()
lat_p99 = lat.groupby(["name", "size"])["p99_ns"].median().unstack()
thr_mean = thr.groupby(["name", "size"])["MBps"].mean().unstack()
thr_sd = thr.groupby(["name", "size"])["MBps"].std().unstack()

with open(f"{d}/summary.md", "w") as f:
    f.write("## Latency, медіана one-way (ns), медіана по запусках\n\n" + md(lat_med) + "\n\n")
    f.write("## Latency, p99 (ns)\n\n" + md(lat_p99) + "\n\n")
    f.write("## Throughput (MB/s), середнє по запусках\n\n" + md(thr_mean) + "\n\n")
    f.write("## Throughput, std (MB/s)\n\n" + md(thr_sd) + "\n")

# ---- Throughput: 3 панелі (одна на групу), спільна вісь Y ----
groups = ["Kernel IPC (pipe, socket, queue)", "Shared memory / mmap", "File I/O"]
fig, axes = plt.subplots(1, 3, figsize=(16, 5), sharey=True)
all_sizes = sorted(thr_mean.columns)
for ax, g in zip(axes, groups):
    for name, row in thr_mean.iterrows():
        if group(name) != g:
            continue
        row = row.dropna()                     # прибирає NaN -> суцільні лінії
        if row.empty:
            continue
        ax.plot(row.index, row.values, marker="o", label=name)
    ax.set_xscale("log", base=2)
    ax.set_yscale("log")
    ax.set_xticks(all_sizes)
    ax.set_xticklabels([fmt_size(s) for s in all_sizes])
    ax.set_title(g)
    ax.set_xlabel("block size (bytes)")
    ax.grid(True, which="both", alpha=.3)
    ax.legend(fontsize=7, loc="best", framealpha=0.85)
axes[0].set_ylabel("throughput, MB/s (log)")
fig.suptitle("Throughput vs block size")
fig.tight_layout()
fig.savefig(f"{d}/throughput.png", dpi=150)

# ---- Latency: стовпчики для 64 B, кольори за групами ----
size = 64 if 64 in lat_med.columns else lat_med.columns[0]
s = lat_med[size].dropna().sort_values()
fig, ax = plt.subplots(figsize=(10, 6))
ax.barh(s.index, s.values, color=[COLORS[group(n)] for n in s.index])
ax.set_xscale("log")
ax.set_xlabel(f"median one-way latency, ns, log (msg={size} B)")
ax.set_title("Latency")
for i, v in enumerate(s.values):
    ax.text(v * 1.05, i, f"{v:,.0f}", va="center", fontsize=8)
handles = [plt.Rectangle((0, 0), 1, 1, color=c) for c in COLORS.values()]
ax.legend(handles, COLORS.keys(), loc="lower right", fontsize=8)
fig.tight_layout()
fig.savefig(f"{d}/latency.png", dpi=150)

# ---- Latency vs message size: лише представники кожного кластера ----
REPS = [
    ("shm_spin", "shared memory + spin", "tab:red", "o"),
    ("mmap_file_msync_async_spin", "mmap file + msync(MS_ASYNC)", "tab:orange", "s"),
    ("shm_sem", "shared memory + semaphore", "tab:purple", "^"),
    ("pipe", "pipe", "tab:blue", "D"),
    ("tcp", "TCP loopback", "tab:green", "v"),
    ("mmap_file_msync_sync_spin", "mmap file + msync(MS_SYNC)", "tab:brown", "X"),
]
fig, ax = plt.subplots(figsize=(10, 6))
sizes = sorted(lat_med.columns)
for key, label, color, marker in REPS:
    if key not in lat_med.index:
        continue
    row = lat_med.loc[key].dropna()
    ax.plot(row.index, row.values, marker=marker, color=color, label=label, linewidth=2)
    ax.annotate(f"{row.values[-1]:,.0f} ns", (row.index[-1], row.values[-1]),
                textcoords="offset points", xytext=(6, 0), va="center", fontsize=8, color=color)
ax.set_xscale("log", base=2)
ax.set_yscale("log")
ax.set_xticks(sizes)
ax.set_xticklabels([fmt_size(x) for x in sizes])
ax.set_xlim(sizes[0] / 1.2, sizes[-1] * 2.2)
ax.set_xlabel("message size (bytes)")
ax.set_ylabel("median one-way latency, ns (log)")
ax.set_title("Latency vs message size (representative mechanisms)")
ax.grid(True, which="both", alpha=.3)
ax.legend(fontsize=9, loc="center left")
fig.tight_layout()
fig.savefig(f"{d}/latency_vs_size.png", dpi=150)

print(f"Збережено в {d}/: summary.md, throughput.png, latency.png, latency_vs_size.png")