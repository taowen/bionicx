# glibc Vulkan Xlib surface control plane

The second controlled Vortek stage binds a genuine glibc/Xlib window to the
Android-hosted Vulkan service. The client advertises and enables
`VK_KHR_surface` and `VK_KHR_xlib_surface`; Vortek translates the X11 window ID
into its local surface object while the Bionic server obtains the live window
dimensions from BionicX's X server.

This exposed an upstream server-side bounds bug in instance-extension
enumeration. The server queried the host extension count but allocated its
temporary array using the count supplied by the remote caller. A normal
two-call Vulkan enumeration begins with a zero-capacity query, so the Android
loader could write the complete host extension list into a zero-sized
allocation. BionicX now always allocates from the host-reported count, then
applies the Android-surface filtering and Xlib-surface injection. It reports
the complete count for the first call and returns `VK_INCOMPLETE` with the
caller capacity for a short second call.

## Controlled verification

On x300 `01408BH601027129`, the untraced glibc process ran as the ordinary
application UID, created a 640x360 X11 window, and passed all 14 checks:

```text
BXTEST PASS host-vulkan-xlib-extensions status=0 returned=16 surface=1 xlib=1
BXTEST PASS host-vulkan-xlib-window display=open window=0x400001 size=640x360
BXTEST PASS host-vulkan-xlib-surface status=0 handle=valid window=0x400001
BXTEST PASS host-vulkan-presentation-support status=0 surface=1 xlib=1
BXTEST PASS host-vulkan-surface-capabilities status=0 extent=640x360 images=1..2 usage=0x9f
BXTEST PASS host-vulkan-surface-formats status=0 advertised=4 returned=4
BXTEST PASS host-vulkan-present-modes status=0 advertised=4 returned=4 fifo=1
BXSUMMARY host-vulkan passed=14 failed=0
```

Android again selected `/vendor/lib64/hw/vulkan.adreno.so`. This stage proves
the WSI discovery and X-window control path only. The next stage must create a
logical device and swapchain, submit a real clear/draw command, present the
result through `AHardwareBuffer`, and assert the visible Android pixels.
