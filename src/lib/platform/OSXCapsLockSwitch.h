/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <memory>

//! Caps Lock input source switching for keys from another computer
/*!
macOS can use the Caps Lock key to switch to and from the last used Latin
input source (System Settings > Keyboard > Text Input > Edit). The system
only does this for Caps Lock presses that come from a hardware keyboard;
synthesized events, whether posted with CGEventPost or IOHIDPostEvent, just
toggle Caps Lock. This class reproduces the system behaviour for remote
presses: a tap switches the input source, pressing and holding turns Caps
Lock on, and while Caps Lock is on a tap turns it off (a hold, if the user set
AppleCapsLockPressAndHoldToggleOff).
*/
class OSXCapsLockSwitch
{
public:
  OSXCapsLockSwitch();
  ~OSXCapsLockSwitch();

  OSXCapsLockSwitch(const OSXCapsLockSwitch &) = delete;
  OSXCapsLockSwitch &operator=(const OSXCapsLockSwitch &) = delete;

  //! Whether Caps Lock switches input sources on this Mac
  /*!
  Asks HIToolbox the way the system does. The TISRomanSwitchState preference
  is only written once the user changes the setting, and the feature is on by
  default for Chinese input sources, so a missing preference does not mean off.
  */
  static bool isEnabled();

  //! Whether Caps Lock is currently on
  static bool isCapsLockOn();

  //! Remote Caps Lock was pressed
  void press();

  //! Remote Caps Lock was released
  void release();

  //! Forget a pending press without acting on it
  void cancel();

private:
  struct State;
  struct HoldTimer;
  std::shared_ptr<State> m_state;
};
