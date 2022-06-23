#include <iostream>

#include "scheduling/scheduler.h"

int main() {
  const auto max_threads_count = std::thread::hardware_concurrency();
  scheduling::Scheduler scheduler(max_threads_count);

  // Schedule a function without parameters that returns `void`:
  {
    auto task = [] {
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    };

    auto callback = [] { std::cout << "The task is completed!" << std::endl; };

    scheduler.Schedule(std::move(task), std::move(callback));
  }

  // Schedule a function without parameters that returns `int`:
  {
    auto task = [] {
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
      return 42;
    };

    auto callback = [](const int result) { std::cout << result << std::endl; };

    scheduler.Schedule(std::move(task), std::move(callback));
  }

  // Schedule a function that calculates the sum of two integers:
  {
    auto task = [](const int a, const int b) { return a + b; };

    auto callback = [](const int sum) { std::cout << sum << std::endl; };

    scheduler.Schedule(std::move(task), std::move(callback), 1, 2);
  }

  // Schedule a function with a given priority:
  {
    constexpr auto priority = scheduling::kDefaultPriority + 1;

    auto task = [] {
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
      return 42;
    };

    auto callback = [](const int result) { std::cout << result << std::endl; };

    scheduler.Schedule(priority, std::move(task), std::move(callback));
  }

  // Cancel a task if it has not started yet:
  {
    auto task = [] {
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
      return 42;
    };

    auto callback = [](const int result) { std::cout << result << std::endl; };

    const auto cancellation_token =
        scheduler.Schedule(std::move(task), std::move(callback));

    if (const auto is_canceled = cancellation_token.Cancel(); is_canceled) {
      std::cout << "The task is canceled successfully!" << std::endl;
    } else {
      std::cout << "The task cannot be canceled" << std::endl;
    }
  }

  return 0;
}
