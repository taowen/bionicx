# XDamage family probe

libX11 only. One two-connection client covers DAMAGE 1.1
`QueryVersion`, `Create`, `Add` (`DamageNotify`), `Subtract`,
`PutImage`-triggered `DamageNotify` and `Destroy`, including under
`GrabServer`.

```sh
ANDROID_SERIAL=<serial> examples/damage-x11-probe/install-and-run.sh
```
