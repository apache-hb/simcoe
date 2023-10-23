#include "engine/input/input.h"

using namespace simcoe;
using namespace simcoe::input;

std::string_view input::toString(DeviceType type) {
    switch (type) {
#define DEVICE(ID, NAME) case DeviceTags::ID: return NAME;
#include "engine/input/input.inc"
    default: return "unknown";
    }
}

std::string_view input::toString(Button button) {
    switch (button) {
#define BUTTON(ID, NAME) case ButtonTags::ID: return NAME;
#include "engine/input/input.inc"
    default: return "unknown";
    }
}

std::string_view input::toString(Axis axis) {
    switch (axis) {
#define AXIS(ID, NAME) case AxisTags::ID: return NAME;
#include "engine/input/input.inc"
    default: return "unknown";
    }
}

///
/// input manager
///

void Manager::poll() {
    bool bDirty = false;
    for (ISource *pSource : sources) {
        if (pSource->poll(state)) {
            bDirty = true;
            state.device = pSource->getDeviceType();
        }
    }

    if (!bDirty) { return; }

    for (IClient *pClient : clients) {
        pClient->onInput(state);
    }
}

void Manager::addSource(ISource *pSource) {
    sources.push_back(pSource);
}

void Manager::addClient(IClient *pClient) {
    clients.push_back(pClient);
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

///
/// button events
///

void Event::update(size_t key) {
    if (key > lastValue) {
        lastValue = key;
        bSendPressEvent = true;
    } else if (key == 0 && lastValue > 0) {
        lastValue = 0;
        bSendReleaseEvent = true;
    } else {
        bSendPressEvent = false;
        bSendReleaseEvent = false;
    }
}

bool Event::beginPress() {
    if (lastValue > 0 && bSendPressEvent) {
        bSendPressEvent = false;
        return true;
    }

    return false;
}

bool Event::beginRelease() {
    if (lastValue == 0 && bSendReleaseEvent) {
        bSendReleaseEvent = false;
        return true;
    }

    return false;
}

bool Event::isPressed() const {
    return lastValue > 0;
}
