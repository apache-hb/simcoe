#include "engine/threads/thread.h"

using namespace simcoe;
using namespace simcoe::threads;

Thread::~Thread() {
    CloseHandle(hThread);
}
