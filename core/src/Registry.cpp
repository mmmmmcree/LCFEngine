#include "Registry.h"
#include "tasks/TaskScheduler.h"

using namespace lcf;

Registry::Registry() : Base()
{
    this->ctx().emplace<TaskScheduler>(TaskScheduler::RunMode::eNewThread); //! must emplace before dispatcher? don't know why
    this->ctx().emplace<Dispatcher>();
}