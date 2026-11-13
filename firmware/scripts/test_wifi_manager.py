#!/usr/bin/env python3
"""Run the production Wi-Fi state machine with asynchronous host Wi-Fi events."""

import os
from pathlib import Path
import subprocess
import tempfile


firmware = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="svitloboard-wifi-") as directory:
    for platform in ("ESP8266", "ESP32"):
        executable = Path(directory) / f"wifi-manager-{platform.lower()}"
        subprocess.run([
            os.environ.get("CXX", "c++"), "-std=c++11", "-Wall", "-Wextra", "-Werror",
            f"-D{platform}",
            "-I", str(firmware / "tests/wifi_manager/stubs"),
            "-I", str(firmware / "include"),
            str(firmware / "src/WifiManager.cpp"),
            str(firmware / "tests/wifi_manager/test_wifi_manager.cpp"),
            "-o", str(executable),
        ], check=True)
        subprocess.run([str(executable)], check=True)
