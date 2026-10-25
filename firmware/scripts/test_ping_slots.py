#!/usr/bin/env python3
"""Run the production slot scheduler on the host without ESP libraries."""

import os
from pathlib import Path
import subprocess
import tempfile

firmware = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="svitloboard-slots-") as directory:
    binary = str(Path(directory) / "ping-slots")
    subprocess.run([
        os.environ.get("CXX", "c++"), "-std=c++11", "-Wall", "-Wextra", "-Werror",
        "-I", str(firmware / "include"), str(firmware / "tests/ping_slots.cpp"),
        "-o", binary,
    ], check=True)
    subprocess.run([binary], check=True)
