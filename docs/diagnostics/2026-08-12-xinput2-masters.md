# XInput2 master-device enumeration

## libXi initialization path

The real libXi client does not jump directly to `XIQueryVersion`. It first
queries `XInputExtension`, sends the legacy `GetExtensionVersion` request to
decide how many event slots are safe, then negotiates XI2 and finally issues
`XIQueryDevice(XIAllDevices)`. Omitting the legacy reply makes XI2 appear
unavailable even if requests 47 and 48 exist.

The wire layout was checked against the upstream libXi `XIQueryDevice.c`
parser. Each device record is followed immediately by its padded name and its
class records; BionicX currently returns zero class records.

## Controlled server model

`XInputExtension` advertises XI 2.0 and maps the existing BionicX input objects
to two enabled logical devices:

- device 2, `BionicX pointer`, use `XIMasterPointer`, attached to device 3;
- device 3, `BionicX keyboard`, use `XIMasterKeyboard`, attached to device 2.

`XIAllDevices`, `XIAllMasterDevices`, and either specific master ID are
supported. Invalid IDs return an error. Button, key, valuator, scroll and touch
classes and XI2 event selection/delivery are intentionally not claimed yet;
those need controlled event tests tied to Android injection.

## x300 proof

The tightened glibc/libXi probe checks both master roles and their non-self
attachments rather than only counting records:

```text
BXTEST PASS xinput2 version=2.0 devices=2 masters=1/1
BXSUMMARY desktop-x11 passed=4 failed=1 xerrors=0
```

The ordinary untraced execution is retained in
`evidence/x11-desktop-probe-xinput2.log`.
