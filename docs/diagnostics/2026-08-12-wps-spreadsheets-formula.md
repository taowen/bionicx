# WPS Spreadsheets formula, save, and cold reopen

## Functional path

The final `profiles/wps-spreadsheets.json` profile was installed without
`diagnoseSignals`, Frida, a debugger, root, PRoot, or Termux. On x300
`01408BH601027129`, genuine AArch64 WPS Spreadsheets completed this workflow:

1. create an empty workbook from the full-screen home interface;
2. enter `12` in A1 and `30` in A2 through Android-injected X11 keyboard input;
3. enter the real formula `=SUM(A1:A2)` in A3 and render its result as `42`;
4. save the workbook as OOXML `Book1.xlsx` in the app-private Documents folder;
5. force-stop BionicX, cold-launch the untraced profile, use the WPS Open dialog,
   and reopen the saved workbook with `12`, `30`, and `42` visibly intact.

The cold launch log contains `bionicx-exec: running untraced`, has no
`--diagnose-signals`, and records no process exit, unsupported X opcode, or
`BadImplementation` during the workflow.

## Structural assertion

`examples/wps/verify-xlsx.sh` pulls a named workbook through unprivileged
Android `run-as`, checks ZIP integrity and required OOXML members, then asserts
cell values and formulas from `xl/worksheets/sheet1.xml`. The device artifact
passed:

```text
BXTEST PASS wps-xlsx archive=Book1.xlsx members=16 cells=3
BXCELL A1 value=12 formula=
BXCELL A2 value=30 formula=
BXCELL A3 value=42 formula=SUM(A1:A2)
```

This proves formula persistence independently of the rendered screenshot. See
`evidence/wps-spreadsheets-formula.log`,
`evidence/wps-spreadsheets-formula.png`, and
`evidence/wps-spreadsheets-formula-cold-reopen.png`.
