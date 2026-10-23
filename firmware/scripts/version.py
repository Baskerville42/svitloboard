"""Inject reproducible build metadata without editing source files."""

import os
import subprocess
from pathlib import Path

Import("env")  # type: ignore[name-defined]  # Provided by PlatformIO/SCons.

version = os.environ.get("SVB_FIRMWARE_VERSION")
if version:
    safe = "".join(char for char in version if char.isalnum() or char in ".-+_")
    if safe != version or not safe:
        raise ValueError("SVB_FIRMWARE_VERSION contains unsupported characters")
    # SCons otherwise strips the quotes and turns a version such as
    # 2.0.0-dev.abc1234 into invalid C++ tokens.
    env.Append(CPPDEFINES=[("SVB_FIRMWARE_VERSION", '\\\"%s\\\"' % safe)])

project_root = Path(env.subst("$PROJECT_DIR")).parent
commit = os.environ.get("SVB_GIT_COMMIT") or os.environ.get("GITHUB_SHA")
if commit:
    commit = commit[:7]
else:
    try:
        commit = subprocess.check_output(
            ["git", "rev-parse", "--short=7", "HEAD"],
            cwd=project_root,
            text=True,
        ).strip()
        dirty = subprocess.check_output(
            ["git", "status", "--porcelain", "--untracked-files=normal"],
            cwd=project_root,
            text=True,
        ).strip()
        if dirty:
            commit += "-dirty"
    except (OSError, subprocess.CalledProcessError):
        commit = "unknown"

safe_commit = "".join(char for char in commit if char.isalnum() or char in ".-+_")
if not safe_commit:
    safe_commit = "unknown"
env.Append(CPPDEFINES=[("SVB_GIT_COMMIT", '\\\"%s\\\"' % safe_commit)])
