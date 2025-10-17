#include "Registry.h"

lcf::Registry::Registry() : Base()
{
    this->ctx().emplace<Dispatcher>();
}