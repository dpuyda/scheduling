#include <benchmark/benchmark.h>

#include <taskflow/algorithm/for_each.hpp>
#include <taskflow/taskflow.hpp>

namespace taskflow::benchmarks::fibonacci {
int Fibonacci(const int n, tf::Subflow& sbf) {
  if (n < 2) {
    return 1;
  }
  int res1, res2;
  sbf.emplace(
      [&res1, n](tf::Subflow& sbf_n_1) { res1 = Fibonacci(n - 1, sbf_n_1); });
  sbf.emplace(
      [&res2, n](tf::Subflow& sbf_n_2) { res2 = Fibonacci(n - 2, sbf_n_2); });
  sbf.join();
  return res1 + res2;
}

void Benchmark(benchmark::State& state) {
  for (auto _ : state) {
    const auto n = static_cast<int>(state.range(0));
    int res;
    tf::Executor executor;
    tf::Taskflow taskflow;
    taskflow.emplace([&res, n](tf::Subflow& sbf) { res = Fibonacci(n, sbf); });
    executor.run(taskflow).wait();
    benchmark::ClobberMemory();
  }
}
}  // namespace taskflow::benchmarks::fibonacci

namespace taskflow::benchmarks::linear_chain {
void LinearChain(const int length) {
  tf::Executor executor;
  tf::Taskflow taskflow;
  std::vector<tf::Task> tasks(length);
  auto counter = 0;

  for (auto i = 0; i < length; ++i) {
    tasks[i] = taskflow.emplace([&] { counter++; });
  }

  taskflow.linearize(tasks);
  executor.run(taskflow).get();
}

void Benchmark(benchmark::State& state) {
  for (auto _ : state) {
    const auto length = static_cast<int>(state.range(0));
    LinearChain(length);
    benchmark::ClobberMemory();
  }
}
}  // namespace taskflow::benchmarks::linear_chain

namespace taskflow::benchmarks::matrix_multiplication {
void MatrixMultiplication(const int n, std::vector<std::vector<int>>& a,
                          std::vector<std::vector<int>>& b,
                          std::vector<std::vector<int>>& c) {
  tf::Executor executor;
  tf::Taskflow taskflow;

  auto init_a = taskflow.for_each_index(0, n, 1, [&](const int i) {
    for (int j = 0; j < n; ++j) {
      a[i][j] = i + j;
    }
  });

  auto init_b = taskflow.for_each_index(0, n, 1, [&](const int i) {
    for (int j = 0; j < n; ++j) {
      b[i][j] = i * j;
    }
  });

  auto init_c = taskflow.for_each_index(0, n, 1, [&](const int i) {
    for (int j = 0; j < n; ++j) {
      c[i][j] = 0;
    }
  });

  auto comp_c = taskflow.for_each_index(0, n, 1, [&](const int i) {
    for (int j = 0; j < n; ++j) {
      for (int k = 0; k < n; ++k) {
        c[i][j] += a[i][k] * b[k][j];
      }
    }
  });

  comp_c.succeed(init_a, init_b, init_c);
  executor.run(taskflow).get();
}

void Benchmark(benchmark::State& state) {
  for (auto _ : state) {
    const auto n = static_cast<int>(state.range(0));
    std::vector a(n, std::vector<int>(n));
    std::vector b(n, std::vector<int>(n));
    std::vector c(n, std::vector<int>(n));
    MatrixMultiplication(n, a, b, c);
    benchmark::ClobberMemory();
  }
}
}  // namespace taskflow::benchmarks::matrix_multiplication

BENCHMARK(taskflow::benchmarks::fibonacci::Benchmark)
    ->Name("taskflow/fibonacci")
    ->DenseRange(25, 35)
    ->Unit(benchmark::kMillisecond);

BENCHMARK(taskflow::benchmarks::linear_chain::Benchmark)
    ->Name("taskflow/linear_chain")
    ->RangeMultiplier(2)
    ->Range(1 << 20, 1 << 25)
    ->Unit(benchmark::kMillisecond);

BENCHMARK(taskflow::benchmarks::matrix_multiplication::Benchmark)
    ->Name("taskflow/matrix_multiplication")
    ->RangeMultiplier(2)
    ->Range(128, 2048)
    ->Unit(benchmark::kMillisecond);
