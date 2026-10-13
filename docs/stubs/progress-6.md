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

## 2026-10-13, batch 2: patches 5018-5032 (53 items; 91 implemented so far)
Same method as batch 1; suites rpcrt4 combase ole32 oleaut32 gdi32 user32 ntdll kernel32 kernelbase bcrypt crypt32
(64 and 32) compared with the base: no new failures (kernel32 loader/profile flakes as in batch 1).
- 5018 rpcrt4 NdrClearOutParameters (26): frees and clears [out] parameters after a failed proxy call, only once the
  proxy is unmarshalling, never the caller's own variable.
- 5019 combase CoGetDefaultContext for the current apartment (30); the other apartments' default contexts stay a FIXME.
- 5020 user32 DdeDisconnect from the server side, XTYP_DISCONNECT to the client (43).
- 5021 gdi32 EMR_SMALLTEXTOUT playback (40; the escape and colour records stay unplayed, see the notes).
- 5022 ntdll RtlZombifyActivationContext (389, three todo_wine removed); 5023 ThreadIsIoPending access check (23);
  5024 RtlDeleteTimerQueueEx returns at once with an event (384); 5025 FileCompressionInformation (12).
- 5026 bcrypt STATUS_NOT_SUPPORTED for unknown properties and modes (802-811).
- 5027 crypt32 CERT_RDN_UNIVERSAL_STRING encode/decode/text (170, 171, 208, 209); 5028 invalid add disposition
  E_INVALIDARG for CRLs/CTLs (180, 205); 5029 CERT_CLOSE_STORE_CHECK_FLAG for collection/provider stores (178, 192, 213).
- 5030-5032 ole32 call_as proxy/stub wrappers: LockServer, bind options, IsRunning, BindToObject (468-477);
  FLAG_STGMEDIUM and IDataObject::SetData (464-467, 478, 479); IViewObject and ResizeBorder (480-489).

## 2026-10-13, batch 3: patches 5033-5056 (52 items; 143 implemented so far)
Same method; suites oleaut32 combase ole32 rpcrt4 kernelbase kernel32 shlwapi win32u gdi32 user32 ntoskrnl.exe
(64 and 32) compared with the base: no new failures (todo_wine removed where a test now passes). ntoskrnl patches
are gated with a probe kernel driver (test/sg-driver.h, sg-driver-host.h; 64-bit, a WoW64 host skips).
- 5033 oleaut32 HFONT user marshalling (525-528); 5034 call_as IsName/FindName/GetLibStatistics/CreateInstance
  (535-542); 5036 IsName/FindName ignore case (found while testing 5034, a todo_wine removed).
- 5035 combase HPALETTE user marshalling (154-157); 5039 IContextCallback::ContextCallback in the same context (831).
- 5037 oleaut32 PropertyBag reads every VARIANT type, QuickActivate (543-546); 5038 IRecordInfo GetField,
  PutField, GetFieldNoCopy (551, 897-899).
- 5040 rpcrt4 FC_WSTRING tables (604-606, 992).
- 5041 kernelbase SHRegDeleteUSValue / SHRegDeleteEmptyUSKey (977-980); 5042 SHRegCreateUSKeyW flags (841);
  5043 URLIS_APPLIABLE (54, 353); 5044 URL_ESCAPE_ASCII_URI_COMPONENT (53); 5045 GEO_RFC1766 (22).
- 5046 win32u EnableMouseInPointer is set once per process (702; input/win32u todo marks follow); 5047, 5048
  D3DKMTOpenAdapterFromHdc / FromDeviceName (668, 670); 5049 Get/SetColorAdjustment (664, 665); 5050 GetPixel
  outside the clip region is CLR_INVALID (found); 5051 LockWindowUpdate (696).
- 5052 ntoskrnl driver object extensions (1073, 1074); 5053 KeInitializeDpc / KeSetImportanceDpc (1128, 1129);
  5054 KeQueryGroupAffinity / KeGetCurrentProcessorNumberEx (1132, 1133); 5055 IoSetThreadHardErrorMode (1138);
  5056 ObReferenceObjectByPointer (1116).
- Decided while drafting (recorded in triage-6.tsv): 1163 MmCopyVirtualMemory and 1098 KeQueryPriorityThread
  escalate (no kernel/user split; Wine keeps relative priorities); 1146/1147 ProbeForRead/Write escalate (needs SEH
  in a driver to gate); 1164 ExSetTimerResolution windows-matches (fixed 1 ms).
