#include "engine/input/input.h"

using namespace simcoe;
using namespace simcoe::input;

void Manager::poll() {
    bool dirty = false;
    for (ISource *pSource : sources) {
        if (pSource->poll(state)) {
            dirty = true;
            state.device = pSource->getDeviceType();
        }
    }

    if (!dirty) { return; }

    for (IClient *pClient : clients) {
        pClient->onInput(state);
    }
}

///
/// input toggle
///

Toggle::Toggle(bool bInitial) : bEnabled(bInitial) { }

bool Toggle::update(size_t key) {
    if (key > lastValue) {
        lastValue = key;
        bEnabled = !bEnabled;
        return true;
    }

    return false;
}

bool Toggle::get() const {
    return bEnabled;
}

void Toggle::set(bool bState) {
    lastValue = 0;
    bEnabled = bState;
}
