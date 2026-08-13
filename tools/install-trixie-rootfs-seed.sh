#!/bin/sh
set -eu

# This script runs inside the pinned ARM64 Debian build container.  It uses
# apt/dpkg normally so package dependencies, triggers and maintainer scripts
# define the filesystem; it is deliberately not an ELF-file copier.
snapshot="${BIONICX_DEBIAN_SNAPSHOT:?missing BIONICX_DEBIAN_SNAPSHOT}"
printf '%s\n' \
    'Types: deb' \
    "URIs: http://snapshot.debian.org/archive/debian/$snapshot/" \
    'Suites: trixie' \
    'Components: main' \
    'Architectures: arm64' \
    'Check-Valid-Until: no' \
    '' \
    'Types: deb' \
    "URIs: http://snapshot.debian.org/archive/debian-security/$snapshot/" \
    'Suites: trixie-security' \
    'Components: main' \
    'Architectures: arm64' \
    'Check-Valid-Until: no' \
    > /etc/apt/sources.list.d/debian.sources

export DEBIAN_FRONTEND=noninteractive
printf '#!/bin/sh\nexit 101\n' > /usr/sbin/policy-rc.d
chmod 0755 /usr/sbin/policy-rc.d
mkdir -p /var/cache/apt/archives/partial

apt-get -o Acquire::http::Pipeline-Depth=0 \
    -o Acquire::http::No-Cache=true update
# The host image is only the package-manager/runtime seed. Applications,
# desktop services and their dependencies belong to the device dpkg database
# and are installed by bxapt from the same signed snapshot.
apt-get -o Acquire::http::Pipeline-Depth=0 \
    -o Acquire::http::No-Cache=true install -y --no-install-recommends \
    apt binutils ca-certificates coreutils dash debian-archive-keyring dpkg \
    findutils grep patchelf sed systemd-standalone-sysusers
dpkg --audit

mkdir -p /bionicx/metadata
apt-mark showmanual | sort > /bionicx/metadata/manual-packages.txt
dpkg-query -W -f='${binary:Package}\t${Version}\n' | sort \
    > /bionicx/metadata/packages.tsv

# Downloads are transient; signed sources, keyrings and the dpkg database are
# retained so bxapt owns all subsequent package transactions on Android.
apt-get clean
rm -rf /var/lib/apt/lists/* /var/log/* /tmp/* /var/tmp/*
: > /etc/machine-id
