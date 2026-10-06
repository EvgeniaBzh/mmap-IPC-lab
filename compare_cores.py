#!/usr/bin/env python3
"""Порівняння: процеси на різних ядрах vs на одному ядрі.
Використання: python3 compare_cores.py results_full results_samecore [msg_size=64]
Створює: compare_cores.png і compare_cores.md (у папці другого аргументу)"""
import sys
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

if len(sys.argv) < 3:
    sys.exit("usage: compare_cores.py <dir_diff_cores> <dir_same_core> [msg_size]")
a, b = sys.argv[1], sys.argv[2]
size = int(sys.argv[3]) if len(sys.argv) > 3 else 64
KEYS = ["shm_spin", "shm_sem", "mmap_anon_spin", "mmap_anon_sem", "pipe", "fifo",
        "socketpair", "unix", "tcp", "mqueue", "mmap_file_msync_sync_spin"]

def med(d):
    df = pd.read_csv(f"{d}/results_lat.csv")
    return df[df["size"] == size].groupby("name")["median_ns"].median()

def thr(d, block):
    df = pd.read_csv(f"{d}/results_thr.csv")
    return df[df["size"] == block].groupby("name")["MBps"].mean()

la, lb = med(a), med(b)
names = [k for k in KEYS if k in la.index and k in lb.index]
tab = pd.DataFrame({"different cores (ns)": la[names], "same core (ns)": lb[names]})
tab["ratio same/diff"] = tab["same core (ns)"] / tab["different cores (ns)"]

fig, ax = plt.subplots(figsize=(10, 6))
x = range(len(names)); w = 0.38
ax.bar([i - w / 2 for i in x], tab["different cores (ns)"], w, label="different cores")
ax.bar([i + w / 2 for i in x], tab["same core (ns)"], w, label="same core")
ax.set_yscale("log"); ax.set_xticks(list(x)); ax.set_xticklabels(names, rotation=45, ha="right")
ax.set_ylabel(f"median one-way latency, ns (log), msg={size} B")
ax.set_title("Latency: different cores vs same core"); ax.legend(); ax.grid(True, axis="y", alpha=.3)
fig.tight_layout(); fig.savefig(f"{b}/compare_cores.png", dpi=150)

with open(f"{b}/compare_cores.md", "w") as f:
    f.write(f"## Latency, msg={size} B, медіана (ns)\n\n| mechanism | different cores | same core | same/diff |\n|---|---|---|---|\n")
    for n, r in tab.iterrows():
        f.write(f"| {n} | {r.iloc[0]:,.0f} | {r.iloc[1]:,.0f} | {r.iloc[2]:.2f} |\n")
    ta, tb = thr(a, 65536), thr(b, 65536)
    nm = [k for k in ["pipe", "fifo", "socketpair", "unix", "tcp", "shm", "mmap_anon", "mmap_file"] if k in ta.index and k in tb.index]
    f.write("\n## Throughput, block=64K, MB/s\n\n| mechanism | different cores | same core | same/diff |\n|---|---|---|---|\n")
    for n in nm:
        f.write(f"| {n} | {ta[n]:,.0f} | {tb[n]:,.0f} | {tb[n] / ta[n]:.2f} |\n")
print(f"Збережено: {b}/compare_cores.png, {b}/compare_cores.md")