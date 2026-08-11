# X11 ListExtensions (core opcode 99)

## Symptom

The real libX11 client returned zero extensions and Android logged
`Unsupported opcode 99`, despite five installed server extension handlers.
This prevents toolkits from discovering optional fast paths reliably.

## Root cause and correction

`ClientOpcodes` and `XClientRequestHandler` had no opcode 99 dispatch.
`ExtensionRequests.listExtensions` now serializes the protocol reply as a
count followed by length-prefixed Latin-1 names and four-byte tail padding.
The reply length is computed from the padded payload, as required by X11.

## x300 regression

```text
BXTEST PASS list-extensions count=5
BXINFO extension BIG-REQUESTS
BXINFO extension DRI3
BXINFO extension Present
BXINFO extension Composite
BXINFO extension GLX
BXSUMMARY passed=10 failed=2 observational_input=no
```

There is no longer an `Unsupported opcode 99` entry. The two remaining strict
failures are the separately tracked drawing pixel mismatch and PolyText8.
