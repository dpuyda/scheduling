#pragma once

#include <algorithm>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <numeric>
#include <thread>
#include <vector>

namespace scheduling {
namespace internal {
class SchedulerImpl;
}  // namespace internal

/**
 * The default task priority. The task queue is sorted by task priority.
 */
constexpr int kDefaultPriority = 0;

/**
 * Allows to cancel a task scheduled using the scheduler.
 */
class CancellationToken {
 public:
  CancellationToken(const CancellationToken&) = default;
  CancellationToken(CancellationToken&&) = default;
  CancellationToken& operator=(const CancellationToken&) = default;
  CancellationToken& operator=(CancellationToken&&) = default;
  ~CancellationToken() = default;

  /**
   * Attempts to cancel the scheduled task. A task can be canceled only if it
   * has not started yet.
   *
   * @return `true` if the task is canceled successfully or `false` if the task
   * is already completed, is being executed or if the scheduler is destroyed.
   */
  [[nodiscard]] bool Cancel() const { return cancel_function_(); }

 private:
  friend class internal::SchedulerImpl;

  explicit CancellationToken(std::function<bool()> cancel_function)
      : cancel_function_(std::move(cancel_function)) {}

  std::function<bool()> cancel_function_;
};

namespace internal {
struct PrioritizedTask {
  PrioritizedTask(std::function<void()> task, const int priority)
      : task(std::move(task)), priority(priority) {}
  std::function<void()> task;
  int priority;
};

class SchedulerImpl : public std::enable_shared_from_this<SchedulerImpl> {
 public:
  explicit SchedulerImpl(const size_t max_threads_count) : disposing_(false) {
    task_threads_.resize(max_threads_count);
    available_threads_.resize(max_threads_count);
    std::iota(available_threads_.rbegin(), available_threads_.rend(), 0);
  }

  SchedulerImpl(const SchedulerImpl&) = delete;
  SchedulerImpl(SchedulerImpl&&) = delete;
  SchedulerImpl& operator=(const SchedulerImpl&) = delete;
  SchedulerImpl& operator=(SchedulerImpl&&) = delete;

  ~SchedulerImpl() {
    {
      std::lock_guard lock(mutex_);
      disposing_ = true;
    }

    for (auto& thread : task_threads_) {
      if (thread) {
        thread->join();
      }
    }
  }

  template <typename TaskType, typename CallbackType, typename... Args>
  CancellationToken Schedule(const int priority, TaskType&& task,
                             CallbackType&& callback, Args&&... args) {
    auto pending_task = std::make_shared<PrioritizedTask>(
        [this, task = std::forward<TaskType>(task),
         callback = std::forward<CallbackType>(callback),
         ... args = std::forward<Args>(args)]() mutable {
          using ResultType = std::invoke_result_t<TaskType, Args...>;
          if constexpr (std::is_void_v<ResultType>) {
            task(std::forward<Args>(args)...);
            callback();
          } else {
            auto result = task(std::forward<Args>(args)...);
            callback(std::move(result));
          }
        },
        priority);

    if (task_threads_.empty()) {
      pending_task->task();
      return CancellationToken([] { return false; });
    }

    std::lock_guard lock(mutex_);

    if (disposing_) {
      return CancellationToken([] { return false; });
    }

    const auto it =
        std::upper_bound(pending_tasks_.begin(), pending_tasks_.end(),
                         pending_task, [](const auto& lhs, const auto& rhs) {
                           return lhs->priority > rhs->priority;
                         });

    pending_tasks_.insert(it, pending_task);

    if (!available_threads_.empty()) {
      const auto thread_index = available_threads_.back();
      available_threads_.pop_back();
      StartThread(thread_index);
    }

    auto weak_scheduler = weak_from_this();

    return CancellationToken([weak_scheduler = std::move(weak_scheduler),
                              task = std::move(pending_task)] {
      if (const auto scheduler = weak_scheduler.lock()) {
        return scheduler->Cancel(task);
      }
      return false;
    });
  }

 private:
  void StartThread(const size_t thread_index) {
    auto run = [this](const size_t new_thread_index) {
      while (true) {
        std::unique_lock lock(mutex_);

        if (pending_tasks_.empty()) {
          available_threads_.push_back(new_thread_index);
          return;
        }

        const auto pending_task = pending_tasks_.front();
        pending_tasks_.pop_front();

        lock.unlock();

        pending_task->task();
      }
    };

    auto& task_thread = task_threads_[thread_index];

    if (task_thread) {
      task_thread->join();
    }

    task_thread = std::make_unique<std::thread>(
        [run = std::move(run), thread_index] { run(thread_index); });
  }

  bool Cancel(const std::shared_ptr<PrioritizedTask>& pending_task) {
    std::lock_guard lock(mutex_);
    return std::erase(pending_tasks_, pending_task) != 0;
  }

  std::mutex mutex_;
  std::list<std::shared_ptr<PrioritizedTask>> pending_tasks_;
  std::vector<std::unique_ptr<std::thread>> task_threads_;
  std::vector<size_t> available_threads_;
  bool disposing_;
};
}  // namespace internal

/**
 * Allows to schedule an async task and to be notified when the task is
 * completed. The number of threads running simultaneously by the scheduler is
 * limited. Copy-assigned and copy-created schedulers share the same thread
 * pool.
 */
class Scheduler {
 public:
  /**
   * Creates a `Scheduler` instance.
   *
   * @param[in] max_threads_count The maximum number of threads simultaneously
   * run by the scheduler.
   */
  explicit Scheduler(const size_t max_threads_count)
      : impl_(std::make_shared<internal::SchedulerImpl>(max_threads_count)) {}

  Scheduler(const Scheduler&) = default;
  Scheduler(Scheduler&&) = default;
  Scheduler& operator=(const Scheduler&) = default;
  Scheduler& operator=(Scheduler&&) = default;
  ~Scheduler() = default;

  /**
   * Schedules a task with the default priority. Tasks scheduled sooner
   * are closer to the beginning of the task queue than tasks scheduled later.
   * When the task is completed, the callback is called. The task is run and
   * the callback is called on a scheduler thread. The task will be started when
   * a thread for this task becomes available.
   *
   * @param[in] task The task to schedule.
   * @param[in] args The task parameters.
   * @param[in] callback The callback that will be called when the task is
   * completed.
   *
   * @return The cancellation token allowing to cancel the task.
   */
  template <typename TaskType, typename CallbackType, typename... Args>
  CancellationToken Schedule(TaskType&& task, CallbackType&& callback,
                             Args&&... args) {
    return impl_->Schedule(kDefaultPriority, std::forward<TaskType>(task),
                           std::forward<CallbackType>(callback),
                           std::forward<Args>(args)...);
  }

  /**
   * Schedules a task with a given priority. When the task is completed,
   * the callback is called. The task is run and the callback is called on
   * a scheduler thread. The task will be started when a thread for this task
   * becomes available.
   *
   * @param[in] priority The task priority. Tasks with a higher priority are
   * closer to the beginning of the task queue than tasks with a lower priority.
   * If there are multiple tasks with the same priority, tasks scheduled sooner
   * are closer to the beginning of the task queue than tasks scheduled later.
   * The default task priority is 0.
   * @param[in] task The task to schedule.
   * @param[in] args The task parameters.
   * @param[in] callback The callback that will be called when the task is
   * completed.
   *
   * @return The cancellation token allowing to cancel the task.
   */
  template <typename TaskType, typename CallbackType, typename... Args>
  CancellationToken Schedule(const int priority, TaskType&& task,
                             CallbackType&& callback, Args&&... args) {
    return impl_->Schedule(priority, std::forward<TaskType>(task),
                           std::forward<CallbackType>(callback),
                           std::forward<Args>(args)...);
  }

 private:
  std::shared_ptr<internal::SchedulerImpl> impl_;
};
}  // namespace scheduling
