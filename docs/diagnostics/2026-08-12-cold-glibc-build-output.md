# Cold Android-glibc build output contract

## Symptom

Adding Fontconfig/Xft packages changed the cached builder-image hash and forced
the first full Android-glibc rebuild in this checkout. `build-android-glibc.sh`
prints upstream patch and compiler progress before printing its output path.
`examples/hello/build-bundle.sh` captured the entire stdout stream as one shell
variable, so its later `cp` treated build logs plus the path as a filename and
failed with `File name too long`. Cache-hit builds had hidden this defect.

## Correction and verification

The generic hello bundle now mirrors the full producer output to stderr with
`tee` and consumes only its final stdout line as the artifact directory. The
already completed cold-build result was then consumed successfully, and both
the Fontconfig/Xft and existing desktop probe bundles resolved complete ELF
dependency closures.

This is a packaging-interface correction only; it does not alter the glibc
recipe or runtime binaries.
