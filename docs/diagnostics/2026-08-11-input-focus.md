# Android-to-X11 input observation

## Initial observation

ADB-injected key, tap, and swipe actions produced zero events in the first
probe. The Android input bridge was initially suspected.

## Isolation

The probe selected KeyPress/Button/Motion masks but did not provide
`WM_HINTS.window_group`. BionicX's Winlator-derived desktop helper recognizes a
mapped window as an application window only when its name is nonempty and its
window-group points to itself. Consequently the test had no reliable focused
window for keyboard routing.

The client now sets the standard self-group WM_HINT and explicitly performs
`XSetInputFocus`. The explicit focus request is itself a strict regression;
this separates core X11 focus behavior from the host's window-management
policy.

## x300 regression

The run injected Android keycode A, a tap, and a swipe:

```text
BXTEST PASS input-focus
BXOBS input-events keys=1 buttons=1 motions=61
BXSUMMARY passed=13 failed=0 observational_input=yes
x11-probe exited with 0
```

No server correction was necessary. This note is retained because correcting
the controlled client prevented a false diagnosis and an inappropriate input
routing workaround.
