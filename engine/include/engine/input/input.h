#pragma once

#include <array>
#include <vector>

namespace simcoe::input {
    namespace DeviceTags {
        enum Slot : unsigned {
            eDeviceRawInput, ///< Windows Raw Input
            eDeviceWin32,    ///< win32 messages
            eDeviceXInput,   ///< XInput
            eDeviceGameInput ///< GameInput
        };
    }

    namespace AxisTags {
        enum Slot : unsigned {
            eTotal
        };
    }

    namespace ButtonTags {
        enum Slot : unsigned {
            eTotal
        };
    }

    using DeviceType = DeviceTags::Slot;
    using AxisType = AxisTags::Slot;
    using ButtonType = ButtonTags::Slot;

    using ButtonState = std::array<size_t, ButtonTags::eTotal>;
    using AxisState = std::array<float, AxisTags::eTotal>;

    struct State final {
        DeviceType device;
        ButtonState buttons;
        AxisState axes;
    };

    struct ISource {
        virtual ~ISource() = default;
        ISource(DeviceType type) : type(type) { }

        DeviceType getDeviceType() const { return type; }

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

        void addSource(ISource *pSource) {
            sources.push_back(pSource);
        }

        void addClient(IClient *pClient) {
            clients.push_back(pClient);
        }

        const State& getState() const { return state; }

    private:
        std::vector<ISource *> sources;
        std::vector<IClient *> clients;

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
}
