# Security

BionicX profiles are not sandboxes. A launched ELF and every library or preload
module execute as native code with all permissions granted to the BionicX app
UID. Only install bundles you trust and verify their hashes before activation.

The runtime intentionally avoids root and long-lived tracing. The WPS migration
helper uses root solely to copy files from a different Android app's private
directory; do not use it on paths you have not inspected.

Profiles may reference only a fixed token set, but they can still select any
file readable by the app UID. Treat write access to `files/profiles/active.json`
as equivalent to native-code execution.
