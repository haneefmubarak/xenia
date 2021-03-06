/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2014 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/hid/winkey/winkey_input_driver.h"

#include "xenia/hid/hid-private.h"

namespace xe {
namespace hid {
namespace winkey {

WinKeyInputDriver::WinKeyInputDriver(InputSystem* input_system)
    : packet_number_(1), InputDriver(input_system) {}

WinKeyInputDriver::~WinKeyInputDriver() {}

X_STATUS WinKeyInputDriver::Setup() { return X_STATUS_SUCCESS; }

X_RESULT WinKeyInputDriver::GetCapabilities(uint32_t user_index, uint32_t flags,
                                            X_INPUT_CAPABILITIES* out_caps) {
  if (user_index != 0) {
    return X_ERROR_DEVICE_NOT_CONNECTED;
  }

  // TODO(benvanik): confirm with a real XInput controller.
  out_caps->type = 0x01;      // XINPUT_DEVTYPE_GAMEPAD
  out_caps->sub_type = 0x01;  // XINPUT_DEVSUBTYPE_GAMEPAD
  out_caps->flags = 0;
  out_caps->gamepad.buttons = 0xFFFF;
  out_caps->gamepad.left_trigger = 0xFF;
  out_caps->gamepad.right_trigger = 0xFF;
  out_caps->gamepad.thumb_lx = (int16_t)0xFFFF;
  out_caps->gamepad.thumb_ly = (int16_t)0xFFFF;
  out_caps->gamepad.thumb_rx = (int16_t)0xFFFF;
  out_caps->gamepad.thumb_ry = (int16_t)0xFFFF;
  out_caps->vibration.left_motor_speed = 0;
  out_caps->vibration.right_motor_speed = 0;
  return X_ERROR_SUCCESS;
}

#define IS_KEY_TOGGLED(key) ((GetKeyState(key) & 0x1) == 0x1)
#define IS_KEY_DOWN(key) ((GetAsyncKeyState(key) & 0x8000) == 0x8000)

X_RESULT WinKeyInputDriver::GetState(uint32_t user_index,
                                     X_INPUT_STATE* out_state) {
  if (user_index != 0) {
    return X_ERROR_DEVICE_NOT_CONNECTED;
  }

  packet_number_++;

  uint16_t buttons = 0;
  uint8_t left_trigger = 0;
  uint8_t right_trigger = 0;
  int16_t thumb_lx = 0;
  int16_t thumb_ly = 0;
  int16_t thumb_rx = 0;
  int16_t thumb_ry = 0;

  if (IS_KEY_TOGGLED(VK_CAPITAL)) {
    // dpad toggled
    if (IS_KEY_DOWN(0x41)) {
      // A
      buttons |= 0x0004;  // XINPUT_GAMEPAD_DPAD_LEFT
    }
    if (IS_KEY_DOWN(0x44)) {
      // D
      buttons |= 0x0008;  // XINPUT_GAMEPAD_DPAD_RIGHT
    }
    if (IS_KEY_DOWN(0x53)) {
      // S
      buttons |= 0x0002;  // XINPUT_GAMEPAD_DPAD_DOWN
    }
    if (IS_KEY_DOWN(0x57)) {
      // W
      buttons |= 0x0001;  // XINPUT_GAMEPAD_DPAD_UP
    }
  } else {
    // left stick
    if (IS_KEY_DOWN(0x41)) {
      // A
      thumb_lx += SHRT_MIN;
    }
    if (IS_KEY_DOWN(0x44)) {
      // D
      thumb_lx += SHRT_MAX;
    }
    if (IS_KEY_DOWN(0x53)) {
      // S
      thumb_ly += SHRT_MIN;
    }
    if (IS_KEY_DOWN(0x57)) {
      // W
      thumb_ly += SHRT_MAX;
    }
  }

  if (IS_KEY_DOWN(0x4C)) {
    // L
    buttons |= 0x4000;  // XINPUT_GAMEPAD_X
  }
  if (IS_KEY_DOWN(VK_OEM_7)) {
    // '
    buttons |= 0x2000;  // XINPUT_GAMEPAD_B
  }
  if (IS_KEY_DOWN(VK_OEM_1)) {
    // ;
    buttons |= 0x1000;  // XINPUT_GAMEPAD_A
  }
  if (IS_KEY_DOWN(0x50)) {
    // P
    buttons |= 0x8000;  // XINPUT_GAMEPAD_Y
  }

  if (IS_KEY_DOWN(0x5A)) {
    // Z
    buttons |= 0x0020;  // XINPUT_GAMEPAD_BACK
  }
  if (IS_KEY_DOWN(0x58)) {
    // X
    buttons |= 0x0010;  // XINPUT_GAMEPAD_START
  }

  out_state->packet_number = packet_number_;
  out_state->gamepad.buttons = buttons;
  out_state->gamepad.left_trigger = left_trigger;
  out_state->gamepad.right_trigger = right_trigger;
  out_state->gamepad.thumb_lx = thumb_lx;
  out_state->gamepad.thumb_ly = thumb_ly;
  out_state->gamepad.thumb_rx = thumb_rx;
  out_state->gamepad.thumb_ry = thumb_ry;

  return X_ERROR_SUCCESS;
}

X_RESULT WinKeyInputDriver::SetState(uint32_t user_index,
                                     X_INPUT_VIBRATION* vibration) {
  if (user_index != 0) {
    return X_ERROR_DEVICE_NOT_CONNECTED;
  }

  return X_ERROR_SUCCESS;
}

X_RESULT WinKeyInputDriver::GetKeystroke(uint32_t user_index, uint32_t flags,
                                         X_INPUT_KEYSTROKE* out_keystroke) {
  if (user_index != 0) {
    return X_ERROR_DEVICE_NOT_CONNECTED;
  }

  X_RESULT result = X_ERROR_EMPTY;

  uint16_t virtual_key = 0;
  uint16_t unicode = 0;
  uint16_t keystroke_flags = 0;
  uint8_t hid_code = 0;

  out_keystroke->virtual_key = virtual_key;
  out_keystroke->unicode = unicode;
  out_keystroke->flags = keystroke_flags;
  out_keystroke->user_index = 0;
  out_keystroke->hid_code = hid_code;

  // X_ERROR_EMPTY if no new keys
  // X_ERROR_DEVICE_NOT_CONNECTED if no device
  // X_ERROR_SUCCESS if key
  return result;
}

}  // namespace winkey
}  // namespace hid
}  // namespace xe
