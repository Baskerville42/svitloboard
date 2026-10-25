#!/usr/bin/env python3
"""Compile the production ConfigStore and AppConfig against an in-memory host FS."""

import os
from pathlib import Path
import subprocess
import tempfile


firmware = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="svb-config-tests-") as temporary:
    executable = Path(temporary) / "config-store-tests"
    subprocess.run([
        os.environ.get("CXX", "c++"), "-std=c++11", "-O2", "-Wall", "-Wextra", "-Werror",
        "-DESP8266", "-DSVB_CONFIG_SCHEMA=3",
        "-I", str(firmware / "tests/config_store/stubs"),
        "-I", str(firmware / "include"),
        str(firmware / "src/ConfigStore.cpp"),
        str(firmware / "src/AppConfig.cpp"),
        str(firmware / "tests/config_store/test_config_store.cpp"),
        "-o", str(executable),
    ], check=True)
    subprocess.run([str(executable)], check=True)
