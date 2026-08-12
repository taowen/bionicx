#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
snapshot=c2f4ad4534f4637b543a9a3b085e28f50cf6d01c
base_url="https://raw.githubusercontent.com/brunodev85/winlator-app/$snapshot/app/src/main/jniLibs/arm64-v8a"
destination="$repo_dir/android/app/src/main/audioJniLibs/arm64-v8a"
mkdir -p "$destination"

files=(
    'libpulseaudio.so 2f5dd7e2c828e570787182eb85b21350b7ac9dbc1da0e1988d0cb48de7ed4ef6'
    'libpulsecore-13.0.so 364d57e94cd9dbd7a52fe6182d64321cded3efd88c1c3dbd07a2d2b11cd487ed'
    'libpulse.so db4dbb786d613ff0496a5c104eba85afb7fa67a890c91cfe6b2b5a7ea385a8cd'
    'libpulsecommon-13.0.so 96abf45e5981723d855b69d63d72ddc2033898128ee6d6df036f518349681014'
    'libsndfile.so a499fbaadb35b594e831183f21109b0b9b279ffeedb25ef7badc7da936809f0b'
    'libltdl.so e6c7a402a53b78bacf7312cd5c7e7d9aef5f15cd6e0bc847912522ca09a250c4'
)
for entry in "${files[@]}"; do
    read -r name checksum <<< "$entry"
    target="$destination/$name"
    if [[ ! -f "$target" ]] || ! printf '%s  %s\n' "$checksum" "$target" | sha256sum -c - >/dev/null 2>&1; then
        curl -fL --retry 3 "$base_url/$name" -o "$target.download"
        printf '%s  %s\n' "$checksum" "$target.download" | sha256sum -c -
        mv "$target.download" "$target"
    fi
done

printf '%s\n' "$destination"
