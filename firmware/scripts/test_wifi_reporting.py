#!/usr/bin/env python3
"""Exercise full ESP32 Wi-Fi/config/reporting translation units with fake I/O."""

import os
from pathlib import Path
import subprocess
import tempfile


firmware = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="svitloboard-wifi-reporting-") as directory:
    executable = Path(directory) / "wifi-reporting"
    subprocess.run([
        os.environ.get("CXX", "c++"), "-std=c++11", "-O2", "-Wall", "-Wextra", "-Werror",
        "-DESP32", '-DSVB_BOARD_NAME="ESP32-C3-DevKitM-1"',
        "-I", str(firmware / "tests/wifi_reporting/stubs"),
        "-I", str(firmware / "include"),
        str(firmware / "src/AppConfig.cpp"),
        str(firmware / "src/WifiManager.cpp"),
        str(firmware / "src/SvitlobotClient.cpp"),
        str(firmware / "tests/wifi_reporting/test_wifi_reporting.cpp"),
        "-o", str(executable),
    ], check=True)
    subprocess.run([str(executable)], check=True)
