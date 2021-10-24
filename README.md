# Introduction

`Scheduler` is a simple and minimalistic class allowing you to schedule
an async task and to be notified when the task is completed. Each task is
executed on a thread created by `Scheduler`. When a task is completed,
the callback is called on the same thread on which the task was executed.
The maximum number of threads running simultaneously by `Scheduler` is limited.
`Scheduler` can execute multiple tasks on the same thread. If you copy
a `Scheduler` instance, the newly created copy keeps using the same thread
pool.

# Usage

To use `Scheduler`, simply add [scheduler.h](include/scheduling/scheduler.h) to
your project.

# Examples

Below are examples showing how to schedule a task using `Scheduler`.

First, specify the maximum number of threads that can be running simultaneously
by `Scheduler` and create a `Scheduler` instance. For example:

```cpp
const auto max_threads_count = std::thread::hardware_concurrency();
scheduling::Scheduler scheduler(max_threads_count);
```

To schedule a function without parameters that returns `void`:

```cpp
auto task = [] {
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
};

auto callback = [] { std::cout << "The task is completed!" << std::endl; };

scheduler.Schedule<void>(std::move(task), std::move(callback));
```

To schedule a function without parameters that returns `int`:

```cpp
auto task = [] {
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  return 42;
};

auto callback = [](const int result) { std::cout << result << std::endl; };

scheduler.Schedule<int>(std::move(task), std::move(callback));
```

To schedule a function that calculates the sum of two integers:

```cpp
auto task = [](const int a, const int b) { return a + b; };

auto callback = [](const int sum) { std::cout << sum << std::endl; };

scheduler.Schedule<int>(std::move(task), std::move(callback), 1, 2);
```

To schedule a function with a given priority:

```cpp
constexpr auto priority = scheduling::kDefaultPriority + 1;

auto task = [] {
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  return 42;
};

auto callback = [](const int result) { std::cout << result << std::endl; };

scheduler.Schedule<int>(priority, std::move(task), std::move(callback));
```

> *Note: The user code has to make sure that there is no thread starvation for
> a scheduled task. `Scheduler` does not guarantee that a task will be executed
> when tasks of higher priority are being scheduled. A task with a higher
> priority is always closer to the beginning of the task queue than a task with
> a lower priority.*

To cancel a task if it has not started yet:

```cpp
const auto cancellation_token =
  scheduler.Schedule<int>(std::move(task), std::move(callback));

const auto is_canceled = cancellation_token.Cancel();
```

> *Note: A task that has already started cannot be canceled. The user code
> should make sure that such a task completes as soon as possible if needed.*

# Building examples and tests

To build examples and tests using the CMake script
[CMakeLists.txt](CMakeLists.txt):

1. Create a folder named "build" in the root of the git repository and navigate
to this folder:

    ```
    mkdir build && cd build
    ```

2. Run CMake from this folder:

    ```
    cmake ..
    ```

    Use the `BUILD_EXAMPLES` and `BUILD_TESTS` options to define what targets
    to build. The default value of both options is `ON`.

# C++ standard

`Scheduler` requires C\++20. If you need to compile it using C++17, 14 or 11,
it is easy to update [scheduler.h](include/scheduling/scheduler.h) to suit your
needs.

# License

`Scheduler` is licensed under the [MIT License](LICENSE).
