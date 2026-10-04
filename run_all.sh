#!/bin/bash
# Повний прогін. Налаштування через env:
#   REPEAT=5 ITERS=200000 TOTAL_MIB=512 ./run_all.sh
#   IPC_SAME_CORE=1 ./run_all.sh   (обидва процеси на одному ядрі)
#   OUT=dir                        (куди писати результати)
set -u
REPEAT=${REPEAT:-5}; ITERS=${ITERS:-200000}; TOTAL_MIB=${TOTAL_MIB:-512}
OUT=${OUT:-results}; mkdir -p "$OUT"
LAT=$OUT/results_lat.csv; THR=$OUT/results_thr.csv
echo "run,name,mode,size,mean_ns,median_ns,p99_ns,max_ns" > "$LAT"
echo "run,name,mode,size,MBps" > "$THR"
: > "$OUT/errors.log"
run() { local f=$1; shift; local o
        if o=$("$@" 2>>"$OUT/errors.log"); then echo "$r,$o" >> "$f"; else echo "FAILED: $*" | tee -a "$OUT/errors.log" >&2; fi; }

{ echo "# env"; uname -a; lscpu | grep -E "Model name|^CPU\(s\)|L2|L3"; free -h | head -2; gcc --version | head -1
  echo "REPEAT=$REPEAT ITERS=$ITERS TOTAL_MIB=$TOTAL_MIB SAME_CORE=${IPC_SAME_CORE:-0}"; } > "$OUT/environment.txt"

SLOW=$((ITERS/50))
for r in $(seq "$REPEAT"); do
  echo "== run $r/$REPEAT" >&2
  for s in 8 64 512 4096; do
    for k in pipe fifo socketpair unix tcp; do run "$LAT" ./stream_bench $k lat $s "$ITERS"; done
    run "$LAT" ./mq_bench lat $s $((ITERS/4))
    for sy in spin sem; do
      run "$LAT" ./shm_bench lat $s "$ITERS" $sy
      for v in anon anon_populate file; do run "$LAT" ./mmap_bench $v lat $s "$ITERS" $sy; done
    done
    run "$LAT" ./mmap_bench file_msync_async lat $s "$SLOW" spin
    run "$LAT" ./mmap_bench file_msync_sync  lat $s "$SLOW" spin
  done
  for b in 4096 65536 1048576; do
    for k in pipe fifo socketpair unix tcp; do run "$THR" ./stream_bench $k thr $b "$TOTAL_MIB"; done
    run "$THR" ./shm_bench thr $b "$TOTAL_MIB"
    for v in anon anon_populate file; do run "$THR" ./mmap_bench $v thr $b "$TOTAL_MIB"; done
    for v in write write_fsync read read_rand mmap_read mmap_seq mmap_rand mmap_write mmap_write_msync; do
      run "$THR" ./file_bench $v $b "$TOTAL_MIB"; done
  done
  run "$THR" ./mq_bench thr 4096 $((TOTAL_MIB/4))
  run "$THR" ./mq_bench thr 8192 $((TOTAL_MIB/4))
done

./capacity > "$OUT/capacity.csv" 2>>"$OUT/errors.log"
echo "name,mode,pages,mmap_us,first_touch_ns_per_page,second_touch_ns_per_page,minflt_first_pass" > "$OUT/pagefault.csv"
for v in anon anon_populate anon_hugetlb file file_populate file_private; do ./mmap_bench $v pf 256 >> "$OUT/pagefault.csv" 2>>"$OUT/errors.log"; done
for v in anon file anon_private file_private; do ./mmap_bench $v demo; done > "$OUT/mmap_demo.txt"
echo "Готово: $OUT/ (див. errors.log для невдалих запусків, напр. hugetlb/O_DIRECT)" >&2
