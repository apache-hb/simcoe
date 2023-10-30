#include "engine/threads/thread.h"

using namespace simcoe;
using namespace simcoe::threads;

Thread::Thread(ThreadStart fn)
    : hThread(start(fn))
{ }

Thread::~Thread() {
    CloseHandle(hThread);
}
