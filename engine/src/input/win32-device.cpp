#include "engine/input/win32-device.h"
#include "common.h"

#include "engine/service/logging.h"

#include <unordered_map>

using namespace simcoe;
using namespace simcoe::input;

///
/// keyboard input
///

namespace {
    // handling keyboard accuratley is more tricky than it first seems
    // https://learn.microsoft.com/en-us/windows/win32/inputdev/about-keyboard-input#keystroke-message-flags
    WORD mapKeyCode(WPARAM wParam, LPARAM lParam) {
        WORD vkCode = LOWORD(wParam);
        WORD wKeyFlags = HIWORD(lParam);
        WORD wScanCode = LOBYTE(wKeyFlags);

        if ((wKeyFlags & KF_EXTENDED) == KF_EXTENDED) {
            wScanCode = MAKEWORD(wScanCode, 0xE0);
        }

        if (vkCode == VK_SHIFT || vkCode == VK_CONTROL || vkCode == VK_MENU) {
            vkCode = LOWORD(MapVirtualKeyA(wScanCode, MAPVK_VSC_TO_VK_EX));
        }

        return vkCode;
    }

    const std::unordered_map<int, Button> kDesktopButtons = {
        { 'A', Button::eKeyA },
        { 'B', Button::eKeyB },
        { 'C', Button::eKeyC },
        { 'D', Button::eKeyD },
        { 'E', Button::eKeyE },
        { 'F', Button::eKeyF },
        { 'G', Button::eKeyG },
        { 'H', Button::eKeyH },
        { 'I', Button::eKeyI },
        { 'J', Button::eKeyJ },
        { 'K', Button::eKeyK },
        { 'L', Button::eKeyL },
        { 'M', Button::eKeyM },
        { 'N', Button::eKeyN },
        { 'O', Button::eKeyO },
        { 'P', Button::eKeyP },
        { 'Q', Button::eKeyQ },
        { 'R', Button::eKeyR },
        { 'S', Button::eKeyS },
        { 'T', Button::eKeyT },
        { 'U', Button::eKeyU },
        { 'V', Button::eKeyV },
        { 'W', Button::eKeyW },
        { 'X', Button::eKeyX },
        { 'Y', Button::eKeyY },
        { 'Z', Button::eKeyZ },

        { '0', Button::eKey0 },
        { '1', Button::eKey1 },
        { '2', Button::eKey2 },
        { '3', Button::eKey3 },
        { '4', Button::eKey4 },
        { '5', Button::eKey5 },
        { '6', Button::eKey6 },
        { '7', Button::eKey7 },
        { '8', Button::eKey8 },
        { '9', Button::eKey9 },

        { VK_LEFT,  Button::eKeyLeft },
        { VK_RIGHT, Button::eKeyRight },
        { VK_UP,    Button::eKeyUp },
        { VK_DOWN,  Button::eKeyDown },

        { VK_ESCAPE, Button::eKeyEscape },
        { VK_LSHIFT, Button::eKeyShiftLeft },

        { VK_RSHIFT, Button::eKeyShiftRight },
        { VK_LCONTROL, Button::eKeyControlLeft },
        { VK_RCONTROL, Button::eKeyControlRight },
        { VK_LMENU, Button::eKeyAltLeft },
        { VK_RMENU, Button::eKeyAltRight },
        { VK_SPACE, Button::eKeySpace },

        { VK_RETURN, Button::eKeyEnter },

        { VK_OEM_3, Button::eKeyTilde },

        { VK_LBUTTON, Button::eKeyMouseLeft },
        { VK_RBUTTON, Button::eKeyMouseRight },
        { VK_MBUTTON, Button::eKeyMouseMiddle },

        { VK_XBUTTON1, Button::eKeyMouseX1 },
        { VK_XBUTTON2, Button::eKeyMouseX2 }
    };
}

Win32Keyboard::Win32Keyboard()
    : ISource(DeviceTags::eWin32)
{ }

bool Win32Keyboard::poll(State& state) {
    bool bDirty = false;

    for (const auto& [vk, button] : kDesktopButtons) {
        bDirty |= detail::update(state.buttons[button], buttons[button]);
    }

    return bDirty;
}

void Win32Keyboard::handleMsg(UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:
        // ignore key repeats
        if (HIWORD(lParam) & KF_REPEAT) { return; }

        setKey(mapKeyCode(wParam, lParam), index++);
        break;

    case WM_SYSKEYUP:
    case WM_KEYUP:
        setKey(mapKeyCode(wParam, lParam), 0);
        break;

    case WM_RBUTTONDOWN:
        setKey(VK_RBUTTON, index++);
        break;
    case WM_RBUTTONUP:
        setKey(VK_RBUTTON, 0);
        break;

    case WM_LBUTTONDOWN:
        setKey(VK_LBUTTON, index++);
        break;
    case WM_LBUTTONUP:
        setKey(VK_LBUTTON, 0);
        break;

    case WM_MBUTTONDOWN:
        setKey(VK_MBUTTON, index++);
        break;
    case WM_MBUTTONUP:
        setKey(VK_MBUTTON, 0);
        break;

    case WM_XBUTTONDOWN:
        setXButton(GET_XBUTTON_WPARAM(wParam), index++);
        break;

    case WM_XBUTTONUP:
        setXButton(GET_XBUTTON_WPARAM(wParam), 0);
        break;

    default:
        break;
    }
}

void Win32Keyboard::setKey(WORD key, size_t value) {
    auto it = kDesktopButtons.find(key);
    if (it == kDesktopButtons.end()) {
        LOG_WARN("Unknown key: {}", key);
        return;
    }

    buttons[it->second] = value;
}

void Win32Keyboard::setXButton(WORD key, size_t value) {
    switch (key) {
    case XBUTTON1:
        setKey(VK_XBUTTON1, value);
        break;
    case XBUTTON2:
        setKey(VK_XBUTTON2, value);
        break;
    default:
        setKey(key, value);
        break;
    }
}

///
/// mouse input
///

namespace {
    math::int2 getWindowCenter(Window *pWindow) {
        auto rect = pWindow->getClientCoords();

        int centerX = (rect.right - rect.left) / 2;
        int centerY = (rect.bottom - rect.top) / 2;

        return math::int2{centerX, centerY};
    }

    math::int2 getCursorPoint() {
        POINT point;
        GetCursorPos(&point);
        return math::int2{point.x, point.y};
    }
}

Win32Mouse::Win32Mouse(Window *pWindow, bool bEnabled)
    : ISource(DeviceTags::eWin32)
    , pWindow(pWindow)
    , bMouseEnabled(bEnabled)
{ }

bool Win32Mouse::poll(State& state) {
    if (!bMouseEnabled) {
        return false;
    }

    update();

    bool bDirty = false;
    if (totalEventsToSend > 0) {
        totalEventsToSend -= 1;
        bDirty = true;
    }

    state.axes[Axis::eMouseX] = float(mouseAbsolute.x - mouseOrigin.x);
    state.axes[Axis::eMouseY] = float(mouseAbsolute.y - mouseOrigin.y);

    return bDirty;
}

void Win32Mouse::update() {
    auto cursor = getCursorPoint();
    if (bMouseCaptured) {
        auto center = getWindowCenter(pWindow);

        mouseOrigin = center;
        updateMouseAbsolute(cursor);

        SetCursorPos(center.x, center.y);
    } else {
        mouseOrigin = mouseAbsolute; // our origin is the last absolute position
        updateMouseAbsolute(cursor);
    }
}

void Win32Mouse::updateMouseAbsolute(math::int2 mousePoint) {
    if (mousePoint != mouseAbsolute) {
        totalEventsToSend = 2; // we need to send at least 2 events to make sure the mouse delta returns to 0,0
    }

    mouseAbsolute = mousePoint;
}
