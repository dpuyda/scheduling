#include <gtest/gtest.h>
#include <scheduling/scheduling.h>

namespace {
using namespace scheduling;
using namespace testing;

int Fibonacci(ThreadPool& thread_pool, const int n) {
  if (n < 2) {
    return 1;
  }
  int a = 0, b = 0;
  thread_pool.Submit([&, n] { a = Fibonacci(thread_pool, n - 1); });
  thread_pool.Submit([&, n] { b = Fibonacci(thread_pool, n - 2); });
  thread_pool.Wait([&] { return a != 0 && b != 0; });
  return a + b;
}

int LinearChain(const int length) {
  int counter = 0;
  std::vector<Task> v(length);
  v[0] = Task([&] { ++counter; });
  for (auto i = v.begin(), j = std::next(v.begin()); j != v.end(); ++i, ++j) {
    *j = Task([&] { ++counter; });
    j->Succeed(&*i);
  }
  ThreadPool thread_pool;
  thread_pool.Submit(&v[0]);
  thread_pool.Wait();
  return counter;
}

void MatrixMultiplication(const int n, std::vector<std::vector<int>>& a,
                          std::vector<std::vector<int>>& b,
                          std::vector<std::vector<int>>& c) {
  std::vector<Task> tasks;
  tasks.reserve(4 * n + 1);

  tasks.emplace_back();

  for (int i = 0; i < n; ++i) {
    tasks
        .emplace_back([&, i] {
          for (int j = 0; j < n; ++j) {
            a[i][j] = i + j;
          }
        })
        .Precede(&tasks[0]);
  }

  for (int i = 0; i < n; ++i) {
    tasks
        .emplace_back([&, i] {
          for (int j = 0; j < n; ++j) {
            b[i][j] = i * j;
          }
        })
        .Precede(&tasks[0]);
  }

  for (int i = 0; i < n; ++i) {
    tasks
        .emplace_back([&, i] {
          for (int j = 0; j < n; ++j) {
            c[i][j] = 0;
          }
        })
        .Precede(&tasks[0]);
  }

  for (int i = 0; i < n; ++i) {
    tasks
        .emplace_back([&, i] {
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

TEST(ThreadPoolTest, ArithmeticExpression) {
  int a, b, c, d, sum_ab, sum_cd, product;

  std::vector<Task> tasks;
  tasks.reserve(7);

  auto& get_a = tasks.emplace_back([&] {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    a = 1;
  });

  auto& get_b = tasks.emplace_back([&] {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    b = 2;
  });

  auto& get_c = tasks.emplace_back([&] {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    c = 3;
  });

  auto& get_d = tasks.emplace_back([&] {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    d = 4;
  });

  auto& get_sum_ab = tasks.emplace_back([&] {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    sum_ab = a + b;
  });

  auto& get_sum_cd = tasks.emplace_back([&] {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    sum_cd = c + d;
  });

  auto& get_product = tasks.emplace_back([&] {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    product = sum_ab * sum_cd;
  });

  get_sum_ab.Succeed(&get_a, &get_b);
  get_sum_cd.Succeed(&get_c, &get_d);
  get_product.Succeed(&get_sum_ab, &get_sum_cd);

  ThreadPool thread_pool;
  thread_pool.Submit(tasks);
  thread_pool.Wait();

  EXPECT_EQ(a, 1);
  EXPECT_EQ(b, 2);
  EXPECT_EQ(c, 3);
  EXPECT_EQ(d, 4);
  EXPECT_EQ(sum_ab, a + b);
  EXPECT_EQ(sum_cd, c + d);
  EXPECT_EQ(product, (a + b) * (c + d));
}

TEST(ThreadPoolTest, Fibonacci) {
  int a = 1, b = 1;
  ThreadPool thread_pool;
  for (int n = 2; n < 35; ++n) {
    const auto actual = Fibonacci(thread_pool, n);
    const auto expected = a + b;
    EXPECT_EQ(actual, expected) << "n = " << n;
    a = b;
    b = expected;
  }
}

TEST(ThreadPoolTest, LinearChain) {
  for (int n = 1; n < 1000; ++n) {
    const auto actual = LinearChain(n);
    EXPECT_EQ(actual, n) << "n = " << n;
  }
}

TEST(ThreadPoolTest, MatrixMultiplication) {
  for (int n = 1; n < 100; ++n) {
    std::vector a(n, std::vector<int>(n)), b(n, std::vector<int>(n)),
        actual(n, std::vector<int>(n)), expected(n, std::vector<int>(n));
    MatrixMultiplication(n, a, b, actual);
    for (int i = 0; i < n; ++i) {
      for (int j = 0; j < n; ++j) {
        for (int k = 0; k < n; ++k) {
          expected[i][j] += (i + k) * (k * j);
        }
      }
    }
    EXPECT_EQ(actual, expected) << "n = " << n;
  }
}

TEST(ThreadPoolTest, ResubmitGraph) {
  auto counter = 0;
  constexpr auto repeat_count = 1'000'000;
  ThreadPool thread_pool;
  std::vector<Task> tasks(32);
  tasks[0] = Task([&] { ++counter; });
  for (auto i = tasks.begin(), j = std::next(i); j != tasks.end(); ++i, ++j) {
    *j = Task([&] { ++counter; });
    j->Succeed(&*i);
  }
  for (int i = 0; i < repeat_count; ++i) {
    thread_pool.Submit(&tasks[0]);
    thread_pool.Wait();
  }
  EXPECT_EQ(counter, tasks.size() * repeat_count);
}

TEST(ThreadPoolTest, Cancel) {
  {
    SCOPED_TRACE("Cancel a not started task");

    auto completed = false;
    Task task([&] { completed = true; });
    EXPECT_TRUE(task.Cancel());

    ThreadPool thread_pool;
    thread_pool.Submit(&task);
    thread_pool.Wait();
    EXPECT_FALSE(completed);
  }

  {
    SCOPED_TRACE("Cancel successors");

    std::atomic_flag flag_1, flag_2;
    auto count = 0;

    std::vector<Task> tasks;
    tasks.emplace_back([&] {
      flag_1.test_and_set();
      flag_1.notify_one();
      flag_2.wait(false);
    });
    tasks.emplace_back([&] { ++count; });
    tasks.emplace_back([&] { ++count; });
    tasks[0].Precede(&tasks[1]);
    tasks[1].Precede(&tasks[2]);

    ThreadPool thread_pool;
    thread_pool.Submit(tasks);
    flag_1.wait(false);
    EXPECT_TRUE(tasks[1].Cancel());
    flag_2.test_and_set();
    flag_2.notify_one();
    thread_pool.Wait();

    EXPECT_EQ(count, 0);
  }

  {
    SCOPED_TRACE("Cancel a running task");

    std::atomic_flag flag_1, flag_2;
    Task task([&] {
      flag_1.test_and_set();
      flag_1.notify_one();
      flag_2.wait(false);
    });

    ThreadPool thread_pool;
    thread_pool.Submit(&task);
    flag_1.wait(false);
    EXPECT_FALSE(task.Cancel());
    flag_2.test_and_set();
    flag_2.notify_one();
  }

  {
    SCOPED_TRACE("Cancel a completed task");

    bool completed = false;
    Task task([&] { completed = true; });

    ThreadPool thread_pool;
    thread_pool.Submit(&task);
    thread_pool.Wait();

    EXPECT_TRUE(completed);
    EXPECT_FALSE(task.Cancel());
  }
}
}  // namespace
