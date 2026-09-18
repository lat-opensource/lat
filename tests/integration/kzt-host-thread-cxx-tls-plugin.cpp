// SPDX-License-Identifier: GPL-2.0-or-later

static int constructor_count;
static int destructor_count;

class ThreadGuard {
public:
    ThreadGuard() : value(0x4567)
    {
        __atomic_add_fetch(&constructor_count, 1, __ATOMIC_RELAXED);
    }

    ~ThreadGuard()
    {
        __atomic_add_fetch(&destructor_count, 1, __ATOMIC_RELAXED);
    }

    int value;
};

static thread_local ThreadGuard guard;

extern "C" int kzt_host_thread_cxx_tls_check(void)
{
    if (guard.value < 0x4567 || guard.value > 0x4667) {
        return 96;
    }
    ++guard.value;
    return 0;
}

extern "C" int kzt_host_thread_cxx_tls_counts(
    int *constructors, int *destructors)
{
    if (!constructors || !destructors) {
        return -1;
    }
    *constructors = __atomic_load_n(
        &constructor_count, __ATOMIC_ACQUIRE);
    *destructors = __atomic_load_n(
        &destructor_count, __ATOMIC_ACQUIRE);
    return 0;
}
