# cupsGetDests sees the controlled bionicx-test destination

On `01408BH601027129` an app-private `cupsd` listened on
`files/run/cups/run/cups.sock` with `FileDevice Yes`. `lpadmin` created
the `bionicx-test` `file:` destination. The glibc probe called
`cupsGetDests()` with `CUPS_SERVER` set to that bare socket path:

```text
cups-probe: destinations=1
cups-probe: destination=bionicx-test instance=
```

The probe is `examples/cups-probe/cups-probe.c`. It does not replace the
shared seed. `tests/test-cups-probe-profile.sh` checks the profile and
the destination name contract.
