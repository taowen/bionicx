# Chrome ANGLE Vulkan on Adreno 750

Device `HA27DTL0` (Lenovo TB321FU / Xiaoxin Pad Pro, Android 14 / API 34,
Snapdragon 8 Gen 3 `pineapple`, `vulkan.adreno`, 4 KiB, 2560x1600). Seed
`ed998c095a1b9384b1f022d06101ac3fc3c61761ac751546bd84edca298e44e2`. No
root; UID `u0_a286`.

`vulkan-frames` is `burst=2048 signal=0` and compositor `blue=230400`
(same as vivo Mali). Hello paints. Chrome 151 is the same pinned deb.

`chrome-smoke.json` plus `open-gpu.sh` opens `chrome://gpu` and the page
paints. GPU0 is `ANGLE (Qualcomm, Vulkan 1.3.128 (Vortek (Adreno (TM)
750) (0x43051401)), … Adreno Vulkan Driver-512.762.18)`. Canvas /
compositing / WebGL are hardware accelerated. The `Vulkan: Disabled`
line is Chrome's native Skia-Vulkan path (`SkiaGraphite` stays off);
ANGLE is still on the Vortek Vulkan ICD.

vivo `10AFA31610002QH` (Mali-G1-Ultra) now paints the same
`chrome://gpu` page after `WaitForFences` stopped blocking the RPC
thread and stopped exporting SYNC_FD for that wait.

See `evidence/HA27DTL0/hello.png`, `vulkan-frames.png`, and
`chrome-smoke.png`.
