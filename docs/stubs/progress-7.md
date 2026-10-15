# Progress, drafter 7 (round 4)

## Round 4: SH-DIALOGS + SH-PROPSTORE (branch stubs-7e, patches 5810-)

### SH-PROPSTORE -- done (5810)
- Shell item property stores are backed by the file's registered property handler (read-write and read), array
  stores/lists/BHID_DataObject, IFileOperation SetProperties/ApplyProperties*.
- Gate: shell32 tests/shellitemprops.c (93 checks, both arches), five mutants shown failing; full shell32 suite
  64/32-bit: no new failures.
