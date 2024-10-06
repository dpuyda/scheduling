// This project is licensed under the MIT License.
//
// The `WorkStealingDeque` class is copied from Google Filament licensed under
// the Apache License 2.0. See the LICENSE-APACHE file in the root directory of
// this project for more information.
// Original code:
// https://github.com/google/filament/blob/main/libs/utils/include/utils/WorkStealingDequeue.h
// Modifications:
// - Make the work-stealing deque variable-sized.
//
// The `Array` class is copied from Taskflow licensed under the MIT License.
// Original code:
// https://github.com/taskflow/taskflow/blob/master/taskflow/core/tsq.hpp
#pragma once
#include <atomic>
#include <cassert>
#include <functional>
#include <new>
#include <optional>
#include <thread>
#include <vector>

namespace scheduling {
namespace internal {
constexpr auto kCancelled = 1;
constexpr auto kInvoked = 1 << 1;

template <typename T>
  requires std::is_pointer_v<T>
class Array {
 public:
  explicit Array(const int capacity)
      : capacity_{capacity},
        mask_{capacity - 1},
        buffer_{new std::atomic<T>[capacity]} {}

  Array(const Array&) = delete;
  Array(Array&&) = delete;
  Array& operator=(const Array&) = delete;
  Array& operator=(Array&&) = delete;
  ~Array() noexcept { delete[] buffer_; }

  void Put(const size_t index, T item) noexcept {
    buffer_[index & mask_].store(item, std::memory_order_relaxed);
  }

  [[nodiscard]] T Get(const size_t index) noexcept {
    return buffer_[index & mask_].load(std::memory_order_relaxed);
  }

  [[nodiscard]] Array* Resize(const size_t bottom, const size_t top) {
    auto* array = new Array{2 * capacity_};
    for (auto i = top; i != bottom; ++i) {
      array->Put(i, Get(i));
    }
    return array;
  }

  [[nodiscard]] int Capacity() const { return capacity_; }

 private:
  const int capacity_, mask_;
  std::atomic<T>* buffer_;
};

template <typename T>
  requires std::is_pointer_v<T>
class WorkStealingDeque {
 public:
  explicit WorkStealingDeque(const int capacity = 1024)
      : top_{0}, bottom_{0}, array_{new Array<T>{capacity}} {
    assert(capacity && (capacity & capacity - 1) == 0);
    garbage_.reserve(64);
  }

  WorkStealingDeque(const WorkStealingDeque&) = delete;
  WorkStealingDeque(WorkStealingDeque&&) = delete;
  WorkStealingDeque& operator=(const WorkStealingDeque&) = delete;
  WorkStealingDeque& operator=(WorkStealingDeque&&) = delete;

  ~WorkStealingDeque() noexcept {
    for (auto* array : garbage_) {
      delete array;
    }
    delete array_.load();
  }

  void Push(T item) {
    // std::memory_order_relaxed is sufficient because this load doesn't acquire
    // anything from another thread. bottom_ is only written in pop() which
    // cannot be concurrent with push().
    const auto bottom = bottom_.load(std::memory_order_relaxed);
    const auto top = top_.load(std::memory_order_acquire);
    auto* array = array_.load(std::memory_order_relaxed);
    if (array->Capacity() - 1 < bottom - top) {
      array = Resize(array, bottom, top);
    }
    array->Put(bottom, item);
    // std::memory_order_release is used because we release the item we just
    // pushed to other threads which are calling steal().
    bottom_.store(bottom + 1, std::memory_order_release);
  }

  [[nodiscard]] T Pop() {
    // std::memory_order_seq_cst is needed to guarantee ordering in steal() Note
    // however that this is not a typical acquire/release operation:
    // - Not acquire because bottom_ is only written in push() which is not
    // concurrent.
    // - Not release because we're not publishing anything to steal() here.
    // QUESTION: does this prevent top_ load below to be reordered before the
    // "store" part of fetch_sub()? Hopefully it does. If not we'd need a full
    // memory barrier.
    const auto bottom = bottom_.fetch_sub(1, std::memory_order_seq_cst) - 1;
    // bottom could be -1 if we tried to pop() from an empty queue. This will be
    // corrected below.
    assert(bottom >= -1);
    auto* array = array_.load(std::memory_order_relaxed);
    // std::memory_order_seq_cst is needed to guarantee ordering in steal().
    // Note however that this is not a typical acquire operation (i.e. other
    // thread's writes of top_ don't publish data).
    auto top = top_.load(std::memory_order_seq_cst);
    if (top < bottom) {
      // The queue isn't empty, and it's not the last item, just return it, this
      // is the common case.
      return array->Get(bottom);
    }
    T item{nullptr};
    if (top == bottom) {
      // We just took the last item.
      item = array->Get(bottom);
      // Because we know we took the last item, we could be racing with steal()
      // -- the last item being both at the top and bottom of the queue. We
      // resolve this potential race by also stealing that item from ourselves.
      if (top_.compare_exchange_strong(top, top + 1, std::memory_order_seq_cst,
                                       std::memory_order_relaxed)) {
        // Success: we stole our last item from ourselves, meaning that a
        // concurrent steal() would have failed. top_ now equals top + 1, we
        // adjust top to make the queue empty.
        ++top;
      } else {
        // Failure: top_ was not equal to top, which means the item was stolen
        // under our feet. top now equals to top_. Simply discard the item we
        // just popped. The queue is now empty.
        item = nullptr;
      }
    } else {
      // We could be here if the item was stolen just before we read top_, we'll
      // adjust bottom_ below.
      assert(top - bottom == 1);
    }
    // std::memory_order_relaxed used because we're not publishing any data. No
    // concurrent writes to bottom_ possible, it's always safe to write bottom_.
    bottom_.store(top, std::memory_order_relaxed);
    return item;
  }

  [[nodiscard]] T Steal() {
    // Note: A key component of this algorithm is that top_ is read before
    // bottom_ here (and observed as such in other threads).

    // std::memory_order_seq_cst is needed to guarantee ordering in pop(). Note
    // however that this is not a typical acquire operation (i.e. other thread's
    // writes of top_ don't publish data).
    auto top = top_.load(std::memory_order_seq_cst);

    // std::memory_order_acquire is needed because we're acquiring items
    // published in push(). std::memory_order_seq_cst is needed to guarantee
    // ordering in pop().
    if (const auto bottom = bottom_.load(std::memory_order_seq_cst);
        top >= bottom) {
      // The queue is empty.
      return nullptr;
    }

    // The queue isn't empty.
    auto* array = array_.load(std::memory_order_acquire);
    const auto item = array->Get(top);
    if (!top_.compare_exchange_strong(top, top + 1, std::memory_order_seq_cst,
                                      std::memory_order_relaxed)) {
      // Failure: the item we just tried to steal was pop()'ed under our feet,
      // simply discard it; nothing to do -- it's okay to try again.
      return nullptr;
    }
    // Success: we stole an item, just return it.
    return item;
  }

 private:
  [[nodiscard]] Array<T>* Resize(Array<T>* array, const size_t bottom,
                                 const size_t top) {
    auto* tmp = array->Resize(bottom, top);
    garbage_.push_back(array);
    std::swap(array, tmp);
    array_.store(array, std::memory_order_release);
    return array;
  }

#ifdef __cpp_lib_hardware_interference_size
  alignas(std::hardware_destructive_interference_size) std::atomic<int> top_,
      bottom_;
#else
  std::atomic<int> top_, bottom_;
#endif
  std::atomic<Array<T>*> array_;
  std::vector<Array<T>*> garbage_;
};
}  // namespace internal

/**
 * \brief Represents a task in a task graph.
 *
 * A task graph is a collection of tasks and dependencies between them.
 * Dependencies between tasks define the order in which the tasks should be
 * executed.
 */
class Task {
 public:
  /**
   * \brief Creates an empty task.
   *
   * Empty tasks can be used to define dependencies between task groups.
   */
  Task() = default;

  /**
   * \brief Creates a task.
   *
   * The signature of the function to execute should be equivalent to the
   * following:
   * \code{.cpp}
   * void func();
   * \endcode
   *
   * \param func The function to execute.
   */
  template <typename TaskType, typename = std::enable_if_t<std::convertible_to<
                                   TaskType, std::function<void()>>>>
  explicit Task(TaskType&& func) : func_{std::forward<TaskType>(func)} {}

  Task(const Task& other)
      : total_predecessors_{other.total_predecessors_},
        func_{other.func_},
        next_{other.next_} {
    remaining_predecessors_.store(other.remaining_predecessors_);
    cancellation_flags_.store(other.cancellation_flags_);
  }

  Task(Task&& other) noexcept
      : total_predecessors_{other.total_predecessors_},
        func_{std::move(other.func_)},
        next_{std::move(other.next_)} {
    remaining_predecessors_.store(other.remaining_predecessors_);
    cancellation_flags_.store(other.cancellation_flags_);
  }

  Task& operator=(const Task& other) {
    total_predecessors_ = other.total_predecessors_;
    remaining_predecessors_.store(other.remaining_predecessors_);
    cancellation_flags_.store(other.cancellation_flags_);
    func_ = other.func_;
    next_ = other.next_;
    return *this;
  }

  Task& operator=(Task&& other) noexcept {
    total_predecessors_ = other.total_predecessors_;
    remaining_predecessors_.store(other.remaining_predecessors_);
    cancellation_flags_.store(other.cancellation_flags_);
    func_ = std::move(other.func_);
    next_ = std::move(other.next_);
    return *this;
  }

  ~Task() = default;

  /**
   * \brief Defines a task that should be executed before the current task.
   *
   * \param task A task that should be executed before the current task.
   */
  void Succeed(Task* task) {
    task->next_.push_back(this);
    ++total_predecessors_;
    remaining_predecessors_.fetch_add(1);
  }

  /**
   * \brief Defines tasks that should be executed before the current task.
   *
   * \param task, tasks Tasks that should be executed before the current task.
   */
  template <typename... TasksType>
  void Succeed(Task* task, const TasksType&... tasks) {
    task->next_.push_back(this);
    ++total_predecessors_;
    remaining_predecessors_.fetch_add(1);
    Succeed(tasks...);
  }

  /**
   * \brief Defines a task that should be executed after the current task.
   *
   * \param task A task that should be executed after the current task.
   */
  void Precede(Task* task) {
    next_.push_back(task);
    ++task->total_predecessors_;
    task->remaining_predecessors_.fetch_add(1);
  }

  /**
   * \brief Defines tasks that should be executed after the current task.
   *
   * \param task, tasks Tasks that should be executed after the current task.
   */
  template <typename... TasksType>
  void Precede(Task* task, const TasksType&... tasks) {
    next_.push_back(task);
    ++task->total_predecessors_;
    task->remaining_predecessors_.fetch_add(1);
    Precede(tasks...);
  }

  /**
   * \brief Cancels the task.
   *
   * Cancelling a task never fails. If `false` is returned, it means that the
   * task has been invoked earlier, or will be invoked at least once after the
   * cancellation. When a task is cancelled and will not be invoked anymore, its
   * successors also will not be invoked. Call `Reset` to undo cancellation.
   *
   * \see Reset
   *
   * \return `false` if the task has been invoked earlier or will be invoked at
   * least once after the cancellation, `true` otherwise.
   */
  bool Cancel() {
    return (cancellation_flags_.fetch_or(internal::kCancelled) &
            internal::kInvoked) == 0;
  }

  /**
   * \brief Clears cancellation flags.
   *
   * Call `Reset` to undo task cancellation.
   *
   * \see Cancel
   */
  void Reset() { cancellation_flags_.store(0); }

 private:
  friend class ThreadPool;
  bool delete_{false}, is_root_{false};
  int total_predecessors_{0};
  std::atomic<int> remaining_predecessors_{0}, cancellation_flags_{0};
  std::function<void()> func_;
  std::vector<Task*> next_;
};

/**
 * \brief A static thread pool that manages a specified number of background
 * threads and allows to execute tasks on these threads.
 *
 * The threads, managed by the thread pool, execute tasks in a work-stealing
 * manner.
 */
class ThreadPool {
 public:
  /**
   * \brief Creates a `ThreadPool` instance.
   *
   * When created, a `ThreadPool` instance creates a specified number of
   * threads that will be running in the background until the `ThreadPool`
   * instance is destroyed.
   *
   * \param threads_count The number of threads to create.
   */
  explicit ThreadPool(
      const unsigned threads_count = std::thread::hardware_concurrency())
      : queues_count_{threads_count + 1}, queues_{threads_count + 1} {
    threads_.reserve(threads_count);
    for (unsigned i = 0; i != threads_count; ++i) {
      threads_.emplace_back([this, i] { Run(i + 1); });
    }
  }

  ThreadPool(const ThreadPool&) = delete;
  ThreadPool(ThreadPool&&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;
  ThreadPool& operator=(ThreadPool&&) = delete;

  ~ThreadPool() noexcept {
    Wait();
    stop_.test_and_set();
    tasks_count_ += queues_count_;
    tasks_count_.notify_all();
    for (auto& thread : threads_) {
      thread.join();
    }
  }

  /**
   * \brief Submits a function that should be executed on a thread managed by
   * the thread pool.
   *
   * When submitted, the function is pushed into one of the thread pool task
   * queues. Eventually, the function will be popped from the queue and executed
   * on one of the threads managed by the thread pool. The order of function
   * execution is undetermined.
   *
   * The signature of the function should be equivalent to the following:
   * \code{.cpp}
   * void func();
   * \endcode
   *
   * \param func The function to execute.
   */
  template <typename FuncType, typename = std::enable_if_t<std::convertible_to<
                                   FuncType, std::function<void()>>>>
  void Submit(FuncType&& func) {
    auto* task = new Task(std::forward<FuncType>(func));
    task->delete_ = true;
    Submit(task);
  }

  /**
   * \brief Submits a task that should be executed on a thread managed by the
   * thread pool.
   *
   * When submitted, the task is pushed into one of the thread pool task queues.
   * Eventually, the task will be popped from the queue and executed on one of
   * the threads managed by the thread pool. The order of task execution is
   * undetermined.
   *
   * \param task The task to execute.
   */
  void Submit(Task* task) {
    ++tasks_count_;
    queues_[index_].Push(task);
    tasks_count_.notify_one();
  }

  /**
   * \brief Submits a task graph that should be executed on threads managed by
   * the thread pool.
   *
   * A graph is a collection of tasks and dependencies between them. When
   * submitted, the tasks that do not have predecessors are pushed into the
   * thread pool task queues.
   *
   * \param tasks The tasks to execute.
   */
  template <typename TasksType>
  void Submit(TasksType& tasks) {
    for (auto& task : tasks) {
      task.is_root_ = task.total_predecessors_ == 0;
    }
    for (auto& task : tasks) {
      if (task.is_root_) {
        Submit(&task);
      }
    }
  }

  /**
   * \brief Blocks the current thread and executes tasks from the task queues
   * until a specified predicate is satisfied.
   *
   * The signature of the predicate function should be equivalent to the
   * following:
   * \code{.cpp}
   * bool predicate();
   * \endcode
   *
   * \param predicate The predicate which returns `false` if the waiting should
   * be continued.
   */
  template <typename PredicateType>
  void Wait(const PredicateType& predicate) {
    while (!predicate()) {
      if (auto* task = GetTask()) {
        Execute(task);
      }
    }
  }

  /**
   * \brief Blocks the current thread until all task queues are empty.
   *
   * Other threads may push tasks into the task queues while the current thread
   * is blocked.
   */
  void Wait() const {
    while (const auto count = tasks_count_.load()) {
      tasks_count_.wait(count);
    }
  }

 private:
  void Run(const unsigned i) {
    index_ = i;
    for (auto attempts = 0;;) {
      if (constexpr auto max_attempts = 100; ++attempts > max_attempts) {
        tasks_count_.wait(0);
      }
      if (auto* task = GetTask()) {
        Execute(task);
        attempts = 0;
      } else if (stop_.test()) {
        return;
      }
    }
  }

  void Execute(Task* task) {
    for (Task* next = nullptr; task; next = nullptr) {
      task->remaining_predecessors_.store(task->total_predecessors_);
      if (task->cancellation_flags_.fetch_or(internal::kInvoked) &
          internal::kCancelled) {
        break;
      }
      if (task->func_) {
        task->func_();
      }
      auto it = task->next_.begin();
      for (; it != task->next_.end(); ++it) {
        if ((*it)->remaining_predecessors_.fetch_sub(1) == 1) {
          next = *it++;
          break;
        }
      }
      for (; it != task->next_.end(); ++it) {
        if ((*it)->remaining_predecessors_.fetch_sub(1) == 1) {
          Submit(*it);
        }
      }
      if (task->delete_) {
        delete task;
      }
      task = next;
    }
    if (tasks_count_.fetch_sub(1) == 1) {
      tasks_count_.notify_all();
    }
  }

  Task* GetTask() {
    const auto i = index_;
    auto* task = queues_[i].Pop();
    if (task) {
      return task;
    }
    for (unsigned j = 1; j != queues_count_; ++j) {
      task = queues_[(i + j) % queues_count_].Steal();
      if (task) {
        return task;
      }
    }
    return nullptr;
  }

  static thread_local unsigned index_;
  const unsigned queues_count_;
  std::atomic_flag stop_;
  std::atomic<unsigned> tasks_count_;
  std::vector<std::thread> threads_;
  std::vector<internal::WorkStealingDeque<Task*>> queues_;
};
}  // namespace scheduling
