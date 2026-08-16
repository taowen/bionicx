# Durable Krita, qBittorrent and KeePassXC workflows

Device `01408BH601027129`, seed
`ed998c095a1b9384b1f022d06101ac3fc3c61761ac751546bd84edca298e44e2`.
Packages remain `ii`: Krita `1:5.2.9+dfsg-1+deb13u1`, qBittorrent
`5.1.0-2`, KeePassXC `2.7.10+dfsg1-1`. No `--runtime-root`.

## KeePassXC

`examples/keepassxc/seed-db.sh` is 6/6 (`db-create` / `add` / `ls` /
`show` / `reopen` / `persist`, 1918-byte `.kdbx`). The untraced
`keepassxc` profile then opens that same file through
`keepassxc-deferred-open` + D-Bus. The compositor shows Title `login`,
Username `bionicx`, URL `https://example.com`, notes `BionicX-probe`.
A second `keepassxc-cli show` after the GUI still returns those three
fields.

## qBittorrent

The 256 KiB web-seed payload hashes
`a68590ec9ed4b1530a44cfb5f9df3457503ebc106d1b0124d865ca217f38537d`
before launch, after `am force-stop`, and after a cold relaunch. The
matching `.fastresume` stays in `BT_backup`. Both GUI shots show
`bionicx-network-payload.bin` at 256.0 KiB / 100% / Seeding. Launching
with the torrent argument again raises the “already in the transfer
list” merge dialog; the completed transfer is already the durable
state.

## Krita

`examples/krita-glx-destroy-probe` is 4/4 (`destroy-null`,
`choose-fbconfig`, `create-new`, `destroy-created`). Untraced Krita
opens `bionicx-image.ppm` (640×480 four-quadrant canvas plus yellow
circle). `profiles/krita-export.json` then runs `krita --nosplash
--export` as the primary process on the live X display. Device logcat:
`krita exited with 0`. The written
`files/homes/krita/Documents/bionicx-saved.png` is an 8-bit RGBA PNG,
IHDR 640×480, 7968 bytes, same four-quadrant image.

Rerun `examples/popular-workflows/run.sh` to recapture under `build/evidence/`.
