#ifndef DEVICE
#   define DEVICE(ID, NAME)
#endif

DEVICE(eDeviceWin32, "input.device.win32")
DEVICE(eDeviceXInput, "input.device.xinput")
DEVICE(eDeviceGameInput, "input.device.gameinput")

#undef DEVICE

#ifndef AXIS
#   define AXIS(ID, NAME)
#endif

AXIS(eMouseX, "input.axis.mouse.x")
AXIS(eMouseY, "input.axis.mouse.y")

AXIS(eGamepadLeftX, "input.axis.gamepad.left.x")
AXIS(eGamepadLeftY, "input.axis.gamepad.left.y")

AXIS(eGamepadRightX, "input.axis.gamepad.right.x")
AXIS(eGamepadRightY, "input.axis.gamepad.right.y")

AXIS(eGamepadLeftTrigger, "input.axis.gamepad.lefttrigger")
AXIS(eGamepadRightTrigger, "input.axis.gamepad.righttrigger")

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

BUTTON(ePadBumperLeft, "input.button.gamepad.bumper.left")
BUTTON(ePadBumperRight, "input.button.gamepad.bumper.right")

BUTTON(ePadStart, "input.button.gamepad.start")
BUTTON(ePadBack, "input.button.gamepad.back")

// events from when sticks are pushed down
BUTTON(ePadStickLeft, "input.button.gamepad.stick.left")
BUTTON(ePadStickRight, "input.button.gamepad.stick.right")

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

BUTTON(eKeyShiftLeft, "input.button.leftshift")
BUTTON(eKeyShiftRight, "input.button.rightshift")

BUTTON(eKeyControlLeft, "input.button.leftcontrol")
BUTTON(eKeyControlRight, "input.button.rightcontrol")

BUTTON(eKeyAltLeft, "input.button.leftalt")
BUTTON(eKeyAltRight, "input.button.rightalt")

#undef BUTTON
