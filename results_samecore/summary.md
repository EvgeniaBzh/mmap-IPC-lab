## Latency, медіана one-way (ns), медіана по запусках

| name | 8 | 64 | 512 | 4K |
|---|---|---|---|---|
| fifo | 812 | 812 | 812 | 958 |
| mmap_anon_populate_sem | 812 | 812 | 833 | 833 |
| mmap_anon_populate_spin | 312 | 312 | 312 | 333 |
| mmap_anon_sem | 812 | 812 | 833 | 854 |
| mmap_anon_spin | 312 | 312 | 312 | 333 |
| mmap_file_msync_async_spin | 437 | 437 | 437 | 458 |
| mmap_file_msync_sync_spin | 25,458 | 25,375 | 25,437 | 26,791 |
| mmap_file_sem | 812 | 833 | 833 | 833 |
| mmap_file_spin | 312 | 312 | 312 | 333 |
| mqueue | 792 | 791 | 792 | 1,041 |
| pipe | 812 | 812 | 833 | 958 |
| shm_sem | 812 | 812 | 812 | 833 |
| shm_spin | 312 | 312 | 312 | 333 |
| socketpair | 917 | 896 | 937 | 1,104 |
| tcp | 1,583 | 1,562 | 1,562 | 1,771 |
| unix | 917 | 916 | 917 | 1,104 |

## Latency, p99 (ns)

| name | 8 | 64 | 512 | 4K |
|---|---|---|---|---|
| fifo | 854 | 854 | 875 | 1,333 |
| mmap_anon_populate_sem | 895 | 875 | 937 | 875 |
| mmap_anon_populate_spin | 416 | 416 | 417 | 437 |
| mmap_anon_sem | 1,146 | 916 | 916 | 1,166 |
| mmap_anon_spin | 416 | 396 | 437 | 458 |
| mmap_file_msync_async_spin | 625 | 625 | 625 | 666 |
| mmap_file_msync_sync_spin | 32,583 | 33,958 | 32,646 | 36,187 |
| mmap_file_sem | 875 | 875 | 937 | 875 |
| mmap_file_spin | 416 | 417 | 437 | 437 |
| mqueue | 958 | 875 | 916 | 1,083 |
| pipe | 896 | 854 | 1,146 | 1,104 |
| shm_sem | 1,000 | 1,104 | 875 | 895 |
| shm_spin | 437 | 396 | 416 | 458 |
| socketpair | 1,645 | 1,625 | 1,646 | 1,833 |
| tcp | 2,208 | 1,687 | 2,041 | 1,958 |
| unix | 1,895 | 1,625 | 1,646 | 1,833 |

## Throughput (MB/s), середнє по запусках

| name | 4K | 8K | 64K | 1M |
|---|---|---|---|---|
| fifo | 6,854 |  | 11,725 | 11,549 |
| file_mmap_rand | 7,463 |  | 15,094 | 16,589 |
| file_mmap_read | 15,674 |  | 16,768 | 16,678 |
| file_mmap_seq | 16,234 |  | 16,745 | 16,925 |
| file_mmap_write | 4,308 |  | 4,281 | 4,243 |
| file_mmap_write_msync | 1,011 |  | 1,158 | 1,172 |
| file_read | 9,062 |  | 12,042 | 12,402 |
| file_read_rand | 5,256 |  | 9,868 | 12,460 |
| file_write | 3,044 |  | 6,383 | 6,662 |
| file_write_fsync | 1,029 |  | 1,330 | 1,405 |
| mmap_anon | 28,595 |  | 29,071 | 23,983 |
| mmap_anon_populate | 27,875 |  | 29,280 | 23,696 |
| mmap_file | 24,618 |  | 28,122 | 24,044 |
| mqueue | 5,133 | 7,981 |  |  |
| pipe | 6,853 |  | 9,920 | 11,874 |
| shm | 28,657 |  | 28,162 | 23,634 |
| socketpair | 4,143 |  | 12,937 | 15,278 |
| tcp | 2,781 |  | 11,454 | 14,150 |
| unix | 4,619 |  | 12,973 | 15,206 |

## Throughput, std (MB/s)

| name | 4K | 8K | 64K | 1M |
|---|---|---|---|---|
| fifo | 92 |  | 123 | 291 |
| file_mmap_rand | 81 |  | 110 | 431 |
| file_mmap_read | 167 |  | 229 | 225 |
| file_mmap_seq | 53 |  | 134 | 418 |
| file_mmap_write | 70 |  | 14 | 42 |
| file_mmap_write_msync | 195 |  | 39 | 63 |
| file_read | 64 |  | 240 | 166 |
| file_read_rand | 35 |  | 504 | 106 |
| file_write | 2,149 |  | 105 | 177 |
| file_write_fsync | 110 |  | 57 | 41 |
| mmap_anon | 1,500 |  | 267 | 76 |
| mmap_anon_populate | 1,478 |  | 208 | 69 |
| mmap_file | 6,156 |  | 1,500 | 105 |
| mqueue | 996 | 434 |  |  |
| pipe | 7 |  | 327 | 214 |
| shm | 1,275 |  | 1,751 | 630 |
| socketpair | 986 |  | 225 | 584 |
| tcp | 371 |  | 117 | 652 |
| unix | 241 |  | 108 | 632 |
