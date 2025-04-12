# post_efu.py
Import("env")
import os
import subprocess
from datetime import datetime
from SCons.Script import AlwaysBuild
from make_efu import make_efu

variant = env.subst("$PIOENV")
build_dir = env.subst("$PROJECT_BUILD_DIR")
project_dir = env.subst("$PROJECT_DIR")

sketch_bin = os.path.join(build_dir, variant, "firmware.bin")
fs_bin = os.path.join(build_dir, variant, "littlefs.bin")
efu_out = os.path.join(build_dir, variant, f"{variant}.efu")

output_dir = os.path.join(project_dir, "firmware", "EFU")
os.makedirs(output_dir, exist_ok=True)

def after_build(source, target, env):
    if not os.path.exists(sketch_bin):
        print("[EFU ERROR] Sketch binary not found.")
        return

    if not os.path.exists(fs_bin):
        print("[EFU] Filesystem image not found, building...")
        subprocess.run(["pio", "run", "-t", "buildfs"], cwd=project_dir)

    if not os.path.exists(fs_bin):
        print("[EFU ERROR] Filesystem still missing after build attempt.")
        return

    # Build raw .efu file
    print(f"[EFU] Creating EFU: {efu_out}")
    make_efu(sketch_bin, fs_bin, efu_out)

    # Validate and timestamp copy to firmware/EFU
    timestamp = datetime.now().strftime('%Y%m%d')
    new_name = f"{variant}_{timestamp}.efu"
    final_path = os.path.join(output_dir, new_name)

    efu_tool = os.path.join(project_dir, "efu_tool.py")
    result = subprocess.run([
        "C:\\Users\\jay\\.platformio\\python3\\python.EXE", efu_tool,
        "--efu", efu_out,
        "--project", project_dir,
        "--env", variant,
        "--output", output_dir
    ])

    if result.returncode != 0:
        print("[EFU TOOL] ❌ EFU validation failed!")
    else:
        print(f"[EFU TOOL] ✅ EFU validated and saved to {final_path}")

AlwaysBuild(env.Alias("post_efu", "buildprog", after_build))

