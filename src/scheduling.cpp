#include "scheduling/scheduling.h"

namespace scheduling {
thread_local unsigned ThreadPool::index_{0};
}  // namespace scheduling
