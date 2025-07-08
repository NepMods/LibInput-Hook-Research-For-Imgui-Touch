**LibInput-Touch-Intercept-For-ImGui**

A research-oriented codebase and documentation for hooking Android's `libinput.so` touch-event paths to integrate with Dear ImGui on AOSP Android 15+ builds.

> **Credits**: **NepMods**.
>
> **Note**: This report has been **refactored by AI** because my native language isnt english.

---

## Table of Contents

1. [Background](#background)
2. [Problem Statement](#problem-statement)
3. [Symbol Analysis](#symbol-analysis)
4. [Hooking Strategy](#hooking-strategy)
5. [Implementation](#implementation)

   * [Loading & Binding](#loading--binding)
   * [Custom Hook Function](#custom-hook-function)
   * [Hook Setup](#hook-setup)
6. [Usage](#usage)
7. [Limitations & Future Work](#limitations--future-work)
8. [References](#references)

---

## Background

Dear ImGui is a popular immediate-mode GUI library often used in game engines and tooling. On Android, touch input must be captured from the platform's input subsystem (e.g., via `libinput.so`) and forwarded into ImGui's `AInputEvent` handler for proper interaction.

Older Android releases (up to Android 14) exposed the symbol:

```cpp
_ZN7android13InputConsumer21initializeMotionEventEPNS_11MotionEventEPKNS_12InputMessageE
```

which could be hooked to intercept and inject touch events into ImGui.

## Problem Statement

Starting with AOSP Android 15 and newer builds, the symbol `initializeMotionEvent` is no longer exported. Instead, internal input handling has been refactored, and the equivalent functionality resides behind a new symbol:

```cpp
_ZN7android13InputConsumer7consumeEPNS_26InputEventFactoryInterfaceEblPjPPNS_10InputEventE
```

Hooking the new symbol is non-trivial due to:

* Different argument list and calling conventions.
* Obscured or inlined code paths in the latest `InputTransport.cpp`.

This documentation outlines the symbol analysis, hooking approach using **DobbyHook**, and example implementation with **xdl** symbol resolution.

## Symbol Analysis

1. **Legacy Symbol** (`initializeMotionEvent`)

   * Signature:

     ```cpp
     status_t initializeMotionEvent(MotionEvent* outEvent,
                                    const InputMessage* message);
     ```
   * Exposed in `InputConsumer` class and located in `InputTransport.cpp`.
   * Provided a straightforward hook target for intercepting raw touch data.

2. **New Symbol** (`consume`)

   * Signature (demangled):

     ```cpp
     status_t consume(InputEventFactoryInterface* factory,
                     bool /*raw or cooked*/,
                     long /*sequence id*/,
                     uint32_t* outPolicyFlags,
                     InputEvent** outEvent);
     ```
   * Responsible for reading raw `InputMessage`s and producing higher-level `InputEvent` objects (e.g., `MotionEvent`).
   * Requires hooking at the factory-consume level and extracting the resulting `InputEvent*` pointer.

## Hooking Strategy

* **Target Method**: `android::InputConsumer::consume` in `libinput.so`.
* **Hooking Library**: **DobbyHook**.
* **Symbol Resolver**: **xdl** (`xdl_open`, `xdl_sym`).
* **Extraction**: After calling the original method, inspect the output pointer (`InputEvent**`) to retrieve the newly created `AInputEvent*` object.
* **Injection**: Forward the `AInputEvent*` to `ImGui_ImplAndroid_HandleInputEvent` for processing.

## Implementation

### Loading & Binding

```cpp
#include <xdl.h>
#include <dobby.h>
#include <android/log.h>
#include <imgui_impl_android.h>

static int32_t (*origConsume)(void* consumer,
                              void* factory,
                              bool isRaw,
                              long sequenceId,
                              uint32_t* outPolicyFlags,
                              void** outEventPtr);

extern "C" void setup_input_consumer_hook() {
    void* libIn = xdl_open("libinput.so", RTLD_NOW);
    if (!libIn) {
        __android_log_print(ANDROID_LOG_ERROR, "NepMods", "Failed to open libinput.so");
        return;
    }

    const char* symbol = "_ZN7android13InputConsumer7consumeEPNS_26InputEventFactoryInterfaceEblPjPPNS_10InputEventE";
    void* addr = xdl_sym(libIn, symbol, nullptr);
    __android_log_print(ANDROID_LOG_INFO, "NepMods", "Found consume() at %p", addr);

    if (addr) {
        DobbyHook(addr,
                  (void*)myConsume,
                  (void**)&origConsume);
    } else {
        __android_log_print(ANDROID_LOG_ERROR, "NepMods", "Symbol not found: %s", symbol);
    }
}
```

### Custom Hook Function

```cpp
int32_t myConsume(void* consumer,
                  void* factory,
                  bool isRaw,
                  long sequenceId,
                  uint32_t* outPolicyFlags,
                  void** outEventPtr) {
    int32_t result = origConsume(consumer,
                                 factory,
                                 isRaw,
                                 sequenceId,
                                 outPolicyFlags,
                                 outEventPtr);

    if (outEventPtr && *outEventPtr) {
        AInputEvent* event = reinterpret_cast<AInputEvent*>(*outEventPtr);
        ImGui_ImplAndroid_HandleInputEvent(event);
    }

    return result;
}
```

### Hook Setup

Invoke `setup_input_consumer_hook()` early in your library's initialization (e.g., `JNI_OnLoad`).

## Usage

* Launch your target application with `LD_PRELOAD` set to your hook library.
* Verify that ImGui touch interactions work on Android 15+ builds.
* Use `adb logcat -s NepMods` for debugging.

## Limitations & Future Work

* **Lifecycle Handling**: Ensure hooks survive app background/foreground transitions.
* **Multiple Input Types**: Extend hooks to handle keyboard, joystick, and other input events.
* **Symbol Stability**: Investigate symbol obfuscation or version-specific mangling changes.
* **Automated Symbol Discovery**: Integrate scripts to parse exported symbols and auto-generate hook signatures.

## References

* AOSP `InputTransport.cpp` (Android 15):
  [https://android.googlesource.com/platform/frameworks/native/+/14e8b01/libs/input/InputTransport.cpp](https://android.googlesource.com/platform/frameworks/native/+/14e8b01/libs/input/InputTransport.cpp)
* Dear ImGui Android Backend:
  [https://github.com/ocornut/imgui/blob/master/backends/imgui\_impl\_android.cpp](https://github.com/ocornut/imgui/blob/master/backends/imgui_impl_android.cpp)

* DobbyHook Library:
    [https://github.com/jmpews/Dobby](https://github.com/jmpews/Dobby)
* xdl Symbol Resolver:
    [https://github.com/hexhacking/xDL](https://github.com/hexhacking/xDL)

---

