# STM32WL's CMSIS device header #defines RNG as a peripheral base-address
# macro ((RNG_TypeDef*)RNG_BASE). The Crypto library (attermann/Crypto)
# unconditionally declares a global object literally named RNG
# (Crypto/RNG.h: extern RNGClass RNG;) and RNG.cpp itself #includes
# <Arduino.h>, so on STM32WL the macro clobbers that declaration/definition
# and the build fails with "expected ')' before '*' token".
#
# A global `-include` build flag doesn't work here: it would also apply to
# the framework's own board/variant sources, which PlatformIO compiles from a
# different working directory where a relative path can't resolve. Instead,
# scope the fix to just the Crypto library's own build environment via
# PlatformIO's LibBuilder API: force-include microReticulum/Log.h (which
# already #undefs RNG right after <Arduino.h>, see Log.h) only when compiling
# Crypto's sources, so its own later <Arduino.h> include is a no-op (include
# guard) and never re-defines the macro.
Import("env")

import os

if env.get("PIOPLATFORM") == "ststm32":
    # Find microReticulum's Log.h whether this project *is* microReticulum
    # (this repo's own platformio.ini building itself) or merely *depends on*
    # it (an example project pulling it in via lib_deps).
    project_dir = env.subst("$PROJECT_DIR")
    log_h = os.path.join(project_dir, "src", "microReticulum", "Log.h")
    if not os.path.isfile(log_h):
        log_h = None
        for lb in env.GetLibBuilders():
            if lb.name == "microReticulum":
                log_h = os.path.join(lb.path, "src", "microReticulum", "Log.h")
                break

    if log_h and os.path.isfile(log_h):
        for lb in env.GetLibBuilders():
            if lb.name == "Crypto":
                lb.env.Append(CCFLAGS=["-include", log_h])
