# Bare-Metal AVX2 Matrix Transpose (16x16 Byte)

This repository contains a bare-metal AVX2 implementation of a 16x16 byte matrix
transposition.

The algorithm shows roughly 2x performance gain compared to a naive scalar
baseline implementation, even though the entire working set fits in L1 cache.
Hardware profiling reveals that when an algorithm is dominated by memory
operations, the load/store unit throughput becomes the bottleneck, regardless
of cache residency.

This AVX2 implementation bypasses the load/store port limitations by absorbing
the entire matrix into 256-bit registers. Executing the transposition purely
through vector shuffle units eliminates memory stalls and maximizes throughput
to the theoretical limit of the silicon.

## Highlights

 * 99 core cycles, bounded entirely by the vector shuffle execution units (Ports 5 and 11).
 * 8 loads and 8 stores, with zero Store Buffer stalls.
 * Perfect ratio between instructions and uops. 229 instructions, 229 uops
   issued, 229 uops retired. Every instruction is a single uop.

## Performance Telemetry (Intel i9-12900H P-Core)

| Counter | SIMD | Naive (Clang) | Naive (GCC) |
|---------|------:|------:|------:|
| instructions | 229 | 482 | 748 |
| core cycles | 99 | 216 | 217 |
| uops_issued.any | 229 | 481 | 748 |
| uops_retired.slots | 229 | 481 | 748 |
| resource_stalls.sb | 0 | 135 | 92
| uops_dispatched.port_0 | 27 | 0 | 40 |
| uops_dispatched.port_1 | 68 | 0 | 30 |
| uops_dispatched.port_2_3_10 | 9 | 240 | 241 |
| uops_dispatched.port_4_9 | 9 | 240 | 242 |
| uops_dispatched.port_5_11 | 87 | 0 | 66 |

## Contents

 * `simdtrans.s` The heavily commented AVX2 assembly implementation, including register lane-mapping matrices.
 * `mtxtrans.cpp` The libpfc C++ benchmarking harness configured for Golden Cove PMU events.

## Brief Algorithm Description

Detailed description can be found in the assembler code. Here is a quick overview

 * `transpose2x2x1` (byte pairs): Load four rows at a time, rearrange them into
   lane pairs using `vinserti128`/`vextracti128`, then use
   `vpunpcklbw`/`vpunpckhbw` to interleave adjacent bytes. This transposes all
   2×2 sub-blocks of individual bytes. The even and odd results are separated
   with `vpblendw` and packed with `vpackusdw`.
 * `transpose2x2x2` (word pairs): Treats the 2-byte results from level 1 as
   elements and transposes 2×2 blocks of words, effectively completing 4×4 block
   transposes. Uses `vpunpcklwd`/`vpunpckhwd` with blend-and-shuffle sequences
   to separate even and odd dwords.
 * `transpose2x2x4` (dword pairs): Transposes 2×2 blocks of dwords using
   `vpunpckldq`/`vpunpckhdq` with `vpblendd`, completing 8×8 block transposes.

Between each level, `swaphldqw` rearranges the high/low 128-bit lanes across
register pairs so that the rows feeding the next level are properly aligned.

After all three levels, `vpermq` fixes the final lane ordering, and a sequence
of `vinserti128`/`vextracti128`/`vpblendd` assembles the eight output rows from
the eight `YMM` registers for the final store.

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
