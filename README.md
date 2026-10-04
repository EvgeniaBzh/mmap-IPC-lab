# Lab 2: mmap & IPC comparison (C, Linux)

## Збірка
    make            # потрібні gcc, Linux (на Windows - WSL2)

## Програми
| Файл | Що вимірює |
|---|---|
| `stream_bench.c` | pipe, FIFO, socketpair, UNIX socket, TCP loopback (lat + thr) |
| `mq_bench.c` | POSIX message queue (lat + thr) |
| `shm_bench.c` | POSIX shared memory: `shm_open`+`mmap`; sync = spin або sem; режим `both` (fork) або `a`/`b` (непов'язані процеси) |
| `mmap_bench.c` | mmap-режими: anon / anon_populate / anon_hugetlb / file / file_populate / file_msync_async / file_msync_sync; `pf` (page faults); `demo` (MAP_PRIVATE vs MAP_SHARED) |
| `file_bench.c` | read/write/fsync/O_DIRECT проти mmap (read, seq, rand, write, msync) |
| `capacity.c` | місткість pipe, FIFO, socket, mqueue, shm |
| `common.h`, `shared_core.h` | таймінг, pin на ядра, статистика, ping-pong та SPSC ring у спільній пам'яті |

## Приклади
    ./stream_bench pipe lat 64 200000          # latency, 64 B, 200k ітерацій
    ./stream_bench unix thr 65536 1024         # throughput, блок 64 KiB, 1 GiB
    ./shm_bench lat 64 200000 sem              # shm із семафором (а не spin)
    ./mmap_bench file_msync_sync lat 64 3000   # вартість msync(MS_SYNC)
    ./mmap_bench file_private pf 256           # вартість page fault + COW
    ./mmap_bench anon_private demo             # MAP_PRIVATE не є IPC
    ./file_bench read 65536 512                # читання файлу read()
    ./file_bench mmap_seq 65536 512            # ... проти mmap + MADV_SEQUENTIAL
    ./capacity
    # два непов'язані процеси через shm (два термінали):
    ./shm_bench lat 64 100000 spin a   &   ./shm_bench lat 64 100000 spin b

## Повний прогін і графіки
    REPEAT=5 ./run_all.sh                       # -> results/
    IPC_SAME_CORE=1 OUT=results_samecore ./run_all.sh   # обидва процеси на одному ядрі
    python3 plot.py results                     # -> summary.md, throughput.png, latency.png
Швидка перевірка: `REPEAT=1 ITERS=20000 TOTAL_MIB=64 ./run_all.sh`

## Змінні середовища
`IPC_SAME_CORE=1`, `IPC_NO_PIN=1`, `IPC_CPU_A/IPC_CPU_B`, `FILE_PATH`, `MMAP_PATH`.

## Формат результатів
- lat: `name,lat,msg_size,mean_ns,median_ns,p99_ns,max_ns` (one-way = RTT/2)
- thr: `name,thr,block_size,MB/s` (MiB/s)
- додаткова статистика (CPU time, context switches, minor faults) друкується в stderr: `[rusage] ...`

## Для коректних вимірів
- Запускай на фізичній машині/WSL2 з ≥2 ядрами; у віртуалках з 1 vCPU spin-варіанти деградують (код автоматично додає `sched_yield`).
- Відключи енергозбереження: `sudo cpupower frequency-set -g performance`.
- Для чесного порівняння файлів із диском: `FILE_PATH` на справжній диск (не tmpfs), `drop_caches`, `*_odirect`.
- `anon_hugetlb`: `echo 64 | sudo tee /proc/sys/vm/nr_hugepages`.
- Запускай `strace -c`, `perf stat -e context-switches,cpu-migrations,page-faults` для бонусних метрик.

## Відомі особливості
- Кільцевий буфер shm/mmap має розмір 1 МіБ: блок 1 МіБ повністю заповнює його, тому продюсер і консюмер не працюють паралельно - throughput для такого блоку нижчий (це варто пояснити у звіті або збільшити `RING_SIZE` у `shared_core.h`).
- `anon_hugetlb` і `*_odirect` залежать від налаштувань системи; якщо недоступні - відзначити це у звіті.
