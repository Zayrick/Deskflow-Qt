/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/OSXCapsLockSwitch.h"

#include "base/Log.h"
#include "platform/OSXAutoTypes.h"

#include <Carbon/Carbon.h>
#include <IOKit/hidsystem/IOHIDLib.h>
#include <IOKit/hidsystem/IOHIDParameter.h>
#include <dispatch/dispatch.h>
#include <dlfcn.h>

#include <cstdint>
#include <mutex>

namespace {

// macOS asks to "press and hold" Caps Lock without giving a duration; this is
// about how long the built-in keyboard takes before the Caps Lock light comes on.
const int64_t kHoldDelayNs = 500 * NSEC_PER_MSEC;

const CFStringRef kHIToolboxDomain = CFSTR("com.apple.HIToolbox");

// HIToolbox functions the system uses for its own Caps Lock switch. They are
// not in the public headers, so they are looked up at run time and every
// caller has a public API fallback.
void *hiToolbox()
{
  static void *handle =
      dlopen("/System/Library/Frameworks/Carbon.framework/Frameworks/HIToolbox.framework/HIToolbox", RTLD_LAZY);
  return handle;
}

template <typename T> T hiToolboxSymbol(const char *name)
{
  void *handle = hiToolbox();
  return handle ? reinterpret_cast<T>(dlsym(handle, name)) : nullptr;
}

bool boolProperty(TISInputSourceRef source, CFStringRef key)
{
  auto value = static_cast<CFBooleanRef>(TISGetInputSourceProperty(source, key));
  return value && CFBooleanGetValue(value);
}

AutoCFArray copyKeyboardInputSources(bool selectableOnly)
{
  const void *keys[] = {kTISPropertyInputSourceCategory, kTISPropertyInputSourceIsSelectCapable};
  const void *values[] = {kTISCategoryKeyboardInputSource, kCFBooleanTrue};
  AutoCFDictionary filter(
      CFDictionaryCreate(
          nullptr, keys, values, selectableOnly ? 2 : 1, &kCFTypeDictionaryKeyCallBacks,
          &kCFTypeDictionaryValueCallBacks
      ),
      CFRelease
  );
  return AutoCFArray(TISCreateInputSourceList(filter.get(), false), CFRelease);
}

bool preferenceSaysEnabled()
{
  CFPreferencesAppSynchronize(kHIToolboxDomain);
  Boolean exists = false;
  const CFIndex state = CFPreferencesGetAppIntegerValue(CFSTR("TISRomanSwitchState"), kHIToolboxDomain, &exists);
  if (exists) {
    return state != 0;
  }

  // Never changed by the user: on whenever an enabled input source supports it.
  auto key = hiToolboxSymbol<const CFStringRef *>("kTISPropertyInputSourceDoesCapsLockSwitch");
  if (key == nullptr) {
    return false;
  }
  AutoCFArray sources = copyKeyboardInputSources(false);
  for (CFIndex i = 0; sources && i < CFArrayGetCount(sources.get()); ++i) {
    auto source = (TISInputSourceRef)CFArrayGetValueAtIndex(sources.get(), i);
    if (boolProperty(source, *key)) {
      return true;
    }
  }
  return false;
}

bool pressAndHoldTogglesOff()
{
  CFPreferencesAppSynchronize(kHIToolboxDomain);
  return CFPreferencesGetAppBooleanValue(CFSTR("AppleCapsLockPressAndHoldToggleOff"), kHIToolboxDomain, nullptr);
}

io_connect_t openHIDSystem()
{
  io_service_t service = IOServiceGetMatchingService(MACH_PORT_NULL, IOServiceMatching(kIOHIDSystemClass));
  if (service == 0) {
    return 0;
  }
  io_connect_t connect = 0;
  if (IOServiceOpen(service, mach_task_self(), kIOHIDParamConnectType, &connect) != KERN_SUCCESS) {
    connect = 0;
  }
  IOObjectRelease(service);
  return connect;
}

void setCapsLock(bool on)
{
  io_connect_t connect = openHIDSystem();
  if (connect == 0) {
    LOG_WARN("unable to open the HID system to set caps lock");
    return;
  }
  IOHIDSetModifierLockState(connect, kIOHIDCapsLockState, on);
  IOServiceClose(connect);
  LOG_DEBUG("caps lock %s", on ? "on" : "off");
}

// Switch between the last used Latin input source and the one the system
// would switch to, like a Caps Lock tap on the Mac's own keyboard.
void toggleInputSource()
{
  using CopySource = TISInputSourceRef (*)();
  static auto copyNonRoman = hiToolboxSymbol<CopySource>("TISCopyCurrentNonRomanInputSourceForRomanSwitch");

  std::lock_guard<std::mutex> lock(g_tisMutex);

  AutoTISInputSourceRef current(TISCopyCurrentKeyboardInputSource(), CFRelease);
  if (!current) {
    return;
  }

  AutoTISInputSourceRef target(nullptr, CFRelease);
  if (!boolProperty(current.get(), kTISPropertyInputSourceIsASCIICapable)) {
    target.reset(TISCopyCurrentASCIICapableKeyboardInputSource());
  } else {
    if (copyNonRoman) {
      target.reset(copyNonRoman());
    }
    if (!target) {
      AutoCFArray sources = copyKeyboardInputSources(true);
      for (CFIndex i = 0; sources && i < CFArrayGetCount(sources.get()); ++i) {
        auto source = (TISInputSourceRef)CFArrayGetValueAtIndex(sources.get(), i);
        if (!boolProperty(source, kTISPropertyInputSourceIsASCIICapable)) {
          target.reset((TISInputSourceRef)CFRetain(source));
          break;
        }
      }
    }
  }

  if (!target || CFEqual(target.get(), current.get())) {
    return;
  }

  if (OSStatus status = TISSelectInputSource(target.get()); status != noErr) {
    LOG_WARN("failed to switch input source: %d", static_cast<int>(status));
    return;
  }
  auto id = static_cast<CFStringRef>(TISGetInputSourceProperty(target.get(), kTISPropertyInputSourceID));
  char name[256] = "";
  if (id) {
    CFStringGetCString(id, name, sizeof(name), kCFStringEncodingUTF8);
  }
  LOG_DEBUG("caps lock switched input source to %s", name);
}

} // namespace

struct OSXCapsLockSwitch::State
{
  std::mutex mutex;
  // Bumped on every press, release and cancel so a stale hold timer does nothing.
  uint64_t generation = 0;
  bool pending = false;
  bool wasOn = false;
};

struct OSXCapsLockSwitch::HoldTimer
{
  std::weak_ptr<State> state;
  uint64_t generation;
};

OSXCapsLockSwitch::OSXCapsLockSwitch() : m_state(std::make_shared<State>())
{
  // do nothing
}

OSXCapsLockSwitch::~OSXCapsLockSwitch()
{
  cancel();
}

bool OSXCapsLockSwitch::isEnabled()
{
  using IsEnabled = Boolean (*)();
  static auto isRomanSwitchEnabled = hiToolboxSymbol<IsEnabled>("TISIsRomanSwitchEnabled");
  if (isRomanSwitchEnabled) {
    return isRomanSwitchEnabled();
  }
  return preferenceSaysEnabled();
}

bool OSXCapsLockSwitch::isCapsLockOn()
{
  return (CGEventSourceFlagsState(kCGEventSourceStateHIDSystemState) & kCGEventFlagMaskAlphaShift) != 0;
}

void OSXCapsLockSwitch::press()
{
  std::lock_guard<std::mutex> lock(m_state->mutex);
  ++m_state->generation;
  m_state->pending = false;
  m_state->wasOn = isCapsLockOn();

  if (m_state->wasOn && !pressAndHoldTogglesOff()) {
    setCapsLock(false);
    return;
  }

  m_state->pending = true;
  auto *timer = new HoldTimer{m_state, m_state->generation};
  dispatch_after_f(
      dispatch_time(DISPATCH_TIME_NOW, kHoldDelayNs), dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), timer,
      [](void *context) {
        std::unique_ptr<HoldTimer> timer(static_cast<HoldTimer *>(context));
        auto state = timer->state.lock();
        if (!state) {
          return;
        }
        std::lock_guard<std::mutex> lock(state->mutex);
        if (state->pending && state->generation == timer->generation) {
          // Held long enough: this is Caps Lock, not an input source switch.
          state->pending = false;
          setCapsLock(!state->wasOn);
        }
      }
  );
}

void OSXCapsLockSwitch::release()
{
  bool tap = false;
  {
    std::lock_guard<std::mutex> lock(m_state->mutex);
    ++m_state->generation;
    tap = m_state->pending;
    m_state->pending = false;
  }
  if (tap) {
    toggleInputSource();
  }
}

void OSXCapsLockSwitch::cancel()
{
  std::lock_guard<std::mutex> lock(m_state->mutex);
  ++m_state->generation;
  m_state->pending = false;
}
