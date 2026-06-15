# ese pre-processor project
# (C) 2026 ESE Project Contributors
# Author: Ryan Pratt

import argparse
import sys
from pathlib import Path

from .parser import ParseError
from .scanner import iter_source_files, process_file
from .validator import ValidationError


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="esepp")
    parser.add_argument("root", nargs="?", default=".", help="project root to scan")
    args = parser.parse_args(argv)

    root = Path(args.root).resolve()
    if not root.exists():
        print(f"esepp error: root does not exist: {root}", file=sys.stderr)
        return 1
    if not root.is_dir():
        print(f"esepp error: root is not a directory: {root}", file=sys.stderr)
        return 1

    files = iter_source_files(root)
    blocks_found = 0
    generated_updated = 0
    had_error = False

    for path in files:
        try:
            result = process_file(path)
        except (ParseError, ValidationError) as error:
            print(f"esepp error: {path}", file=sys.stderr)
            print(error, file=sys.stderr)
            had_error = True
            continue
        blocks_found += result.blocks_found
        generated_updated += result.generated_sections_updated

    if had_error:
        return 1

    print(f"files scanned: {len(files)}")
    print(f"ESE blocks found: {blocks_found}")
    print(f"generated sections updated: {generated_updated}")
    return 0
