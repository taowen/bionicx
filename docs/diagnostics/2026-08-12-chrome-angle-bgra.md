# Chrome ANGLE BGRA capability negotiation

Chrome 151 reached its GPU process after the DRI3 and program-binary fixes,
but every GPU raster tile failed with:

```
Could not find SharedImageBackingFactory with params: usage:
DisplayRead|RasterWrite, format: BGRA_8888, gmb_type: empty
```

Chromium's matching `GLCommonImageBackingFactory` only admits BGRA when its
`FeatureInfo` sees `GL_EXT_texture_format_BGRA8888`. Gladio previously exposed
only the desktop spelling `GL_EXT_bgra`.

Adding the GLES spelling alone was not sufficient for Chrome's ANGLE layer.
`SystemInfo.getInfo` showed `ANGLE_OPENGL`, `GaneshGL`, and no BGRA extension in
ANGLE's client-facing extension list. The exact bundled ANGLE revision
`a17d5224d83f` enables `bgraTexImageFormatsBroken` for non-Mesa Qualcomm
vendors and then forcibly clears `textureFormatBGRA8888EXT`. That workaround
targets a native Qualcomm GL driver, while the API observed by ANGLE is the
Gladio translation driver and its BGRA path is implemented differently.

The renderer now reports `BionicX Gladio` as its GL vendor instead of leaking
the host ICD vendor. This prevents upper layers from applying native-driver
workarounds to the translation layer while retaining the actual host GPU in
device diagnostics. Chrome then stopped emitting SharedImage factory failures
and advanced to real Skia rendering calls. The next missing call is
`glVertexAttribIPointer`; framebuffer/stencil and shader failures occur after
that unimplemented call and are tracked separately.

The controlled GLX test now requires the Chromium-recognized extension name,
the translation-driver identity, and a real BGRA texture/FBO clear/readback on
the device's host GLES driver. Device `01408BH601027129` passed all 23 checks
and the Android screenshot compositor check.
