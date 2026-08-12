#!/usr/bin/env python3
"""Resolve and optionally copy the recursive DT_NEEDED closure of ELF files."""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
from collections import deque
from pathlib import Path


NEEDED = re.compile(r"\(NEEDED\).*\[(.+)]")
RPATH = re.compile(r"\((?:RUNPATH|RPATH)\).*\[(.+)]")
INTERPRETER = re.compile(r"Requesting program interpreter:\s*(.+?)]")


def elf_metadata(readelf: str, path: Path) -> tuple[list[str], list[str], str | None]:
    dynamic = subprocess.run(
        [readelf, "-d", str(path)], text=True, capture_output=True, check=False
    )
    if dynamic.returncode != 0:
        raise RuntimeError(f"readelf -d failed for {path}: {dynamic.stderr.strip()}")
    needed: list[str] = []
    runpaths: list[str] = []
    for line in dynamic.stdout.splitlines():
        match = NEEDED.search(line)
        if match:
            needed.append(match.group(1))
        match = RPATH.search(line)
        if match:
            runpaths.extend(item for item in match.group(1).split(":") if item)

    program = subprocess.run(
        [readelf, "-l", str(path)], text=True, capture_output=True, check=False
    )
    match = INTERPRETER.search(program.stdout)
    return needed, runpaths, match.group(1) if match else None


def expand_runpath(value: str, owner: Path) -> Path:
    origin = str(owner.parent)
    return Path(value.replace("${ORIGIN}", origin).replace("$ORIGIN", origin))


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--entry", type=Path, action="append", required=True)
parser.add_argument("--search-root", type=Path, action="append", required=True)
parser.add_argument("--readelf", default=os.environ.get("READELF", "readelf"))
parser.add_argument("--copy-to", type=Path)
parser.add_argument(
    "--exclude-copy-root",
    type=Path,
    action="append",
    default=[],
    help="resolve objects below this root but do not duplicate them in --copy-to",
)
parser.add_argument("--json", type=Path)
args = parser.parse_args()

entry_arguments = list(args.entry)
entries = [path.resolve() for path in entry_arguments]
roots = [path.resolve() for path in args.search_root]
excluded_copy_roots = [path.resolve() for path in args.exclude_copy_root]
for path in entries:
    if not path.is_file():
        parser.error(f"entry does not exist: {path}")
for path in roots:
    if not path.is_dir():
        parser.error(f"search root does not exist: {path}")
for path in excluded_copy_roots:
    if not path.is_dir():
        parser.error(f"excluded copy root does not exist: {path}")

index: dict[str, list[Path]] = {}
for root in roots:
    for path in root.rglob("*"):
        if path.is_file() or path.is_symlink():
            index.setdefault(path.name, []).append(path.resolve())

queue = deque(entries)
seen: dict[Path, dict[str, object]] = {}
missing: dict[str, list[str]] = {}
copy_names: dict[Path, set[str]] = {}
for argument, path in zip(entry_arguments, entries):
    # An explicit runtime-loaded entry may be a SONAME symlink. Preserve the
    # requested basename: resolving it before recording the copy name would
    # leave dlopen("libfoo.so.N") with only libfoo.so.N.x.y on the device.
    copy_names.setdefault(path, set()).add(argument.name)
while queue:
    owner = queue.popleft().resolve()
    if owner in seen:
        continue
    needed, runpaths, interpreter = elf_metadata(args.readelf, owner)
    resolved: dict[str, str] = {}
    local_search = [expand_runpath(value, owner) for value in runpaths]
    for soname in needed:
        candidates = []
        candidates.extend(path / soname for path in local_search if (path / soname).exists())
        candidates.extend(index.get(soname, []))
        candidates = list(dict.fromkeys(path.resolve() for path in candidates))
        if not candidates:
            missing.setdefault(soname, []).append(str(owner))
            continue
        dependency = candidates[0]
        resolved[soname] = str(dependency)
        copy_names.setdefault(dependency, set()).add(soname)
        queue.append(dependency)
    seen[owner] = {
        "needed": needed,
        "resolved": resolved,
        "runpath": runpaths,
        "interpreter": interpreter,
    }

report = {
    "entries": [str(path) for path in entries],
    "excludedCopyRoots": [str(path) for path in excluded_copy_roots],
    "objects": {str(path): metadata for path, metadata in sorted(seen.items())},
    "missing": missing,
    "copyNames": {
        str(path): sorted(names) for path, names in sorted(copy_names.items())
    },
}

if args.copy_to:
    args.copy_to.mkdir(parents=True, exist_ok=True)
    copied: dict[str, Path] = {}
    for path in seen:
        if any(path.is_relative_to(root) for root in excluded_copy_roots):
            continue
        for name in sorted(copy_names.get(path, {path.name})):
            existing = copied.get(name)
            if existing and existing.read_bytes() != path.read_bytes():
                raise SystemExit(f"SONAME collision for {name}: {existing} and {path}")
            destination = args.copy_to / name
            if not destination.exists():
                shutil.copy2(path, destination, follow_symlinks=True)
            copied[name] = path

rendered = json.dumps(report, indent=2, sort_keys=True) + "\n"
if args.json:
    args.json.write_text(rendered, encoding="utf-8")
else:
    print(rendered, end="")
if missing:
    raise SystemExit(3)
