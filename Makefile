# Build seluruh versi di dalam container: make
# Opsi kompiler sama untuk semua versi (variabel kontrol pada proposal).
CC     = gcc
MPICC  = mpicc
CFLAGS = -O2 -fopenmp -std=gnu11 -Wall -Wextra
LDLIBS = -lm

COMMON = src/common.c src/common.h

all: bin/v0 bin/v1 bin/v2 bin/mpi_hello

bin/v0: src/v0_seq.c $(COMMON) | bin
	$(CC) $(CFLAGS) -o $@ src/v0_seq.c src/common.c $(LDLIBS)

bin/v1: src/v1_omp.c $(COMMON) | bin
	$(CC) $(CFLAGS) -o $@ src/v1_omp.c src/common.c $(LDLIBS)

bin/v2: src/v2_hybrid.c $(COMMON) | bin
	$(MPICC) $(CFLAGS) -o $@ src/v2_hybrid.c src/common.c $(LDLIBS)

bin/mpi_hello: tests/mpi_hello.c | bin
	$(MPICC) $(CFLAGS) -o $@ tests/mpi_hello.c

bin:
	mkdir -p bin

clean:
	rm -rf bin

.PHONY: all clean
