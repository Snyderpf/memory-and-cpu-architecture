# Stack vs. Heap Allocation Benchmark

Benchmarks the latency and CPU cache behavior of stack allocation versus heap allocation for fixed-size objects using percentile-based timing ($P_{50}$, $P_{95}$, $P_{99}$) and `perf stat` cache-miss counters.

## Motivation 

Every latency-sensitive decision starts with knowing which memory region you are touching. 

* **Stack allocation** is a deterministic pointer bump (`sub rsp`).
* **Heap allocation** invokes a memory allocator that can introduce variable, unpredictable work and cache invalidation.

This benchmark measures that difference directly under real CPU conditions rather than relying on theoretical assumptions.

## Key Takeaways
- **Stack overhead is flat:** $P_{50}$ to $P_{99}$ latency stays virtually constant (pure pointer adjustment).
- **Heap has tail risk:** Allocation incurs noticeable $P_{95}$ and $P_{99}$ spikes due to internal allocator operations (freelist lookups/bin refills).
- **Measurement discipline matters:** High-frequency clock overhead can easily hide sub-nanosecond signals at $P_{50}$ without proper batching or cycle counting.


## Setup
- Object under test: a 48-byte struct (`int` + `double` + padding)
- 1,000,000 iterations per benchmark, with a 100,000-iteration warm-up pass
- Compiled with `g++ -std=c++17 -O2 -Wall -Wextra`
- Timing via `std::chrono::high_resolution_clock`
- Cache statistics via `perf stat -e cache-misses,cache-references`


`-O2` was used rather than `-O3` to avoid auto-vectorization effects on
these simple loops — `-O2` gives more predictable, closer-to-literal code
generation, which matters when isolating the cost of a single operation.

**Test environment:** Ubuntu, bare metal, x86_64, GCC. Not run on an
isolated or pinned core, so some run-to-run variance in tail latency
should be expected from ordinary OS scheduling noise.

## Build & run
```bash
mkdir build && cd build
cmake ..
make
./main [stack|heap_alloc|heap_full]
```

Or run all three benchmarks with cache-miss stats in one pass:
```bash
./run_benchmarks.sh
```


## Results

### Timing (p50 / p95 / p99, nanoseconds)

| Benchmark              | p50 | p95 | p99 |
|-------------------------|-----|-----|-----|
| Stack                   | 40  | 41  | ~42 |
| Heap (alloc only)       | 40  | 50  | ~55 |
| Heap (alloc + dealloc)  | 40  | 51  | ~60 |

(Averaged across 5 consecutive runs on the same machine.)

p50 is nearly identical across all three — this is likely `chrono`'s own
per-call overhead dominating at the median, not true allocation cost. The
real signal is in the tail: stack stays essentially flat from p50 to p99,
while both heap variants show a clear upward tail, consistent with an
allocator occasionally doing real work (freelist search, bin refill) that
a stack pointer bump never does.


### Cache-miss rate (`perf stat`, isolated per-benchmark runs)

| Benchmark              | Run 1 | Run 2 | Run 3 |
|-------------------------|-------|-------|-------|
| Stack                   | 6.45% | 7.26% | 6.57% |
| Heap (alloc only)       | 8.03% | 6.99% | 7.20% |
| Heap (alloc + dealloc)  | 7.04% | 6.96% | 6.66% |



No consistent ordering emerged across repeated runs — each benchmark's
miss rate fell within roughly 6.5–8.0% regardless of allocation strategy,
and which benchmark had the highest or lowest rate changed between runs.
This suggests that at this object size (48 bytes) and count (1M), whole-
process `perf stat` cache-miss counts are dominated by measurement noise
(OS scheduling, fixed program startup cost) rather than by a genuine,
repeatable allocator-driven cache-locality difference. A cleaner signal
would likely require `perf record` with source-level annotation to isolate
the hot loop specifically, or a larger object size to create more cache
pressure.

The contrast is itself worth noting: **timing showed a clear, repeatable
signal** (stack ≤ heap-alloc ≤ heap-full, every run); **cache-miss rate did
not**.

### Why alloc+dealloc sometimes looked cheaper than alloc-only

An early result showed the full allocate+free cycle costing *less* than
allocation alone — counterintuitive at first glance. The likely cause:
immediately calling `delete` returns memory to the allocator's thread-local
cache (tcache), so the very next `new` often reuses that exact same,
already L1-resident block. Allocation-only, by contrast, never frees
anything mid-run, so the allocator must keep handing out fresh addresses
across the full ~48MB working set (1M × 48 bytes) — well past L1/L2 cache
capacity — producing more genuine cache pressure.


## Methodology notes & pitfalls caught along the way

- **Missing sort before percentile calculation.** `get_percentile` expects
  pre-sorted data; an early version called it on an unsorted vector,
  producing a result that happened to look plausible but was meaningless.
  Confirmed by comparing sorted vs unsorted output on the same data.
- **Insufficient warm-up.** A 10,000-iteration warm-up left heap's p95/p99
  elevated relative to steady state; increasing to 100,000 (and to
  1,000,000 for heap specifically) flattened the distribution. Heap needed
  a longer warm-up than stack — plausibly because the allocator has
  internal state (freelists, bins) that takes longer to reach steady
  behavior than a stack pointer does.
- **Warm-up contamination in isolated runs.** When isolating a single
  benchmark for `perf stat`, an early version still unconditionally warmed
  up all three benchmarks regardless of which one was being measured,
  silently blending workloads and hiding the real per-benchmark
  difference. Fixed by scoping warm-up to match exactly what's being run
  (now enforced structurally via command-line argument selection).
- **p99 run-to-run variance.** At 1M samples, p99 reflects roughly the
  10,000th-worst sample — rare scheduling events (OS interrupts, context
  switches) can shift it several nanoseconds between runs even when p50
  is stable. Multiple runs were averaged rather than trusting a single
  run's tail value.



## Known limitations

- Timed with `std::chrono::high_resolution_clock` rather than a serializing
  cycle counter. `chrono`'s own call overhead (likely 15–40ns) is comparable
  to or larger than a single stack allocation's true cost, which is why p50
  looks similar across benchmarks — a `rdtscp`-based timer or batch timing
  (amortizing the clock call over many operations) would isolate the signal
  more precisely.
- `perf stat` measures the whole process, not just the benchmark loop, so
  cache-miss counts include CRT init and teardown alongside the measured
  work.
- Object size (48 bytes) and count (1M) may not be large enough to force
  a clearly repeatable cache-locality difference between contiguous stack
  allocation and heap allocation via glibc's default allocator.