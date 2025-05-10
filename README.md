# Contreap

Contreap is a high-performance, concurrent, persistent binary search tree. It supports efficient insertions, deletions, and range queries. Besides, it is built with extensibility and benchmarking in mind.

## Features

- **Multiple Database Backends**:
  - `contreap`: A concurrent persistent treap.
  - `pam`: Batch-based BST using the Parallel Augmented Map (PAM) library.
  - `sequential`: A sequential, persistent treap for baseline comparison.

- **Efficient Range Queries**:
  Contreap support fast range queries, insertions, and deletions.

- **Concurrency**:
  Contreap is designed to leverage multi-core systems to speed up updates while guaranteeing strict order.

- **Benchmarking Suite**:
  Includes scripts and tools for running and measuring performance on various workloads.

## Directory Structure

- `main.cpp` — Entry point for running benchmarks and experiments.
- `db/` — Core database backends (`contreap`, `pam`, `sequential`).
- `lib/` — Supporting libraries (e.g., treap, PAM, parallelism).
- `utils/` — Utility headers for logging and timing.
- `test/` — Test programs (e.g., query latency, shopping system simulation).
- `ycsbc/` — YCSB-C, a C++ port of the Yahoo! Cloud Serving Benchmark, for standardized workload generation.

## Building

Requirements:
- An x86-64(AMD64) platform supporting SIMD and AES-NI
- C++ compiler supporting C++20
- Mimalloc v1.8.4(v2.1.4) or above

To build the main project:
```sh
g++ main.cpp -I. -lpthread -lmimalloc -march=native -msse4 -maes -std=c++20 -O3 -DNDEBUG -o build/main
```

To build YCSB-C, please follow the README document inside the `ycsbc` folder

## Workloads

- Workloads are binary files containing initial records and a sequence of transactions.
- You can generate workloads using:
  ```sh
  ycsbc -db export -P <workload_name>
  ```
- The location of workload config:
  `ycsbc/workload/<workload_name>.spec`
- The location of generated workload:
  `ycsbc/export/<workload_name>.data`
- If the `export` folder does not exist, please create it at first.- For the fomat of workload files, please refer to [YCSB-C](https://github.com/brianfrankcooper/YCSB/wiki).

## Usage

Run the main benchmark:
```sh
./build/main <method> <workload> [options]
```
- `<method>`: `contreap`, `pam` or `sequential`
- `<workload>`: Path to a binary workload file
- Options:
  - `-threads n`: Number of server-side threads (default: number of CPU cores)
  - `-clients n`: Number of client (query) threads (default: 1)
  - `-batchsize b`: Batch size for batched backends (default: 1000)

Example:
```sh
./build/main contreap ycsbc/export/workload_update_rand.data -threads 64 -clients 16
```

## Extending

To add a new backend, implement the `DB::Interface` class (see `db/include/interface.hpp`) and add it to the main switch in `main.cpp`.

## Extra Benchmarking

Extra test programs are provided in the `test/` directory:
- `query_latency.cpp`: Measures query latency for different data structures.
- `shopping_system.cpp`: Simulates a shopping system workload.

Compile and run as needed:
```sh
g++ query_latency.cpp -I. -lpthread -lmimalloc -march=native -msse4 -maes -std=c++20 -O3 -DNDEBUG -o build/query_latency
```

## References

- [YCSB-C](https://github.com/brianfrankcooper/YCSB/wiki): Standardized cloud serving benchmark.
- [Mimalloc](https://https://github.com/microsoft/mimalloc): Used for efficient concurrent memory allocation.
- [MoodyCamel](https://github.com/cameron314/concurrentqueue): A lock-free concurrent queue used for query queueing.
- [PAM (Parallel Augmented Maps)](https://github.com/cmuparlay/pam): Used for the `pam` backend.

## License

GNU General Public License v3.0