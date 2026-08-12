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

apt-get update
# debian:*-slim cleans archives after every apt invocation, so seed the cache
# after update and immediately before the single install transaction.
cp /tmp/*.deb /var/cache/apt/archives/
# WPS 11.1.0.11720 has three known metadata omissions: its postinst invokes
# hexdump without declaring the provider, libwpsmain loads libxslt.so.1, and
# its bundled Qt xcb plugin links libxkbcommon-x11.so.0.  Keep these as
# explicit package-level workarounds rather than copying individual libraries.
apt-get install -y --no-install-recommends \
    bsdextrautils icewm libxkbcommon-x11-0 libxslt1.1 xterm \
    /tmp/google-chrome.deb /tmp/wps-office.deb
dpkg --audit

mkdir -p /bionicx/apps/chrome/opt/google \
    /bionicx/apps/wps-office/opt /bionicx/metadata
mv /opt/google/chrome /bionicx/apps/chrome/opt/google/
mv /opt/kingsoft /bionicx/apps/wps-office/opt/

apt-mark showmanual | sort > /bionicx/metadata/manual-packages.txt
dpkg-query -W -f='${binary:Package}\t${Version}\n' | sort \
    > /bionicx/metadata/packages.tsv

# The deployed image is immutable and apt never runs on Android.  Keep dpkg's
# database and configured files, but discard download/cache/build-only state.
apt-get clean
rm -rf /var/lib/apt/lists/* /var/log/* /tmp/* /var/tmp/*
: > /etc/machine-id
