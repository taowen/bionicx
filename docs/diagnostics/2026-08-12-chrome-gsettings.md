# Chrome GLib settings schema

## Symptom

Every Chrome cold launch emitted a GLib-GIO critical from
`g_settings_schema_source_lookup` because the extracted library closure had no
compiled GSettings schema source. Letting GIO choose dconf would additionally
make a small desktop preference API depend on the still-absent session D-Bus.

## Correction

The reproducible Chrome bundle now copies the four GTK schemas supplied by its
locked Debian dependency set and compiles them at build time. The profile points
`GSETTINGS_SCHEMA_DIR` at that app-private, architecture-independent database
and selects GLib's memory backend. Chrome owns its durable browser preferences;
ephemeral GTK chooser/theme settings do not need a dconf daemon merely to read
their defaults.

## Result

On x300, a cold ordinary-UID launch followed by Android-injected navigation to
Example Domain had zero GSettings/schema-source criticals, zero dconf accesses,
and zero fatal/NSS/FD-ownership/network-service failures. The full-screen page
remained correctly rendered. The compiled database SHA-256 was
`1e621796723a48f5e78a16ccf078f3effb347df58eaf13d0686be121964fcb0f`.

See `evidence/chrome-gsettings-network.log` and
`evidence/chrome-gsettings-network.png`.
