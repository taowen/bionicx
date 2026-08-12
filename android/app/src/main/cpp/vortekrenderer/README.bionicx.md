# Vortek Android server integration

This directory is the Bionic server half from the Winlator-derived Android
source used by BionicX. It is not the guest ICD: that independently maintained
client is pinned at `third_party/vortek` as a Git submodule.

The server is kept with the Android integration because it directly uses
Winlator/BionicX JNI helpers, `XServer`, `GPUImage`, and `AHardwareBuffer`.
BionicX's baseline build removes the optional AdrenoTools custom-driver loader
and opens Android's `/system/lib64/libvulkan.so`, allowing the platform loader
to select the stock vendor HAL. The containing Android source is covered by
`android/LICENSE`.
