Import("env")
import os
import shutil
import subprocess
from make_efu import make_efu
from SCons.Script import AlwaysBuild

def generate_efu(source, target, env):
    build_dir = env.subst("$PROJECT_BUILD_DIR")
    variant = env.subst("$PIOENV")
    project_dir = env.subst("$PROJECT_DIR")
    sketch_bin = os.path.join(build_dir, variant, "firmware.bin")
    fs_bin = os.path.join(build_dir, variant, "littlefs.bin")
    efu_out = os.path.join(build_dir, variant, f"{variant}.efu")

    # Check and build FS image using subprocess
    if not os.path.exists(fs_bin):
        print("[EFU] Filesystem image not found, building it...")
        result = subprocess.run(["pio", "run", "-t", "buildfs"], cwd=project_dir)
        if result.returncode != 0:
            print("[EFU ERROR] Failed to build LittleFS image.")
            return

    if not os.path.exists(sketch_bin):
        print(f"[EFU ERROR] firmware.bin not found: {sketch_bin}")
        return

    try:
        make_efu(sketch_bin, fs_bin, efu_out)
        print(f"[EFU] Successfully created: {efu_out}")

        # Optional: copy to output folder
        output_dir = os.path.join("firmware", "esp32")
        os.makedirs(output_dir, exist_ok=True)
        shutil.copy2(efu_out, os.path.join(output_dir, f"{variant}.efu"))
        print(f"[EFU] Copied to: {output_dir}")
    except Exception as e:
        print(f"[EFU ERROR] {e}")

# Always run this, even if PlatformIO skips recompiling
AlwaysBuild(env.Alias("buildprog", None, generate_efu))
