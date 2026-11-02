#!/usr/bin/env python3
"""Published API pages stay aligned with the implementation.

MkDocs publishes docs/api/*.md. scripts/prepare_site.sh must not replace
those pages with include/*/README.md, which has lagged and previously
shipped the wrong TP segment default.
"""

from __future__ import annotations

import os
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def _posix_bash() -> str | None:
    """Return a bash that can run scripts/prepare_site.sh.

    On Windows, ``bash`` is often the WSL launcher in System32, which exits
    before the script runs. Prefer Git Bash, and reject a shell that cannot
    execute ``set -o pipefail``.
    """
    candidates: list[str] = []
    if os.name == "nt":
        program_files = os.environ.get("PROGRAMFILES", r"C:\Program Files")
        candidates.append(str(Path(program_files) / "Git" / "bin" / "bash.exe"))
        candidates.append(str(Path(program_files) / "Git" / "usr" / "bin" / "bash.exe"))
    candidates.append("bash")
    for candidate in candidates:
        try:
            proc = subprocess.run(
                [candidate, "-c", "set -euo pipefail; printf ok"],
                capture_output=True,
                text=True,
                timeout=30,
                check=False,
            )
        except (OSError, subprocess.TimeoutExpired):
            continue
        if proc.returncode == 0 and proc.stdout == "ok":
            return candidate
    return None


def _write_shell_script(destination: Path) -> None:
    """Copy prepare_site.sh with LF endings so Git Bash can execute it."""
    text = (ROOT / "scripts" / "prepare_site.sh").read_text(encoding="utf-8")
    normalized = text.replace("\r\n", "\n").replace("\r", "\n")
    destination.write_text(normalized, encoding="utf-8", newline="\n")


def _read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def test_tp_docs_match_default_payload() -> None:
    header = _read("include/tp/tp_types.h")
    assert "max_segment_size{1392}" in header

    for relative in (
        "docs/api/tp.md",
        "include/tp/README.md",
        "examples/advanced/large_messages/README.md",
    ):
        text = _read(relative)
        assert "1392" in text, relative
        assert "1400" not in text, relative
        assert "max_reassembly_time" not in text, relative
        assert "enable_compression" not in text, relative

    assert "enable_tp" in _read("docs/api/tp.md")
    assert "enable_tp" in _read("include/tp/README.md")


def test_api_pages_keep_post_020_behavior() -> None:
    e2e = _read("docs/api/e2e.md")
    assert "## Receive policy" in e2e
    assert "STACK_MANAGED" in e2e
    assert "unregister_profile" in e2e

    events = _read("docs/api/events.md")
    assert "set_default_endpoint" in events
    assert "set_message_rejection_handler" in events

    assert "set_message_rejection_handler" in _read("docs/api/rpc.md")
    assert "STACK_MANAGED" in _read("include/e2e/README.md")
    assert "set_message_rejection_handler" in _read("include/rpc/README.md")
    assert "set_message_rejection_handler" in _read("include/events/README.md")
    assert "MulticastState" in _read("include/sd/README.md")
    assert "sd_sequence_diagram.svg" in _read("docs/api/sd.md")


def test_prepare_site_does_not_replace_existing_api_pages(tmp_path: Path) -> None:
    project = tmp_path / "proj"
    (project / "scripts").mkdir(parents=True)
    (project / "docs" / "api").mkdir(parents=True)
    (project / "CONTRIBUTING.md").write_text("# contributing\n", encoding="utf-8")
    (project / "CHANGELOG.md").write_text("# changelog\n", encoding="utf-8")

    _write_shell_script(project / "scripts" / "prepare_site.sh")
    script_bytes = (project / "scripts" / "prepare_site.sh").read_bytes()
    assert b"\r" not in script_bytes

    for module in ("sd", "rpc", "tp", "events", "e2e"):
        mirror = project / "include" / module
        mirror.mkdir(parents=True)
        (mirror / "README.md").write_text(
            f"# stale {module}\n\n| default | 1400 bytes |\n",
            encoding="utf-8",
        )
        (project / "docs" / "api" / f"{module}.md").write_text(
            f"# kept {module}\n\ndefault 1392\nenable_tp\n",
            encoding="utf-8",
        )

    serial_dir = project / "include" / "serialization"
    serial_dir.mkdir(parents=True)
    (serial_dir / "README.md").write_text("# stale serialization\n", encoding="utf-8")
    (project / "docs" / "api" / "serialization.md").write_text(
        "---\ntitle: kept serialization\n---\n\n# kept serialization\n",
        encoding="utf-8",
    )

    bash = _posix_bash()
    if bash is None:
        if os.name == "nt":
            print("skip prepare_site: no working bash")
            return
        raise AssertionError("bash is required to run scripts/prepare_site.sh")

    proc = subprocess.run(
        [bash, "scripts/prepare_site.sh"],
        cwd=project,
        capture_output=True,
        text=True,
        check=False,
    )
    if proc.returncode != 0:
        raise AssertionError(
            f"{bash} scripts/prepare_site.sh exited {proc.returncode}\n{proc.stdout}\n{proc.stderr}"
        )

    published = (project / "docs" / "api" / "tp.md").read_text(encoding="utf-8")
    assert "kept tp" in published
    assert "1392" in published
    assert "1400" not in published
    for module in ("sd", "rpc", "events", "e2e"):
        text = (project / "docs" / "api" / f"{module}.md").read_text(encoding="utf-8")
        assert text.startswith(f"# kept {module}\n")

    serial = (project / "docs" / "api" / "serialization.md").read_text(encoding="utf-8")
    assert "kept serialization" in serial
    assert "stale serialization" not in serial


if __name__ == "__main__":
    import tempfile

    test_tp_docs_match_default_payload()
    test_api_pages_keep_post_020_behavior()
    with tempfile.TemporaryDirectory() as directory:
        test_prepare_site_does_not_replace_existing_api_pages(Path(directory))
    print("ok")
