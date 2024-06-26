#include <benchmark/benchmark.h>

#include <scheduling/scheduling.hpp>

namespace scheduling::benchmarks::fibonacci {
int Fibonacci(ThreadPool& thread_pool, const int n) {
  if (n < 2) {
    return 1;
  }
  int a, b;
  std::atomic counter{0};
  thread_pool.Submit([&, n] {
    a = Fibonacci(thread_pool, n - 1);
    counter.fetch_add(1);
  });
  thread_pool.Submit([&, n] {
    b = Fibonacci(thread_pool, n - 2);
    counter.fetch_add(1);
  });
  thread_pool.Wait([&] { return counter.load() == 2; });
  return a + b;
}

void Benchmark(benchmark::State& state) {
  const auto n = static_cast<int>(state.range(0));
  for (auto _ : state) {
    ThreadPool thread_pool;
    benchmark::DoNotOptimize(Fibonacci(thread_pool, n));
    benchmark::ClobberMemory();
  }
}
}  // namespace scheduling::benchmarks::fibonacci

namespace scheduling::benchmarks::linear_chain {
void LinearChain(const int length) {
  int counter = 0;
  std::vector<Task> v(length);
  v[0] = Task([&] { ++counter; });
  for (auto i = v.begin(), j = std::next(v.begin()); j != v.end(); ++i, ++j) {
    *j = Task([&] { ++counter; });
    j->Succeed(&*i);
  }
  ThreadPool thread_pool;
  thread_pool.Submit(&v[0]);
}

void Benchmark(benchmark::State& state) {
  const auto length = static_cast<int>(state.range(0));
  for (auto _ : state) {
    LinearChain(length);
    benchmark::ClobberMemory();
  }
}
}  // namespace scheduling::benchmarks::linear_chain

namespace scheduling::benchmarks::matrix_multiplication {
void MatrixMultiplication(const int n, std::vector<std::vector<int>>& a,
                          std::vector<std::vector<int>>& b,
                          std::vector<std::vector<int>>& c) {
  std::vector<Task> tasks;
  tasks.reserve(4 * n + 1);

  tasks.emplace_back();

  for (int i = 0; i < n; ++i) {
    tasks
        .emplace_back([&, i, n] {
          for (int j = 0; j < n; ++j) {
            a[i][j] = i + j;
          }
        })
        .Precede(&tasks[0]);
  }

  for (int i = 0; i < n; ++i) {
    tasks
        .emplace_back([&, i, n] {
          for (int j = 0; j < n; ++j) {
            b[i][j] = i * j;
          }
        })
        .Precede(&tasks[0]);
  }

  for (int i = 0; i < n; ++i) {
    tasks
        .emplace_back([&, i, n] {
          for (int j = 0; j < n; ++j) {
            c[i][j] = 0;
          }
        })
        .Precede(&tasks[0]);
  }

  for (int i = 0; i < n; ++i) {
    tasks
        .emplace_back([&, i, n] {
          for (int j = 0; j < n; ++j) {
            for (int k = 0; k < n; ++k) {
              c[i][j] += a[i][k] * b[k][j];
            }
          }
        })
        .Succeed(&tasks[0]);
  }

  ThreadPool thread_pool;
  thread_pool.Submit(tasks);
}

void Benchmark(benchmark::State& state) {
  const auto n = static_cast<int>(state.range(0));
  std::vector a(n, std::vector<int>(n));
  std::vector b(n, std::vector<int>(n));
  std::vector c(n, std::vector<int>(n));
  for (auto _ : state) {
    MatrixMultiplication(n, a, b, c);
    benchmark::ClobberMemory();
  }
}
}  // namespace scheduling::benchmarks::matrix_multiplication

BENCHMARK(scheduling::benchmarks::fibonacci::Benchmark)
    ->Name("scheduling/fibonacci")
    ->DenseRange(25, 35)
    ->Unit(benchmark::kMillisecond);

BENCHMARK(scheduling::benchmarks::linear_chain::Benchmark)
    ->Name("scheduling/linear_chain")
    ->RangeMultiplier(2)
    ->Range(1 << 20, 1 << 25)
    ->Unit(benchmark::kMillisecond);

BENCHMARK(scheduling::benchmarks::matrix_multiplication::Benchmark)
    ->Name("scheduling/matrix_multiplication")
    ->RangeMultiplier(2)
    ->Range(128, 2048)
    ->Unit(benchmark::kMillisecond);
