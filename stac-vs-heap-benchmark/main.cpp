#include <iostream>
#include <vector>
#include <algorithm>
#include <chrono>
#include <string>

struct TestObject
{
    int id;
    double value;
    char padding[32];
};

using Clock = std::chrono::high_resolution_clock;
using ns = std::chrono::nanoseconds;

double get_percentile(const std::vector<double> &sorted_data, double percentile)
{
    size_t n = sorted_data.size();
    size_t index = static_cast<size_t>(n * percentile);
    if (index >= n)
        index = n - 1;

    return sorted_data[index];
}

int64_t current_time_ns()
{
    return std::chrono::duration_cast<ns>(
               Clock::now().time_since_epoch())
        .count();
}

std::vector<double> bench_stack(int iterations)
{
    std::vector<double> timings;
    timings.reserve(iterations);

    // preventing compiler from optimizing obj away
    volatile int sink = 0;

    for (int i = 0; i < iterations; ++i)
    {
        auto start = current_time_ns();
        TestObject obj;
        obj.id = i;
        obj.value = static_cast<double>(i);
        sink = obj.id;

        auto end = current_time_ns();
        timings.push_back(static_cast<double>(end - start));
    }
    return timings;
}

std::vector<double> bench_heap(int iterations, bool include_delete)
{
    std::vector<double> timings;
    timings.reserve(iterations);

    volatile int sink = 0;

    for (int i = 0; i < iterations; ++i)
    {
        auto start = current_time_ns();
        TestObject *obj = new TestObject();
        obj->id = i;
        obj->value = static_cast<double>(i);
        sink = obj->id;

        // consider both alloc and dealloc
        if (include_delete)
        {
            delete obj;
        }

        auto end = current_time_ns();
        timings.push_back(static_cast<double>(end - start));

        // consider only alloc
        if (!include_delete)
        {
            delete obj;
        }
    }
    return timings;
}

void print_stats(const std::string &label, std::vector<double> data)
{
    std::sort(data.begin(), data.end());

    double p50 = get_percentile(data, 0.50);
    double p95 = get_percentile(data, 0.95);
    double p99 = get_percentile(data, 0.99);

    std::cout << label << " results ==================\n";
    std::cout << "p50\t: \t" << p50 << "ns\n";
    std::cout << "p95\t: \t" << p95 << "ns\n";
    std::cout << "p99\t: \t" << p99 << "ns\n";
}

int main(int argc, char *argv[])
{
    constexpr int ITERATIONS = 1'000'000;
    constexpr int WARMUP = 100000;

    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << "[stack|heap_alloc|heap_full]\n";
        return 1;
    }

    std::string mode = argv[1];
    std::cout << "===========================================\n";
    std::cout << "Stack vs Heap Benchmark                    \n";
    std::cout << "Object size: " << sizeof(TestObject) << "bytes\n";
    std::cout << "Iterations:  " << ITERATIONS << "\n";
    std::cout << "Mode:       " << mode << "\n";

    if (mode == "stack")
    {
        bench_stack(WARMUP);
        auto stack_bench = bench_stack(ITERATIONS);
        print_stats("Stack Bench", stack_bench);
    }
    else if (mode == "heap_alloc")
    {
        bench_heap(WARMUP, false);
        auto heap_alloc_only = bench_heap(ITERATIONS, false);
        print_stats("Heap Allocation Only", heap_alloc_only);
    }
    else if (mode == "heap_full")
    {
        bench_heap(WARMUP, true);
        auto heap_full_lifecycle = bench_heap(ITERATIONS, true);
        print_stats("Heap Full Lifecycle", heap_full_lifecycle);
    }
    else
    {
        std::cerr << "Unknown mode: " << mode << "\n";
        return 1;
    }
    return 0;
}