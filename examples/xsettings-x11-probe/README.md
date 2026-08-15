# XSETTINGS manager probe

libX11 only. One two-connection client covers the xfsettingsd register
path: timestamp `PropertyNotify`, `_XSETTINGS_S0` selection, the
`_XSETTINGS_SETTINGS` property, `MANAGER` `ClientMessage`, and
`RESOURCE_MANAGER`, including under `GrabServer`.

```sh
ANDROID_SERIAL=<serial> examples/xsettings-x11-probe/install-and-run.sh
```
