#include <random> 
#include <vector> 
#include <iostream>
#include <array>
#include <bitset>

#include <benchmark/benchmark.h>

#include "card_ops.h"

constexpr auto COMBOS = 133784560;
constexpr unsigned long ACE_MASK = 0x2000200020002000;

std::vector<unsigned long> create_data() {
    std::vector<unsigned long> v(0);
    v.reserve(COMBOS);
    
    for(unsigned long i = 1UL << 51; i > 0; i >>= 1) 
        for(unsigned long j = i >> 1; j > 0; j >>= 1) 
            for(unsigned long k = j >> 1; k > 0; k >>= 1)
                for(unsigned long l = k >> 1; l > 0; l >>= 1)
                    for(unsigned long m = l >> 1; m > 0; m >>= 1)
                        for(unsigned long n = m >> 1; n > 0; n >>= 1) 
                            for(unsigned long o = n >> 1; o > 0; o >>= 1)
                                v.push_back(i | j | k | l | m | n | o);  

    // convert packed 52bit into internal 64bit
    for(auto& x: v) {
        auto tmp = 0UL;
        for(int i = 0; i < 4; ++i) {
            tmp |= ((x >> (13 * i))  & 0x1fffUL) << (16 * i);
        }
        x = (tmp << 1) | ((tmp & ACE_MASK) >> 13);
    }

    return v;
}


static void BM_evaluate(benchmark::State& state) {
    int i = 0;
    auto data = create_data();
    for (auto _ : state) {
        if(i >= COMBOS) {
            i = 0;
        }
        auto cards = data[i++];
        benchmark::DoNotOptimize(evaluate_hand(cards));
        benchmark::ClobberMemory();
    }
}

static void BM_get(benchmark::State& state) {
    int i = 0;
    auto data = create_data();
    for (auto _ : state) {
        if(i >= COMBOS) {
            i = 0;
        }
        auto cards = data[i++];
        benchmark::DoNotOptimize(cards);
        benchmark::ClobberMemory();
    }
}

static void BM_get_rand(benchmark::State& state) {
    std::mt19937 g{std::random_device{}()};
    std::uniform_int_distribution<size_t> d{0,COMBOS};

    std::vector<unsigned long> data(COMBOS, 0);
    
    for(auto _ : state) {
        auto a = data[d(g)];
        benchmark::DoNotOptimize(a);
        benchmark::ClobberMemory();
    }
}

static void BM_rand(benchmark::State& state) {
    std::mt19937 g{std::random_device{}()};
    std::uniform_int_distribution<size_t> d{0,COMBOS};

    for(auto _ : state) {
        auto a = d(g);
        benchmark::DoNotOptimize(a);
        benchmark::ClobberMemory();
    }
}
    
BENCHMARK(BM_evaluate);
BENCHMARK(BM_get);
BENCHMARK(BM_get_rand);
BENCHMARK(BM_rand);
BENCHMARK_MAIN();
