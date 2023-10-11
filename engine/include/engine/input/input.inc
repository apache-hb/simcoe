#ifndef DEVICE
#   define DEVICE(ID, NAME)
#endif

DEVICE(eWin32, "input.device.win32")
DEVICE(eXInput, "input.device.xinput")
DEVICE(eGameInput, "input.device.gameinput")

#undef DEVICE

#ifndef AXIS
#   define AXIS(ID, NAME)
#endif

AXIS(eMouseX, "input.axis.mouse.x")
AXIS(eMouseY, "input.axis.mouse.y")

AXIS(eGamepadLeftX, "input.axis.lstick.x")
AXIS(eGamepadLeftY, "input.axis.lstick.y")

AXIS(eGamepadRightX, "input.axis.rstick.x")
AXIS(eGamepadRightY, "input.axis.rstick.y")

AXIS(eGamepadLeftTrigger, "input.axis.gamepad.ltrigger")
AXIS(eGamepadRightTrigger, "input.axis.gamepad.rtrigger")

#undef AXIS

#ifndef BUTTON
#   define BUTTON(ID, NAME)
#endif

// gamepad keys

// letter keys on xbox, shape keys on ps
BUTTON(ePadButtonUp, "input.button.gamepad.up")
BUTTON(ePadButtonDown, "input.button.gamepad.down")
BUTTON(ePadButtonLeft, "input.button.gamepad.left")
BUTTON(ePadButtonRight, "input.button.gamepad.right")

BUTTON(ePadDirectionUp, "input.button.gamepad.direction.up")
BUTTON(ePadDirectionDown, "input.button.gamepad.direction.down")
BUTTON(ePadDirectionLeft, "input.button.gamepad.direction.left")
BUTTON(ePadDirectionRight, "input.button.gamepad.direction.right")

BUTTON(ePadBumperLeft, "input.button.gamepad.lbumper")
BUTTON(ePadBumperRight, "input.button.gamepad.rbumper")

BUTTON(ePadStart, "input.button.gamepad.start")
BUTTON(ePadBack, "input.button.gamepad.back")

// events from when sticks are pushed down
BUTTON(ePadStickLeft, "input.button.gamepad.lstick")
BUTTON(ePadStickRight, "input.button.gamepad.rstick")

// keyboard keys

BUTTON(eKeyA, "input.button.a")
BUTTON(eKeyB, "input.button.b")
BUTTON(eKeyC, "input.button.c")
BUTTON(eKeyD, "input.button.d")
BUTTON(eKeyE, "input.button.e")
BUTTON(eKeyF, "input.button.f")
BUTTON(eKeyG, "input.button.g")
BUTTON(eKeyH, "input.button.h")
BUTTON(eKeyI, "input.button.i")
BUTTON(eKeyJ, "input.button.j")
BUTTON(eKeyK, "input.button.k")
BUTTON(eKeyL, "input.button.l")
BUTTON(eKeyM, "input.button.m")
BUTTON(eKeyN, "input.button.n")
BUTTON(eKeyO, "input.button.o")
BUTTON(eKeyP, "input.button.p")
BUTTON(eKeyQ, "input.button.q")
BUTTON(eKeyR, "input.button.r")
BUTTON(eKeyS, "input.button.s")
BUTTON(eKeyT, "input.button.t")
BUTTON(eKeyU, "input.button.u")
BUTTON(eKeyV, "input.button.v")
BUTTON(eKeyW, "input.button.w")
BUTTON(eKeyX, "input.button.x")
BUTTON(eKeyY, "input.button.y")
BUTTON(eKeyZ, "input.button.z")

BUTTON(eKey0, "input.button.0")
BUTTON(eKey1, "input.button.1")
BUTTON(eKey2, "input.button.2")
BUTTON(eKey3, "input.button.3")
BUTTON(eKey4, "input.button.4")
BUTTON(eKey5, "input.button.5")
BUTTON(eKey6, "input.button.6")
BUTTON(eKey7, "input.button.7")
BUTTON(eKey8, "input.button.8")
BUTTON(eKey9, "input.button.9")

BUTTON(eKeyLeft, "input.button.left")
BUTTON(eKeyRight, "input.button.right")
BUTTON(eKeyUp, "input.button.up")
BUTTON(eKeyDown, "input.button.down")

BUTTON(eKeyShiftLeft, "input.button.lshift")
BUTTON(eKeyShiftRight, "input.button.rshift")

BUTTON(eKeyControlLeft, "input.button.lctrl")
BUTTON(eKeyControlRight, "input.button.rctrl")

BUTTON(eKeyAltLeft, "input.button.lalt")
BUTTON(eKeyAltRight, "input.button.ralt")

BUTTON(eKeySpace, "input.button.space")
BUTTON(eKeyEnter, "input.button.enter")
BUTTON(eKeyEscape, "input.button.escape")
BUTTON(eKeyTilde, "input.button.tilde")

BUTTON(eKeyMouseLeft, "input.button.mouse.left")
BUTTON(eKeyMouseRight, "input.button.mouse.right")
BUTTON(eKeyMouseMiddle, "input.button.mouse.middle")

BUTTON(eKeyMouseX1, "input.button.mouse.x1")
BUTTON(eKeyMouseX2, "input.button.mouse.x2")

#undef BUTTON
