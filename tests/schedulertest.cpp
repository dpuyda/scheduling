#include <gtest/gtest.h>

#include "scheduling/scheduler.h"

namespace {
using namespace testing;

TEST(SchedulerTest, ExecuteTask) {
  scheduling::Scheduler scheduler(std::thread::hardware_concurrency());

  {
    SCOPED_TRACE("A function without parameters that returns void");

    std::atomic is_task_executed = false;
    auto task = [&] { is_task_executed = true; };

    std::atomic is_callback_called = false;
    auto callback = [&] {
      is_callback_called = true;
      is_callback_called.notify_one();
    };

    scheduler.Schedule<void>(std::move(task), std::move(callback));

    is_callback_called.wait(false);
    EXPECT_TRUE(is_task_executed);
  }

  {
    SCOPED_TRACE("A function without parameters that returns int");

    constexpr auto expected = 42;
    auto task = [] { return expected; };

    std::atomic actual = 0;
    auto callback = [&](const int result) {
      actual = result;
      actual.notify_one();
    };

    scheduler.Schedule<int>(std::move(task), std::move(callback));

    actual.wait(0);
    EXPECT_EQ(actual, expected);
  }

  {
    SCOPED_TRACE("Sum of two integers");

    auto task = [](const int a, const int b) { return a + b; };

    std::atomic actual = 0;
    auto callback = [&](const int result) {
      actual = result;
      actual.notify_one();
    };

    constexpr auto a = 1, b = 2;
    scheduler.Schedule<int>(std::move(task), std::move(callback), a, b);

    actual.wait(0);
    EXPECT_EQ(actual, a + b);
  }
}

TEST(SchedulerTest, WaitForCompletionWhenDestroyed) {
  constexpr auto expected = 42;
  std::atomic actual = 0;

  {
    scheduling::Scheduler scheduler(std::thread::hardware_concurrency());

    auto task = [] { return expected; };

    auto callback = [&](const int result) {
      actual = result;
      actual.notify_one();
    };

    scheduler.Schedule<int>(std::move(task), std::move(callback));
  }

  actual.wait(0);
  EXPECT_EQ(actual, expected);
}

TEST(SchedulerTest, CancelTaskNotStarted) {
  std::atomic is_canceled = true;

  {
    constexpr auto max_threads_count = 1;
    scheduling::Scheduler scheduler(max_threads_count);

    std::atomic release_first_task = false;
    scheduler.Schedule<void>([&] { release_first_task.wait(false); }, [] {});

    const auto cancellation_token =
        scheduler.Schedule<void>([] {}, [&] { is_canceled = false; });

    EXPECT_TRUE(cancellation_token.Cancel());

    release_first_task = true;
    release_first_task.notify_one();
  }

  EXPECT_TRUE(is_canceled);
}

TEST(SchedulerTest, CancelTaskStarted) {
  constexpr auto max_threads_count = 1;
  scheduling::Scheduler scheduler(max_threads_count);

  std::atomic is_task_started = false;
  auto task = [&] {
    is_task_started = true;
    is_task_started.notify_one();
  };

  std::atomic is_callback_called = false;
  auto callback = [&] {
    is_callback_called = true;
    is_callback_called.notify_one();
  };

  const auto cancellation_token =
      scheduler.Schedule<void>(std::move(task), std::move(callback));

  is_task_started.wait(false);

  EXPECT_FALSE(cancellation_token.Cancel());

  is_callback_called.wait(false);
}

TEST(SchedulerTest, CancelSchedulerDestroyed) {
  constexpr auto max_threads_count = 1;
  auto scheduler = std::make_unique<scheduling::Scheduler>(max_threads_count);

  auto task = [] {};
  auto callback = [] {};

  const auto cancellation_token =
      scheduler->Schedule<void>(std::move(task), std::move(callback));

  scheduler = nullptr;

  EXPECT_FALSE(cancellation_token.Cancel());
}

TEST(SchedulerTest, TaskAndCallbackOnSameThread) {
  std::thread::id task_thread_id;
  std::thread::id callback_thread_id;

  {
    constexpr auto max_threads_count = 1;
    scheduling::Scheduler scheduler(max_threads_count);

    const auto task = [&] { task_thread_id = std::this_thread::get_id(); };

    const auto callback = [&] {
      callback_thread_id = std::this_thread::get_id();
    };

    scheduler.Schedule<void>(std::move(task), std::move(callback));
  }

  EXPECT_EQ(task_thread_id, callback_thread_id);
  EXPECT_NE(task_thread_id, std::this_thread::get_id());
}

TEST(SchedulerTest, UseDifferentThreads) {
  std::vector<std::thread::id> thread_id(2);
  std::vector<std::atomic<bool>> task_started_flags(thread_id.size());
  std::vector<std::atomic<bool>> release_task_flags(thread_id.size());

  {
    constexpr auto max_threads_count = 2;
    scheduling::Scheduler scheduler(max_threads_count);

    for (size_t i = 0; i < release_task_flags.size(); ++i) {
      auto task = [&, i] {
        task_started_flags[i] = true;
        task_started_flags[i].notify_one();
        release_task_flags[i].wait(false);
        thread_id[i] = std::this_thread::get_id();
      };

      auto callback = [] {};

      scheduler.Schedule<void>(std::move(task), std::move(callback));
    }

    for (auto& flag : task_started_flags) {
      flag.wait(false);
    }

    for (auto& flag : release_task_flags) {
      flag = true;
      flag.notify_one();
    }
  }

  EXPECT_NE(thread_id[0], thread_id[1]);
}

TEST(SchedulerTest, ReuseThread) {
  std::vector<std::thread::id> thread_id(2);

  {
    constexpr auto max_threads_count = 1;
    scheduling::Scheduler scheduler(max_threads_count);

    std::atomic release_first_task = false;
    const auto callback = [] {};

    scheduler.Schedule<void>(
        [&] {
          release_first_task.wait(false);
          thread_id[0] = std::this_thread::get_id();
        },
        callback);

    scheduler.Schedule<void>([&] { thread_id[1] = std::this_thread::get_id(); },
                             callback);

    release_first_task = true;
    release_first_task.notify_one();
  }

  EXPECT_EQ(thread_id[0], thread_id[1]);
  EXPECT_NE(thread_id[0], std::this_thread::get_id());
}

TEST(SchedulerTest, Priority) {
  std::vector<int> tasks_id;

  {
    constexpr auto max_threads_count = 1;
    scheduling::Scheduler scheduler(max_threads_count);

    std::atomic release_first_task = false;
    const auto callback = [] {};

    scheduler.Schedule<void>(
        scheduling::kDefaultPriority, [&] { release_first_task.wait(false); },
        callback);

    scheduler.Schedule<void>(
        scheduling::kDefaultPriority, [&] { tasks_id.push_back(0); }, callback);

    scheduler.Schedule<void>(
        scheduling::kDefaultPriority + 1, [&] { tasks_id.push_back(1); },
        callback);

    release_first_task = true;
    release_first_task.notify_one();
  }

  EXPECT_EQ(tasks_id[0], 1);
}

TEST(SchedulerTest, TaskOrderSamePriority) {
  std::vector<int> tasks_id;

  {
    constexpr auto max_threads_count = 1;
    scheduling::Scheduler scheduler(max_threads_count);

    std::atomic release_first_task = false;
    const auto callback = [] {};

    scheduler.Schedule<void>([&] { release_first_task.wait(false); }, callback);
    scheduler.Schedule<void>([&] { tasks_id.push_back(0); }, callback);
    scheduler.Schedule<void>([&] { tasks_id.push_back(1); }, callback);

    release_first_task = true;
    release_first_task.notify_one();
  }

  EXPECT_EQ(tasks_id[0], 0);
}

TEST(SchedulerTest, ZeroThreadsCount) {
  constexpr auto max_threads_count = 0;
  scheduling::Scheduler scheduler(max_threads_count);

  std::thread::id task_thread_id;
  auto task = [&] { task_thread_id = std::this_thread::get_id(); };

  std::thread::id callback_thread_id;
  auto callback = [&] { callback_thread_id = std::this_thread::get_id(); };

  const auto cancellation_token =
      scheduler.Schedule<void>(std::move(task), std::move(callback));

  EXPECT_EQ(task_thread_id, std::this_thread::get_id());
  EXPECT_EQ(callback_thread_id, std::this_thread::get_id());
  EXPECT_FALSE(cancellation_token.Cancel());
}
}  // namespace
