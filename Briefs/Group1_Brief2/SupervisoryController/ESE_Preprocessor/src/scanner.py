# ese pre-processor project
# (C) 2026 ESE Project Contributors
# Author: Ryan Pratt

import re
from dataclasses import dataclass
from pathlib import Path

from .generator_cpp import generate_cpp
from .parser import parse_ese_block
from .validator import validate_machine


SUPPORTED_EXTENSIONS = {".cpp", ".hpp", ".h", ".cc", ".cxx", ".hxx"}
IGNORED_FOLDERS = {".git", "build", "cmake-build-debug", "cmake-build-release", ".vs", ".idea"}
ESE_BLOCK_RE = re.compile(r"/\*\s*#ESE-BEGIN(?P<body>.*?)#ESE-END\s*\*/", re.DOTALL)


@dataclass(frozen=True)
class ProcessResult:
    text: str
    changed: bool
    blocks_found: int
    generated_sections_updated: int


@dataclass(frozen=True)
class FileResult:
    path: Path
    changed: bool
    blocks_found: int
    generated_sections_updated: int


def iter_source_files(root: Path) -> list[Path]:
    files: list[Path] = []
    for path in root.rglob("*"):
        if any(part in IGNORED_FOLDERS for part in path.parts):
            continue
        if path.is_file() and path.suffix in SUPPORTED_EXTENSIONS:
            files.append(path)
    return sorted(files)


def process_file(path: Path) -> FileResult:
    original = path.read_text(encoding="utf-8")
    result = process_text(original, str(path))
    if result.changed:
        path.write_text(result.text, encoding="utf-8")
    return FileResult(
        path=path,
        changed=result.changed,
        blocks_found=result.blocks_found,
        generated_sections_updated=result.generated_sections_updated,
    )


def process_text(source: str, filename: str = "<memory>") -> ProcessResult:
    output: list[str] = []
    cursor = 0
    blocks_found = 0
    updated = 0

    for match in ESE_BLOCK_RE.finditer(source):
        blocks_found += 1
        block_text = match.group(0)
        body = match.group("body")
        machine = parse_ese_block(body)
        validate_machine(machine)
        generated = generate_cpp(machine)

        output.append(source[cursor : match.end()])
        existing_end = _generated_section_end(source, match.end(), machine.name)
        if existing_end is None:
            output.append("\n\n")
            cursor = match.end()
        else:
            cursor = existing_end
            output.append("\n\n")
        output.append(generated)
        updated += 1

    output.append(source[cursor:])
    new_source = "".join(output)
    return ProcessResult(
        text=new_source,
        changed=new_source != source,
        blocks_found=blocks_found,
        generated_sections_updated=updated if new_source != source else 0,
    )


def _generated_section_end(source: str, start: int, machine_name: str) -> int | None:
    pattern = re.compile(
        rf"\s*//\s*#ESE-GENERATED-BEGIN:\s*{re.escape(machine_name)}\b.*?"
        rf"//\s*#ESE-GENERATED-END:\s*{re.escape(machine_name)}\b[^\n]*",
        re.DOTALL,
    )
    match = pattern.match(source, start)
    return None if match is None else match.end()
