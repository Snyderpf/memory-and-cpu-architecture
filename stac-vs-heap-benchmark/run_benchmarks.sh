#!/bin/bash

for mode in stack heap_alloc heap_full; do
    echo "=== $mode ==="
    perf stat -e cache-misses,cache-references ./main $mode
    echo ""
done
