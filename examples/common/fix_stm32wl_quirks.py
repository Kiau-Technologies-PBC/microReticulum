# Build-system workarounds for STM32WL (Nucleo-WL55JC1) quirks that show up
# when building microReticulum -- both are narrow, upstream compatibility
# gaps, not anything wrong with this project's own code.
Import("env")

import os

if env.get("PIOPLATFORM") == "ststm32":
    libs = env.GetLibBuilders()

    # --- Fix 1: RNG macro collision -----------------------------------
    # STM32WL's CMSIS device header #defines RNG as a peripheral
    # base-address macro ((RNG_TypeDef*)RNG_BASE). The Crypto library
    # (attermann/Crypto) unconditionally declares a global object literally
    # named RNG (Crypto/RNG.h: extern RNGClass RNG;) and RNG.cpp itself
    # #includes <Arduino.h>, so on STM32WL the macro clobbers that
    # declaration/definition and the build fails with "expected ')' before
    # '*' token".
    #
    # A global `-include` build flag doesn't work here: it would also apply
    # to the framework's own board/variant sources, which PlatformIO
    # compiles from a different working directory where a relative path
    # can't resolve. Instead, scope the fix to just the Crypto library's own
    # build environment: force-include microReticulum/Log.h (which already
    # #undefs RNG right after <Arduino.h>, see Log.h) only when compiling
    # Crypto's sources, so its own later <Arduino.h> include is a no-op
    # (include guard) and never re-defines the macro.

    # Find microReticulum's Log.h whether this project *is* microReticulum
    # (this repo's own platformio.ini building itself) or merely *depends
    # on* it (an example project pulling it in via lib_deps).
    project_dir = env.subst("$PROJECT_DIR")
    log_h = os.path.join(project_dir, "src", "microReticulum", "Log.h")
    if not os.path.isfile(log_h):
        log_h = None
        for lb in libs:
            if lb.name == "microReticulum":
                log_h = os.path.join(lb.path, "src", "microReticulum", "Log.h")
                break

    if log_h and os.path.isfile(log_h):
        for lb in libs:
            if lb.name == "Crypto":
                lb.env.Append(CCFLAGS=["-include", log_h])

    # --- Fix 2: RadioLib's STM32WL HAL needs the framework's SubGhz lib ---
    # RadioLib's hal/Stm32duino/Stm32wlHal.cpp (used for the chip's onboard
    # sub-GHz radio) #includes <SubGhz.h>, a small library bundled inside
    # framework-arduinoststm32/libraries/SubGhz that grants direct access to
    # the SUBGHZ peripheral's control signals. Unlike sibling framework
    # libraries (e.g. SPI), PlatformIO's LDF doesn't auto-resolve this
    # specific include for RadioLib -- a known upstream gap (see
    # jgromes/RadioLib#718). Only relevant if RadioLib is actually a
    # dependency (only STM32WL LoRa-capable examples need it).
    for lb in libs:
        if lb.name == "RadioLib":
            platform = env.PioPlatform()
            framework_dir = platform.get_package_dir("framework-arduinoststm32")
            if framework_dir:
                subghz_src = os.path.join(framework_dir, "libraries", "SubGhz", "src")
                if os.path.isdir(subghz_src):
                    # Let RadioLib's own sources find <SubGhz.h>...
                    lb.env.Append(CPPPATH=[subghz_src])
                    # ...but the header alone isn't enough: LDF never adds
                    # SubGhz as an actual library, so SubGhz.cpp is never
                    # compiled and the linker fails with undefined
                    # references to SubGhzClass's methods. Build and link it
                    # explicitly -- via RadioLib's own (LDF-resolved) env, so
                    # SubGhz.cpp's own #include <SPI.h> resolves too, without
                    # having to separately hunt down SPI's include path.
                    subghz_lib = lb.env.BuildLibrary(
                        os.path.join("$BUILD_DIR", "SubGhzLib"),
                        subghz_src,
                    )
                    env.Append(LIBS=[subghz_lib])
            break
