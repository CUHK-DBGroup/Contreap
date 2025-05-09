# Contreap

Contreap is a high-performance, concurrent, persistent data structure library and benchmarking suite, designed for evaluating and comparing different in-memory database backends. It supports efficient range queries, insertions, and deletions, and is built with extensibility and benchmarking in mind.

## Features

- **Multiple Database Backends**:  
  - `contreap`: A concurrent, batched, persistent treap-based data structure.
  - `sequential`: A sequential, persistent treap for baseline comparison.
  - `pam`: Batch-based backend using the Parallel Augmented Map (PAM) library.

- **Efficient Range Queries**:  
  All backends support fast range queries, insertions, and deletions.

- **Concurrency**:  
  Designed to leverage multi-core systems with configurable server and client threads.

- **Batch Processing**:  
  Supports batched operations for improved throughput.

- **Benchmarking Suite**:  
  Includes scripts and tools for running and measuring performance on various workloads.

## Directory Structure

- `main.cpp` — Entry point for running benchmarks and experiments.
- `db/` — Core database backends (`contreap`, `sequential`, `batch`).
- `lib/` — Supporting libraries (e.g., treap, PAM, parallelism).
- `utils/` — Utility headers for logging and timing.
- `test/` — Example and test programs (e.g., query latency, shopping system simulation).
- `ycsbc/` — YCSB-C, a C++ port of the Yahoo! Cloud Serving Benchmark, for standardized workload generation.
- `workload/` — Generated workload files.

## Building

Requirements:
- C++17 or later
- [TBB (Threading Building Blocks)](https://www.threadingbuildingblocks.org) (for YCSB-C)
- Standard build tools (make, g++/clang++)

To build the main project:
```sh
make
```

To build YCSB-C (for benchmarking):
```sh
cd ycsbc
make
```

## Usage

Run the main benchmark:
```sh
./main <method> <workload> [options]
```
- `<method>`: `contreap`, `sequential`, or `pam`
- `<workload>`: Path to a binary workload file
- Options:
  - `-threads n`: Number of server-side threads (default: number of CPU cores)
  - `-clients n`: Number of client threads (default: 1)
  - `-batchsize b`: Batch size for batched backends (default: 1000)

Example:
```sh
./main contreap workloads/workloadA.bin -threads 8 -clients 2 -batchsize 500
```

## Workloads

- Workloads are binary files containing initial records and a sequence of transactions.
- You can generate workloads using by:
  ```sh
  ycsbc -db export -P <workload_name>
  ```
- The location of generated workload:
  `workload/<workload_name>.spec`
- For the fomat of workload files, please refer to [YCSB-C](https://github.com/brianfrankcooper/YCSB/wiki).

## Testing

Example test programs are provided in the `test/` directory:
- `query_latency.cpp`: Measures query latency for different data structures.
- `shopping_system.cpp`: Simulates a shopping system workload.

Compile and run as needed:
```sh
g++ -std=c++17 -O2 test/query_latency.cpp -o query_latency
./query_latency
```

## Extending

To add a new backend, implement the `DB::Interface` class (see `db/include/interface.hpp`) and add it to the main switch in `main.cpp`.

## References

- [YCSB-C](https://github.com/brianfrankcooper/YCSB/wiki): Standardized cloud serving benchmark.
- [PAM (Parallel Augmented Maps)](https://github.com/cmuparlay/pam): Used for the `pam` backend.

## License

GNU General Public License v3.0