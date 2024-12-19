#ifdef __APPLE__
#include "event_executor.hh"
auto EventExecutor::execute(EventExecutor::Event ev) -> bool { return true; }
#endif
