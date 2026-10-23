"""Generate the deterministic gzip-compressed UI shared by every target."""

from gzip import compress
from pathlib import Path


def generate_one(project_dir: Path, source_name: str, output_name: str) -> None:
    source_path = project_dir / "include" / source_name
    output_path = project_dir / "include" / output_name
    source = source_path.read_text(encoding="utf-8")
    start_marker = 'R"HTML('
    end_marker = ')HTML";'
    start = source.index(start_marker) + len(start_marker)
    end = source.rindex(end_marker)
    payload = compress(source[start:end].encode("utf-8"), compresslevel=9, mtime=0)
    rows = []
    for offset in range(0, len(payload), 16):
        chunk = payload[offset : offset + 16]
        rows.append("  " + ", ".join(f"0x{byte:02x}" for byte in chunk) + ",")
    generated = (
        "#pragma once\n\n"
        f"// Generated from {source_name}. Do not edit by hand.\n"
        "static const uint8_t kWebUiGzip[] PROGMEM = {\n"
        + "\n".join(rows)
        + "\n};\n"
        + f"constexpr size_t kWebUiGzipSize = {len(payload)};\n"
    )
    if not output_path.exists() or output_path.read_text(encoding="utf-8") != generated:
        output_path.write_text(generated, encoding="utf-8")


def generate(project_dir: Path) -> None:
    generate_one(project_dir, "WebUi.h", "WebUiGzip.h")


try:
    Import("env")  # type: ignore[name-defined]  # PlatformIO/SCons injects Import.
except NameError:
    if __name__ == "__main__":
        generate(Path(__file__).resolve().parents[1])
else:
    generate(Path(env.subst("$PROJECT_DIR")))  # type: ignore[name-defined]
