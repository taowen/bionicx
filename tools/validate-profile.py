#!/usr/bin/env python3
import json
import re
import sys
from pathlib import Path


def fail(message: str) -> None:
    raise SystemExit(f"invalid BionicX profile: {message}")


if len(sys.argv) != 2:
    raise SystemExit(f"usage: {sys.argv[0]} PROFILE.json")

path = Path(sys.argv[1])
try:
    profile = json.loads(path.read_text(encoding="utf-8"))
except (OSError, json.JSONDecodeError) as error:
    fail(str(error))

if profile.get("schemaVersion") != 2:
    fail("schemaVersion must be 2")
if "compatibility" in profile:
    fail("compatibility modules were removed; every profile uses the runtime contract")
if not re.fullmatch(r"[a-z0-9][a-z0-9._-]{0,63}", profile.get("id", "")):
    fail("id must be a safe lowercase identifier")
launch = profile.get("launch")
if not isinstance(launch, dict) or not isinstance(launch.get("executable"), str):
    fail("launch.executable is required")
mode = launch.get("mode", "loader")
if mode not in ("direct", "loader"):
    fail("launch.mode must be direct or loader")
if mode == "loader" and not all(
        isinstance(launch.get(key), str) and launch[key]
        for key in ("loader", "libraryPath")):
    fail("loader mode requires loader and libraryPath")
for flag in ("diagnoseSignals", "debugStop"):
    if flag in launch and not isinstance(launch[flag], bool):
        fail(f"launch.{flag} must be boolean")
display = profile.get("display", {})
if display.get("socket", "abstract") not in ("abstract", "filesystem"):
    fail("display.socket must be abstract or filesystem")
dpi = display.get("dpi", 144)
if not isinstance(dpi, int) or not 72 <= dpi <= 400:
    fail("display.dpi must be an integer in 72..400")
for name in launch.get("environment", {}):
    if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", name):
        fail(f"invalid environment variable: {name}")
    if name in {"LD_PRELOAD", "BIONICX_ROOTFS", "BIONICX_TMPDIR",
                "BIONICX_DNS_SERVERS"}:
        fail(f"launch.environment may not override reserved runtime variable {name}")
host_services = profile.get("hostServices", [])
if not isinstance(host_services, list) or any(
        service not in ("dbus", "pulseaudio", "vulkan") for service in host_services):
    fail("hostServices may only contain dbus, pulseaudio and vulkan")
if len(host_services) != len(set(host_services)):
    fail("hostServices must not contain duplicates")

print(f"profile {profile['id']} is valid")
