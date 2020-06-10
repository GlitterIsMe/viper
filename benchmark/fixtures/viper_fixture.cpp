#include "viper_fixture.hpp"

namespace viper::kv_bm {

void ViperFixture::InitMap(uint64_t num_prefill_inserts, const bool re_init) {
    if (viper_initialized_ && !re_init) {
        return;
    }

    return InitMap(num_prefill_inserts, ViperConfig{});
}

void ViperFixture::InitMap(uint64_t num_prefill_inserts, ViperConfig v_config) {
    cpu_set_t cpuset_before;
    pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset_before);
    set_cpu_affinity();

    pool_file_ = VIPER_POOL_FILE;
    const size_t expected_size = MAX_DATA_SIZE * (sizeof(KeyType) + sizeof(ValueType));
    const size_t size_to_zero = ONE_GB * (std::ceil(expected_size / ONE_GB) + 5);
    zero_block_device(pool_file_, size_to_zero);

    viper_ = ViperT::create(pool_file_, BM_POOL_SIZE, v_config);
    prefill(num_prefill_inserts);

    set_cpu_affinity(CPU_ISSET(0, &cpuset_before) ? 0 : 1);
    viper_initialized_ = true;
}

void ViperFixture::DeInitMap() {
    BaseFixture::DeInitMap();
    viper_ = nullptr;
    viper_initialized_ = false;
}

void ViperFixture::insert_empty(uint64_t start_idx, uint64_t end_idx) {
    auto v_client = viper_->get_client();
    for (uint64_t key = start_idx; key < end_idx; ++key) {
        const ValueType value{key};
        v_client.put(key, value);
    }
}

void ViperFixture::setup_and_insert(uint64_t start_idx, uint64_t end_idx) {
    insert_empty(start_idx, end_idx);
}

uint64_t ViperFixture::setup_and_find(uint64_t start_idx, uint64_t end_idx) {
    const auto v_client = viper_->get_const_client();
    uint64_t found_counter = 0;
    for (uint64_t key = start_idx; key < end_idx; ++key) {
        ViperT::ConstAccessor result;
        const bool found = v_client.get(key, result);
        found_counter += found && (result->data[0] == key);
    }
    return found_counter;
}

uint64_t ViperFixture::setup_and_delete(uint64_t start_idx, uint64_t end_idx) {
    auto v_client = viper_->get_client();
    uint64_t delete_counter = 0;
    for (uint64_t key = start_idx; key < end_idx; ++key) {
        delete_counter += v_client.remove(key);
    }
    return delete_counter;
}

}  // namespace viper::kv_bm