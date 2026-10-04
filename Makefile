CC      = gcc
CFLAGS  = -O2 -Wall -Wextra -std=gnu11 -Wno-unused-function -Wno-stringop-truncation -Wno-misleading-indentation
LDLIBS  = -lrt -lpthread
BINS    = stream_bench mq_bench shm_bench mmap_bench file_bench capacity

all: $(BINS)
%: %.c common.h shared_core.h
	$(CC) $(CFLAGS) $< -o $@ $(LDLIBS)
clean:
	rm -f $(BINS) *.dat errors.log
.PHONY: all clean
