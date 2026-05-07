# Bare-Metal AVX2 Matrix Transpose (16x16 Byte)

This repository contains a bare-metal AVX2 implementation of a 16x16 byte matrix
transposition.

The algorithm shows roughly 5x performance gain compared to a naive scalar
baseline implementation, even though the entire working set fits in L1 cache.
Hardware profiling reveals that when an algorithm is dominated by memory
operations, the load/store unit throughput becomes the bottleneck, regardless
of cache residency.

This AVX2 implementation bypasses the load/store port limitations by absorbing
the entire matrix into 256-bit registers. Executing the transposition purely
through vector unpacks eliminates memory stalls and maximizes throughput
to the theoretical limit of the silicon.

## Highlights

 * 44 core cycles, bounded entirely by the vector unpack execution units (ports 1 and 5).
 * 8 loads and 8 stores, with zero Store Buffer stalls.
 * Beats [Zemtsov's SSSE3 baseline](https://pzemtsov.github.io/2014/10/01/how-to-transpose-a-16x16-matrix.html)
   algorithm both in the CPU cycles and the number of uops.

## Performance Telemetry (Intel i9-12900H P-Core)

| Counter | SIMD | [Zemtsov's SSSE3 baseline](https://pzemtsov.github.io/2014/10/01/how-to-transpose-a-16x16-matrix.html) | Naive (Clang) | Naive (GCC) |
|---------|------:|------:|------:|------:|
| instructions | 89 | 144 | 482 | 748 |
| core cycles | $\textcolor{green}{\text{𝟰𝟰}}$ | $\textcolor{red}{\text{𝟰𝟴}}$ | $\textcolor{red}{\text{𝟮𝟭𝟲}}$ | $\textcolor{red}{\text{𝟮𝟭𝟳}}$ |
| uops_issued.any | 89 | 144 | 481 | 748 |
| uops_retired.slots | 89 | 144 | 481 | 748 |
| resource_stalls.sb | $\textcolor{green}{\text{𝟬}}$ | $\textcolor{green}{\text{𝟬}}$ | $\textcolor{red}{\text{𝟭𝟯𝟱}}$ | $\textcolor{red}{\text{𝟵𝟮}}$ |
| uops_dispatched.port_0 | 0 | 0 | 0 | 40 |
| uops_dispatched.port_1 | 32 | 48 | 0 | 30 |
| uops_dispatched.port_2_3_10 | $\textcolor{green}{\text{𝟵}}$ | $\textcolor{red}{\text{𝟮𝟯}}$ | $\textcolor{red}{\text{𝟮𝟰𝟬}}$ | $\textcolor{red}{\text{𝟮𝟰𝟭}}$ |
| uops_dispatched.port_4_9 | $\textcolor{green}{\text{𝟵}}$ | $\textcolor{red}{\text{𝟮𝟮}}$ | $\textcolor{red}{\text{𝟮𝟰𝟬}}$ | $\textcolor{red}{\text{𝟮𝟰𝟮}}$ |
| uops_dispatched.port_5_11 | 32 | 48 | 0 | 66 |

## Contents

 * `simdtrans.s` The AVX2 assembly implementation.
 * `mtxtrans.cpp` The libpfc C++ benchmarking harness configured for Golden Cove PMU events.

## Brief Algorithm Description

The algorithm is a cascade of unpacks. At each stage it interleaves and joins
_n_-byte sized words from 2 registers into _2n_-byte sized words. The trick is
that during the cascade, the algorithm is not trying to enforce a proper matrix
structure of the data. If we did, we would have used some extra swaps. Since all
our swap operations commute, we can postpone structural rearrangements to the
very end. It turns out, this structural rearrangement can be done with a couple
more unpacks.

## libpfc notes

[libpfc](https://github.com/obilaniu/libpfc) on GitHub is a little bit outdated.
If you compile it for a modern kernel, you might want to change `struct
bin_attribute*` to `const struct bin_attribute*` in the kernel module code. And change this
```c
	native_write_msr(addr,
	                 (uint32_t)(newVal >>  0),
	                 (uint32_t)(newVal >> 32));
```
to
```c
	native_write_msr(addr, newVal);
```
Before running the benchmark
```shell
sudo sh -c 'echo 0 > /sys/devices/system/cpu/cpu11/online'
sudo sh -c 'echo 2 > /sys/bus/event_source/devices/cpu_core/rdpmc'
sudo sh -c 'echo 0 > /proc/sys/kernel/nmi_watchdog'
```
The benchmark runs on core 10. You might want to update it in `mtxtrans.cpp`,
along with the line that disables core 11 above, depending on your CPU
architecture.

Also, check the counter codes for your architecture using `perf list --details`.
You may have something different here
```cpp
    // BATCH 1
    CFG[3] = raw_msr(0xae, 0x01); // uops_issued.any
    CFG[4] = raw_msr(0xc2, 0x02); // uops_retired.slots
    CFG[5] = raw_msr(0xa2, 0x08); // resource_stalls.sb
    CFG[6] = raw_msr(0xB2, 0x01); // uops_dispatched.port_0
```
and here
```cpp
    // BATCH 2
    CFG[3] = raw_msr(0xb2, 0x02); // uops_dispatched.port_1
    CFG[4] = raw_msr(0xb2, 0x04); // uops_dispatched.port_2_3_10
    CFG[5] = raw_msr(0xb2, 0x10); // uops_dispatched.port_4_9
    CFG[6] = raw_msr(0xb2, 0x20); // uops_dispatched.port_5_11
```
