#!/usr/bin/env python3
"""Exercise the production slot POST parser and UI functions on the host.

No third-party packages are needed: a C++11 compiler and Node.js suffice.
Only Arduino/server/DOM boundaries are stubbed; function bodies come from the
current firmware sources so behavior changes are exercised by these tests.
"""

import os
from pathlib import Path
import re
import subprocess
import tempfile


firmware = Path(__file__).resolve().parents[1]


def cpp_method(source: str, name: str) -> str:
    start = source.index(f"bool WebApp::{name}(")
    opening = source.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


def js_function(source: str, name: str) -> str:
    start = re.search(rf"(?:^|\n)((?:async )?function {name}\()", source)
    if start is None:
        raise ValueError(f"Missing UI function {name}")
    following = re.search(r"\n(?:async )?function ", source[start.end():])
    end = start.end() + following.start() if following else len(source)
    return source[start.start(1):end].strip()


with tempfile.TemporaryDirectory(prefix="svb-slot-ui-tests-") as temporary:
    output = Path(temporary)
    server = (firmware / "src/WebApp.cpp").read_text(encoding="utf-8")
    (output / "slot_api_production.inc").write_text(
        cpp_method(server, "parseInteger") + "\n" + cpp_method(server, "parsePingSlots"),
        encoding="utf-8",
    )
    executable = output / "slot-api-tests"
    subprocess.run([
        os.environ.get("CXX", "c++"), "-std=c++11", "-O2", "-Wall", "-Wextra", "-Werror",
        "-I", str(output), "-I", str(firmware / "include"),
        str(firmware / "tests/slot_ui/api.cpp"), "-o", str(executable),
    ], check=True)
    subprocess.run([str(executable)], check=True)

    ui = (firmware / "include/WebUi.h").read_text(encoding="utf-8")
    script = output / "slot_ui_production.js"
    script.write_text(
        js_function(ui, "updatePingSlots") + "\n" + js_function(ui, "load"), encoding="utf-8"
    )
    subprocess.run([
        os.environ.get("NODE", "node"), str(firmware / "tests/slot_ui/controls.js"), str(script)
    ], check=True)
