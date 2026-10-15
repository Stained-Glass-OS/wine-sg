# Progress, drafter 7 (round 4)

## Round 4: SH-DIALOGS + SH-PROPSTORE (branch stubs-7e, patches 5810-)

### SH-PROPSTORE -- done (5810)
- Shell item property stores are backed by the file's registered property handler (read-write and read), array
  stores/lists/BHID_DataObject, IFileOperation SetProperties/ApplyProperties*.
- Gate: shell32 tests/shellitemprops.c (93 checks, both arches), five mutants shown failing; full shell32 suite
  64/32-bit: no new failures.

### SH-DIALOGS -- 5811 done (PickIconDlg, SHMultiFileProperties, SHFormatDrive, NetworkAddressControl)
- shelldialogs.c test: 101 checks both arches; mutants MPROP_NOAPPLY 4, FORMAT_NOCONFIRM 3, PICKICON_NOINDEX 3, NETADDR_NOMASK 2.
- SHFormatDrive: sg-admind "format" verb (wire format is a proposal, see patches/notes/5811.txt); tests use a regular file and a stand-in spool.
- Triaged: wiaservc AddDeviceDlg (scanner/camera, not touched), gamingtcui ShowProfileCardUI (no Xbox service).
- Open: rasdlg RasEntryDlgW.
