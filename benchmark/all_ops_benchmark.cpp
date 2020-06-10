#include <string>
#include <random>

#include <benchmark/benchmark.h>

#include "benchmark.hpp"
#include "fixtures/common_fixture.hpp"
#include "fixtures/pmem_kv_fixture.hpp"
#include "fixtures/viper_fixture.hpp"
#include "fixtures/rocksdb_fixture.hpp"
#include "fixtures/faster_fixture.hpp"
#include "fixtures/dram_map_fixture.hpp"


#define GENERAL_ARGS \
            ->Repetitions(NUM_REPETITIONS) \
            ->Iterations(1) \
            ->Unit(BM_TIME_UNIT) \
            ->UseRealTime() \
            ->ThreadRange(1, NUM_MAX_THREADS) \
            ->Threads(9)->Threads(18)->Threads(24)

#define DEFINE_BM(fixture, method) \
            BENCHMARK_DEFINE_F(fixture, method)(benchmark::State& state) { \
                bm_##method(state, *this); \
            } \
            BENCHMARK_REGISTER_F(fixture, method) GENERAL_ARGS

#define BM_INSERT(fixture) DEFINE_BM(fixture, insert)->Args({NUM_PREFILLS, NUM_INSERTS})
#define BM_UPDATE(fixture) DEFINE_BM(fixture, update)->Args({NUM_PREFILLS, NUM_UPDATES})
#define BM_FIND(fixture)   DEFINE_BM(fixture, get)->Args({NUM_PREFILLS, NUM_FINDS})
#define BM_DELETE(fixture) DEFINE_BM(fixture, delete)->Args({NUM_PREFILLS, NUM_DELETES})

#define ALL_BMS(fixture) \
            BM_FIND(fixture); \
            BM_INSERT(fixture); \
            BM_UPDATE(fixture); \
            BM_DELETE(fixture)

using namespace viper::kv_bm;

void bm_insert(benchmark::State& state, BaseFixture& fixture) {
    const uint64_t num_total_prefill = state.range(0);
    const uint64_t num_total_inserts = state.range(1);

    set_cpu_affinity(state.thread_index);

    if (is_init_thread(state)) {
        fixture.InitMap(num_total_prefill);
    }

    const uint64_t num_inserts_per_thread = (num_total_inserts / state.threads) + 1;
    const uint64_t start_idx = (state.thread_index * num_inserts_per_thread) + num_total_prefill;
    const uint64_t end_idx = std::min(start_idx + num_inserts_per_thread, num_total_prefill + num_total_inserts);

    for (auto _ : state) {
        fixture.setup_and_insert(start_idx, end_idx);
    }

    state.SetItemsProcessed(num_inserts_per_thread);

    if (is_init_thread(state)) {
        fixture.DeInitMap();
    }

}

void bm_update(benchmark::State& state, BaseFixture& fixture) {
    const uint64_t num_total_prefill = state.range(0);
    const uint64_t num_total_updates = state.range(1);

    set_cpu_affinity(state.thread_index);

    if (is_init_thread(state)) {
        fixture.InitMap(num_total_prefill);
    }

    const uint64_t num_updates_per_thread = num_total_updates / state.threads;
    const uint64_t start_idx = state.thread_index * num_updates_per_thread;
    const uint64_t end_idx = start_idx + num_updates_per_thread;

    for (auto _ : state) {
        // TODO: change to update!
        fixture.insert_empty(start_idx, end_idx);
    }

    state.SetItemsProcessed(num_updates_per_thread);

    if (is_init_thread(state)) {
        fixture.DeInitMap();
    }
}

void bm_get(benchmark::State& state, BaseFixture& fixture) {
    const uint64_t num_total_prefills = state.range(0);
    const uint64_t num_total_finds = state.range(1);

    set_cpu_affinity(state.thread_index);

    if (is_init_thread(state)) {
        fixture.InitMap(num_total_prefills, /*re_init=*/false);
    }

    const uint64_t num_finds_per_thread = (num_total_finds / state.threads) + 1;
    const uint64_t start_idx = state.thread_index * num_finds_per_thread;
    const uint64_t end_idx = std::min(start_idx + num_finds_per_thread, num_total_finds);

    uint64_t found_counter = 0;
    for (auto _ : state) {
        found_counter = fixture.setup_and_find(start_idx, end_idx);
    }

    state.SetItemsProcessed(num_finds_per_thread);

    if (is_init_thread(state)) {
        fixture.DeInitMap();
    }

    BaseFixture::log_find_count(state, found_counter, end_idx - start_idx);
}

void bm_delete(benchmark::State& state, BaseFixture& fixture) {
    const uint64_t num_total_prefills = state.range(0);
    const uint64_t num_total_deletes = state.range(1);

    set_cpu_affinity(state.thread_index);

    if (is_init_thread(state)) {
        fixture.InitMap(num_total_prefills);
    }

    const uint64_t num_deletes_per_thread = (num_total_deletes / state.threads) + 1;
    const uint64_t start_idx = state.thread_index * num_deletes_per_thread;
    const uint64_t end_idx = std::min(start_idx + num_deletes_per_thread, num_total_deletes);

    uint64_t found_counter = 0;
    for (auto _ : state) {
        found_counter = fixture.setup_and_delete(start_idx, end_idx);
    }

    state.SetItemsProcessed(num_deletes_per_thread);

    if (is_init_thread(state)) {
        fixture.DeInitMap();
    }

    BaseFixture::log_find_count(state, found_counter, end_idx - start_idx);
}

ALL_BMS(PmemKVFixture);
ALL_BMS(ViperFixture);
//ALL_BMS(FasterFixture);
ALL_BMS(DiskRocksDbFixture);
ALL_BMS(PmemRocksDbFixture);
ALL_BMS(DramMapFixture);

int main(int argc, char** argv) {
    std::string exec_name = argv[0];
    const std::string arg = get_output_file("all_ops/all_ops");
    return bm_main({exec_name, arg});
}