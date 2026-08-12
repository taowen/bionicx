#!/usr/bin/env python3
"""Build a small deterministic ODF 1.3 Writer document."""

from pathlib import Path
import sys
import zipfile


EPOCH = (1980, 1, 1, 0, 0, 0)


def entry(name: str, data: str, compressed: bool = True) -> zipfile.ZipInfo:
    info = zipfile.ZipInfo(name, EPOCH)
    info.compress_type = zipfile.ZIP_DEFLATED if compressed else zipfile.ZIP_STORED
    info.external_attr = 0o100644 << 16
    return info


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit(f"usage: {sys.argv[0]} OUTPUT.odt")
    destination = Path(sys.argv[1])
    destination.parent.mkdir(parents=True, exist_ok=True)
    content = """<?xml version="1.0" encoding="UTF-8"?>
<office:document-content office:version="1.3"
 xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0"
 xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0">
 <office:body><office:text>
  <text:h text:outline-level="1">LibreOffice Writer on BionicX</text:h>
  <text:p>Debian trixie ARM64 glibc plus X11 on Android.</text:p>
  <text:p>EDIT_BELOW</text:p>
 </office:text></office:body>
</office:document-content>
"""
    manifest = """<?xml version="1.0" encoding="UTF-8"?>
<manifest:manifest manifest:version="1.3"
 xmlns:manifest="urn:oasis:names:tc:opendocument:xmlns:manifest:1.0">
 <manifest:file-entry manifest:full-path="/" manifest:media-type="application/vnd.oasis.opendocument.text"/>
 <manifest:file-entry manifest:full-path="content.xml" manifest:media-type="text/xml"/>
</manifest:manifest>
"""
    with zipfile.ZipFile(destination, "w") as archive:
        archive.writestr(entry("mimetype", "", False), "application/vnd.oasis.opendocument.text")
        archive.writestr(entry("content.xml", ""), content)
        archive.writestr(entry("META-INF/manifest.xml", ""), manifest)


if __name__ == "__main__":
    main()
