#!/usr/bin/env python3
"""Лише візуалізація/агрегація результатів. Використання: python3 plot.py [results_dir]"""
import sys, pandas as pd
import matplotlib; matplotlib.use("Agg")
import matplotlib.pyplot as plt

d = sys.argv[1] if len(sys.argv) > 1 else "results"
lat = pd.read_csv(f"{d}/results_lat.csv")
thr = pd.read_csv(f"{d}/results_thr.csv")

def md(df):
    cols = [df.index.name or ""] + [str(c) for c in df.columns]
    out = ["| " + " | ".join(cols) + " |", "|" + "---|" * len(cols)]
    for idx, row in df.iterrows():
        out.append("| " + " | ".join([str(idx)] + [("" if pd.isna(v) else f"{v:,.0f}") for v in row]) + " |")
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

fig, ax = plt.subplots(figsize=(9, 5))
for name, row in thr_mean.iterrows():
    ax.plot(row.index, row.values, marker="o", label=name)
ax.set_xscale("log", base=2); ax.set_yscale("log")
ax.set_xlabel("block size (bytes)"); ax.set_ylabel("MB/s"); ax.set_title("Throughput vs block size")
ax.grid(True, which="both", alpha=.3); ax.legend(fontsize=7, ncol=2)
fig.tight_layout(); fig.savefig(f"{d}/throughput.png", dpi=150)

size = 64 if 64 in lat_med.columns else lat_med.columns[0]
s = lat_med[size].dropna().sort_values()
fig, ax = plt.subplots(figsize=(9, 6))
ax.barh(s.index, s.values); ax.set_xscale("log"); ax.set_xlabel(f"median one-way latency, ns (msg={size} B)")
ax.set_title("Latency"); fig.tight_layout(); fig.savefig(f"{d}/latency.png", dpi=150)
print(f"Збережено: {d}/summary.md, throughput.png, latency.png")
