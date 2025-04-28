# .scripts/fix_includes.py
# ESP32 Include Path Patch
Import("env")

# Detect framework and platform
frameworks = env.get("PIOFRAMEWORK", [])
platform = env.get("PLATFORM", "")

# Only fix if it's an ESP32 Arduino project
if "arduino" in frameworks and "espressif32" in platform:
    print("Patching includes for ESP32 Arduino build...")

    BAD_INCLUDES = [
        "-I.pio/packages/framework-arduinoespressif32/tools/sdk/esp32/include",
        "-I.pio/packages/framework-arduinoespressif32/tools/sdk/esp32/include/lwip",
    ]

    for bad_path in BAD_INCLUDES:
        if bad_path in env['BUILD_FLAGS']:
            print(f"Removing bad include: {bad_path}")
            env['BUILD_FLAGS'].remove(bad_path)

    CORRECT_INCLUDE = "-I.pio/packages/framework-arduinoespressif32-libs/esp32/include"

    if CORRECT_INCLUDE not in env['BUILD_FLAGS']:
        print(f"Adding correct include: {CORRECT_INCLUDE}")
        env.Append(BUILD_FLAGS=[CORRECT_INCLUDE])
else:
    print("No fix needed. Skipping include patch.")