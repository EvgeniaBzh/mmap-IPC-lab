## Latency, msg=64 B, медіана (ns)

| mechanism | different cores | same core | same/diff |
|---|---|---|---|
| shm_spin | 83 | 312 | 3.76 |
| shm_sem | 15,291 | 812 | 0.05 |
| mmap_anon_spin | 83 | 312 | 3.76 |
| mmap_anon_sem | 14,812 | 812 | 0.05 |
| pipe | 13,312 | 812 | 0.06 |
| fifo | 13,021 | 812 | 0.06 |
| socketpair | 14,083 | 896 | 0.06 |
| unix | 14,000 | 916 | 0.07 |
| tcp | 16,958 | 1,562 | 0.09 |
| mqueue | 12,666 | 791 | 0.06 |
| mmap_file_msync_sync_spin | 25,208 | 25,375 | 1.01 |

## Throughput, block=64K, MB/s

| mechanism | different cores | same core | same/diff |
|---|---|---|---|
| pipe | 2,353 | 9,920 | 4.22 |
| fifo | 2,098 | 11,725 | 5.59 |
| socketpair | 6,652 | 12,937 | 1.94 |
| unix | 6,746 | 12,973 | 1.92 |
| tcp | 15,027 | 11,454 | 0.76 |
| shm | 50,357 | 28,162 | 0.56 |
| mmap_anon | 50,271 | 29,071 | 0.58 |
| mmap_file | 57,537 | 28,122 | 0.49 |
