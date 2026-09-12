#include <iostream>
#include <vector>
#include <algorithm>
#include <chrono>

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

int main()
{
    constexpr int ITERATIONS = 1'000'000;

    std::vector test = {1.4, 2.1, 3.6, 4.4, 5.2, 3.4, 2.4, 1.9, 1.8};
    std::sort(test.begin(), test.end());
    auto val = get_percentile(test, 0.5);
    auto val_1 = get_percentile(test, 0.95);
    auto val_2 = get_percentile(test, 0.99);
    std::cout << val << std::endl;
    std::cout << val_1 << std::endl;
    std::cout << val_2 << std::endl;

    return 0;
}