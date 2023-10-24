#pragma once

#include "engine/input/input.h"
#include "engine/system/system.h"
#include "engine/util/retry.h"

#include <windows.h>
#include <Xinput.h>

namespace simcoe::input {
    struct XInputGamepad final : ISource {
        XInputGamepad(DWORD dwIndex);

        bool poll(State& state) override;

    private:
        /**
         * @brief check if a button is pressed and update @arg result if it is
         *
         * @param result the destination state
         * @param button the button index to assign the update to
         * @param mask the mask to check against
         * @param state the state bits to check against
         * @return true the button is pressed
         * @return false the button is not pressed
         */
        bool updateButton(State& result, Button button, WORD mask, WORD state);

        DWORD dwIndex;
        size_t keyPressIndex = 0;

        DWORD dwLastPacket = 0;
    };
}
