# Drafter 6 progress (core DLLs, gap-CORE.json, patches 5000-5499)

## 2026-10-13
- Setup: stubs-6 from origin/main; wine-10.0 tarball (SHA-256 checked by newbase.sh) + main's series, built with -j4.
- All 1186 gap-CORE items examined (one line each, evidence machine-checked: cited test lines exist and are ok()
  checks; windows-matches only with a test line or a fetched Microsoft page):
  windows-matches 84, not-applicable 135, no-ground-truth 189, escalate 396 (security-model items each with a
  Windows-matching proposal, escalate-6.md), IMPLEMENT 382 (open work, being done in CAREFUL MODE).
- triage-6.tsv holds every item that is not open work; implemented items leave it as their patches land.

## 2026-10-13, batch 1: patches 5000-5017 (36 items)
Each patch: a table-driven gate in test/ (64- and 32-bit, sourcing scratch-home.sh), mutants shown failing it,
notes in patches/notes/ ("CORE: integrator must review", with what is unverified). Full conformance suites
of every touched DLL (ntdll kernel32 kernelbase msvcrt advapi32 crypt32 bcrypt, 64 and 32) compared with the
series-applied base: no new failures (kernel32 loader/profile differ in both directions on base runs too).
The series applied to the tarball reproduces the build tree.
- 5000 ntdll/server: symbolic links keep a relative target as written (also one that climbs out of and back into
  the link's folder); deleting a directory link removes the link, not the folder (the bug drafter 5 found).
- 5001 kernel32 CreateSymbolicLinkA (item 307); 5002 ProcessSubsystemInformation (2); 5003 KeyValueFullInformationAlign64
  (24, 440); 5004 RtlFindLastBackwardRunSet/Clear (355, 356); 5005 RtlCompactHeap (357); 5006 FileAlignmentInformation (63).
- 5007 advapi32 EFS calls for a file system that cannot encrypt (60, 134-138); 5008 SetServiceBits validation
  (922 stays escalate: the bits are not announced); 5009 registry reflection keys (142, 144).
- 5010 kernel32 UMS -> ERROR_NOT_SUPPORTED (247-259); 5011 pipe session ids on unconnected ends (975, 976, six
  todo_wine removed); 5012 GetNamedPipeHandleState collection arguments (52, one todo_wine removed).
- 5013 crypt32 CryptBinaryToStringA CRYPT_STRING_HEX (49).
- 5014 bcrypt unknown algorithm -> STATUS_NOT_FOUND (9); 5015 BCryptImportKeyPair hImportKey (45); 5016
  BCryptDecrypt flags (18); 5017 AES key wrap export/import, RFC 3394 vectors (36, 814).
