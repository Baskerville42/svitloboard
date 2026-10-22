"""Inject a release version without editing source or platformio.ini."""

import os

Import("env")  # type: ignore[name-defined]  # Provided by PlatformIO/SCons.

version = os.environ.get("SVB_FIRMWARE_VERSION")
if version:
    safe = "".join(char for char in version if char.isalnum() or char in ".-+_")
    if safe != version or not safe:
        raise ValueError("SVB_FIRMWARE_VERSION contains unsupported characters")
    # SCons otherwise strips the quotes and turns a version such as
    # 2.0.0-dev.abc1234 into invalid C++ tokens.
    env.Append(CPPDEFINES=[("SVB_FIRMWARE_VERSION", '\\\"%s\\\"' % safe)])
