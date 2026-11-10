#!/usr/bin/env python3
"""Exercise production request control flow and URLs with mocked hardware."""

import os
from pathlib import Path
import subprocess
import tempfile


firmware = Path(__file__).resolve().parents[1]
source = (firmware / "src/SvitlobotClient.cpp").read_text(encoding="utf-8")


def function(signature: str) -> str:
    start = source.index(signature)
    opening = source.index("{", start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


with tempfile.TemporaryDirectory(prefix="svb-client-tests-") as temporary:
    output = Path(temporary)
    (output / "svitlobot_client_production.inc").write_text("\n".join(function(signature) for signature in [
        "void SvitlobotClient::begin(", "void SvitlobotClient::loop(",
        "void SvitlobotClient::requestPingNow(", "void SvitlobotClient::requestPingAfter(",
        "void SvitlobotClient::queueOffNotification(", "void SvitlobotClient::cancelQueuedOffNotification(",
        "void appendUrlEncoded(", "String SvitlobotClient::requestUrl(",
        "String SvitlobotClient::statusPageUrl(",
    ]), encoding="utf-8")
    executable = output / "svitlobot-client-tests"
    subprocess.run([
        os.environ.get("CXX", "c++"), "-std=c++11", "-O2", "-Wall", "-Wextra", "-Werror",
        '-DSVB_BOARD_NAME="D1-mini"', '-DSVB_FIRMWARE_VERSION="2.4.0-dev.abcdefg"',
        "-I", str(output), "-I", str(firmware / "tests/config_store/stubs"),
        "-I", str(firmware / "include"), str(firmware / "tests/svitlobot_client.cpp"),
        "-o", str(executable),
    ], check=True)
    subprocess.run([str(executable)], check=True)
