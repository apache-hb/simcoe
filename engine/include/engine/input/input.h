#pragma once

#include <array>
#include <span>
#include <vector>
#include <atomic>
#include <string_view>

namespace simcoe::input {
    namespace DeviceTags {
        enum Slot : unsigned {
#define DEVICE(ID, NAME) ID,
#include "engine/input/input.inc"
            eTotal
        };
    }

    namespace AxisTags {
        enum Slot : unsigned {
#define AXIS(ID, NAME) ID,
#include "engine/input/input.inc"
            eTotal
        };
    }

    namespace ButtonTags {
        enum Slot : unsigned {
#define BUTTON(ID, NAME) ID,
#include "engine/input/input.inc"
            eTotal
        };
    }

    using DeviceType = DeviceTags::Slot;
    using Button = ButtonTags::Slot;
    using Axis = AxisTags::Slot;

    using ButtonState = std::array<size_t, Button::eTotal>;
    using AxisState = std::array<float, Axis::eTotal>;

    struct State final {
        DeviceType device;
        ButtonState buttons;
        AxisState axes;
    };

    std::string_view toString(DeviceType type);
    std::string_view toString(Button button);
    std::string_view toString(Axis axis);

    struct ISource {
        virtual ~ISource() = default;
        ISource(DeviceType type) : type(type) { }

        DeviceType getDeviceType() const { return type; }

        /**
         * @brief poll this device for input
         *
         * @param state the state to update
         * @param now the current time
         * @return true the device has provided new data
         * @return false the device has not provided new data
         */
        virtual bool poll(State& state) = 0;

    private:
        DeviceType type;
    };

    struct IClient {
        virtual ~IClient() = default;

        virtual void onInput(const State& state) = 0;
    };

    // manages aggregating input from multiple sources
    // and distributing it to clients
    struct Manager {
        void poll();

        void addSource(ISource *pSource);
        void addClient(IClient *pClient);
        const State& getState() const { return state; }

        std::span<ISource*> getSources() { return sources; }
        std::span<IClient*> getClients() { return clients; }

    private:
        std::vector<ISource*> sources;
        std::vector<IClient*> clients;

        State state = {};
    };

    struct Toggle final {
        Toggle(bool bInitial);
        bool update(size_t key);

        bool get() const;
        void set(bool bState);

    private:
        size_t lastValue = 0;
        bool bEnabled;
    };

    struct Event final {
        void update(size_t key);

        bool beginPress();
        bool beginRelease();
        bool isPressed() const;

    private:
        std::atomic_size_t lastValue = 0;
        std::atomic_bool bSendPressEvent = false;
        std::atomic_bool bSendReleaseEvent = false;
    };
}
