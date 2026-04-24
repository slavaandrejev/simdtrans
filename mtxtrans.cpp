#include <array>
#include <cstddef>
#include <new>
#include <random>

#include <fmt/printf.h>

#include <libpfc.h>

extern "C" {
    void simdtrans(void *in);
}

auto constexpr N = 16;

constexpr PFC_CFG raw_msr(uint8_t event, uint8_t umask) {
    return (1ULL << 22) | (1ULL << 16) | (umask << 8) | event;
}

void print_mtx(const std::array<uint8_t, N * N> &mtx) {
    for (auto r = 0; N > r; ++r) {
        for (auto c = 0; N > c; ++c) {
            if (0 != c) {
                fmt::print(" ");
            }
            fmt::print("{:02x}", mtx[r * 16 + c]);
        }
        fmt::print("\n");
    }
}

void naive_trans(uint8_t *mtx) {
    for (auto r = 0; N > r; ++r) {
        for (auto c = 0; r > c; ++c) {
            std::swap(mtx[r * N + c], mtx[c * N + r]);
        }
    }
}

int main(int argc, char* argv[]) {
    alignas(std::hardware_destructive_interference_size) auto mtx = std::array<uint8_t, N * N>{};

    auto rd   = std::random_device{};
    auto gen  = std::mt19937{rd()};
    auto dist = std::uniform_int_distribution<>(0, 255);
    for (auto r = 0; N > r; ++r) {
        for (auto c = 0; N > c; ++c) {
            mtx[r * N + c] = static_cast<uint8_t>(dist(gen));
        }
    }

    auto mtx2 = mtx;

    print_mtx(mtx);
    fmt::print("-----------------------------------------------\n");
    naive_trans(&mtx[0]);
    print_mtx(mtx);
    fmt::print("-----------------------------------------------\n");

    // Check correctness
    simdtrans(&mtx2[0]);
    for (auto r = 0; N > r; ++r) {
        for (auto c = 0; N > c; ++c) {
            if (mtx[r * N + c] != mtx2[r * N + c]) {
                fmt::print(stderr, "simdtrans is incorrect\n");
                return -1;
            }
        }
    }

    pfcPinThread(10);
    if (0 != pfcInit()) {
        fmt::print("Could not open /sys/module/pfc/* handles; Is module loaded?\n");
        return 1;
    }

    const auto ZERO_CNT = std::array<PFC_CNT, 7>{0, 0, 0, 0, 0, 0, 0};
    auto  CNT           = std::array<PFC_CNT, 7>{0, 0, 0, 0, 0, 0, 0};
    auto  CFG           = std::array<PFC_CFG, 7>{2, 2, 2, 0, 0, 0, 0};

    constexpr uint64_t ITERS = 100000;

    // Counters are specific for Golden Cove. please verify for your architecture with `perf list --details`

    // BATCH 1
    CFG[3] = raw_msr(0xae, 0x01); // uops_issued.any
    CFG[4] = raw_msr(0xc2, 0x02); // uops_retired.slots
    CFG[5] = raw_msr(0xa2, 0x08); // resource_stalls.sb
    CFG[6] = raw_msr(0xB2, 0x01); // uops_dispatched.port_0

    pfcWrCfgs(0, 7, &CFG[0]);
    pfcWrCnts(0, 7, &ZERO_CNT[0]);
    memset(&CNT[0], 0, sizeof(CNT));

    for (auto i = 0; 100 > i; ++i) {
        naive_trans(&mtx[0]);
    }
    PFCSTART(&CNT[0]);
    for(uint64_t k = 0; k < ITERS; ++k) {
        naive_trans(&mtx[0]);
    }
    PFCEND(&CNT[0]);

    fmt::print("instructions                : {}\n", CNT[0] / ITERS);
    fmt::print("core cycles                 : {}\n", CNT[1] / ITERS);
    fmt::print("uops_issued.any             : {}\n", CNT[3] / ITERS);
    fmt::print("uops_retired.slots          : {}\n", CNT[4] / ITERS);
    fmt::print("resource_stalls.sb          : {}\n", CNT[5] / ITERS);
    fmt::print("uops_dispatched.port_0      : {}\n", CNT[6] / ITERS);

    // BATCH 2
    CFG[3] = raw_msr(0xb2, 0x02); // uops_dispatched.port_1
    CFG[4] = raw_msr(0xb2, 0x04); // uops_dispatched.port_2_3_10
    CFG[5] = raw_msr(0xb2, 0x10); // uops_dispatched.port_4_9
    CFG[6] = raw_msr(0xb2, 0x20); // uops_dispatched.port_5_11

    pfcWrCfgs(0, 7, &CFG[0]);
    pfcWrCnts(0, 7, &ZERO_CNT[0]);
    memset(&CNT[0], 0, sizeof(CNT));

    for (auto i = 0; 100 > i; ++i) {
        naive_trans(&mtx[0]);
    }
    PFCSTART(&CNT[0]);
    for(uint64_t k = 0; k < ITERS; ++k) {
        naive_trans(&mtx[0]);
    }
    PFCEND(&CNT[0]);

    fmt::print("uops_dispatched.port_1      : {}\n", CNT[3] / ITERS);
    fmt::print("uops_dispatched.port_2_3_10 : {}\n", CNT[4] / ITERS);
    fmt::print("uops_dispatched.port_4_9    : {}\n", CNT[5] / ITERS);
    fmt::print("uops_dispatched.port_5_11   : {}\n", CNT[6] / ITERS);

    pfcFini();
}
