"""Create the single ESP32 factory image required by ESP Web Tools."""

from pathlib import Path
import subprocess

Import("env")  # type: ignore[name-defined]  # Provided by PlatformIO/SCons.


def merge_factory_image(source, target, env):
    if env.get("PIOPLATFORM") != "espressif32":
        return

    build_dir = Path(env.subst("$BUILD_DIR"))
    platform = env.PioPlatform()
    esptool = Path(platform.get_package_dir("tool-esptoolpy")) / "esptool.py"
    framework = Path(platform.get_package_dir("framework-arduinoespressif32"))
    boot_app0 = framework / "tools" / "partitions" / "boot_app0.bin"
    board = env.BoardConfig()
    chip = board.get("build.mcu", "esp32")
    flash_size = board.get("upload.flash_size", "4MB")
    bootloader_offset = "0x1000" if chip == "esp32" else "0x0"
    output = build_dir / "firmware.factory.bin"

    command = [
        env.subst("$PYTHONEXE"), str(esptool), "--chip", chip, "merge_bin",
        "-o", str(output), "--flash_mode", "dio", "--flash_freq", "40m",
        "--flash_size", str(flash_size),
        bootloader_offset, str(build_dir / "bootloader.bin"),
        "0x8000", str(build_dir / "partitions.bin"),
        "0xe000", str(boot_app0),
        "0x10000", str(build_dir / "firmware.bin"),
    ]
    subprocess.check_call(command)


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_factory_image)
