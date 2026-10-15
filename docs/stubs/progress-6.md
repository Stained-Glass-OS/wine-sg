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

## 2026-10-13, batch 4: patches 5057-5063 (39 items; 182 implemented so far)
Same method; suites cryptowinrt windows.security.authentication.onlineid combase ole32 (64 and 32) compared with the
base: no new failures (web.core has no conformance tests; its gates cover it).
- 5057 cryptowinrt, 5059 onlineid, 5060 web.core: IInspectable on the factories, statics, objects and async operations
  (GetIids lists the interfaces, a factory or statics object answers GetRuntimeClassName with E_ILLEGAL_METHOD_CALL as
  documented, operations are named IAsyncOperation`1<result class> as Windows names them in cryptowinrt's and
  userconsentverifier's tests; BaseTrust). 5060 also IAsyncInfo::get_Id. (1000-1005, 1012-1014, 722-739, 911-916,
  741-745, 757-759, 920.) The boolean operation's class name no longer counts the terminating null in its length.
- 5058 CryptographicBuffer ConvertStringToBinary / ConvertBinaryToString (1016, 1017).
- 5061 StringMap IMap::GetView, a snapshot (747).
- 5062 combase IComThreadingInfo::SetCurrentLogicalThreadId (166); 5063 IContext properties on the thread object
  context (832-835).

## 2026-10-13, batch 5: patches 5064-5071 (8 items; 190 implemented so far)
Same method; suites ntdll kernel32 kernelbase advapi32 shlwapi (64 and 32) compared with the base: no new failures
(kernel32 profile.c:1560 fails on the base's 64-bit run too; loader as in the earlier batches).
- 5064 kernelbase SetProcessAffinityUpdateMode keeps the mode, QueryProcessAffinityUpdateMode (311).
- 5065 kernel32 Wow64GetThreadSelectorEntry (260).
- 5066 kernelbase UrlFixupW reproduces the documented corrections (352).
- 5067 advapi32 GetCurrentHwProfileW/A from the registry, keeping Wine's old GUID as the default (98).
- 5068 ntdll NtQueryDirectoryFile with an event, APC or completion port (62).
- 5069 ntdll FileFsObjectIdInformation is the file system's UUID (46).
- 5070 the registry quota used is the registry's size, the same in GetSystemRegistryQuota and ntdll (974).
- 5071 GetNumberOfConsoleMouseButtons asks conhost (SM_CMOUSEBUTTONS) through a new condrv ioctl (334).

## 2026-10-13, batch 6: patches 5072-5078 (17 items; 207 implemented so far)
Same method; suites ntoskrnl.exe ole32 (64 and 32) compared with the base: no new failures, three todo_wine removed.
- 5072 ntoskrnl callback objects: ExCreateCallback (named; the system's \Callback objects open), ExRegisterCallback,
  ExUnregisterCallback (also from inside the routine), ExNotifyCallback (1090-1093).
- 5073 IoInitializeTimer / IoStartTimer / IoStopTimer, a routine called once a second (1075-1077).
- 5074 driver Reinitialize routines run after DriverEntry with Count (1084, 1085).
- 5075 a DPC queue: KeInsertQueueDpc, KeRemoveQueueDpc, KeFlushQueuedDpcs, KeSetTargetProcessorDpc(Ex) (1097, 1160,
  1130, 1131).
- 5076 RtlIsNtDdiVersionAvailable against the running version (1165).
- 5077 ole32 IPropertyStorage::SetTimes on a simple set (890); 5078 the data cache's IViewObject::Freeze/Unfreeze
  (510, 511).

## 2026-10-14, batch 7 (Fast A, successor): patches 5079-5095 (ntoskrnl.exe, netio.sys)
Same method, build tree with X; suites ntoskrnl.exe (64 and 32) and ole32 (64 and 32) equal the base (ntoskrnl 64-bit shows
the 4 catalog-signing failures the base build has too). Drafter 6's 5072-5078 gates pass; the series applies on a fresh tarball.
Scope change from the coordinator: crypt32/bcrypt/wintrust/cryptowinrt/onlineid/web.core go to drafter 7 (branch stubs-6b,
5300-5499), gdi32/user32/win32u to Fast B (stubs-6c, 5200-5299).
- 5079 MDL lock/map/protect (1104 1109 1111 1112 1126); 5080 READ/WRITE_REGISTER_* (1134); 5081 SeLocateProcessImageName,
  IoGetRelatedDeviceObject (1159 1082); 5082 StartIo/device queue (1142); 5083 cancel-safe IRP queues (1145);
  5084 KQUEUE (1096); 5085 load-image notify flags (1137); 5086 shutdown notification (1086 1087); 5087 error log (1078);
  5088 IoReportResourceForDetection (1088); 5089 IoCreateFile options (1151 1152); 5090 IoGetConfigurationInformation (1083);
  5091 PoSetPowerState/PoRequestPowerIrp (1068 1069); 5092 IoGetDeviceInterfaces (1080); 5093 PlugPlay notification
  (1143 1144); 5094 IoGetDeviceProperty (1179); 5095 netio.sys WskGetLocalAddress/WskGetRemoteAddress + error map (1059 1060 1174).
- ntoskrnl.exe candidates now all closed (1184/1185 triaged no-ground-truth). Open next: netio.sys 1061 1062 1064 1067 1175 1176 1178;
  ntdll 1 11 55 360 361 362 367 395 405 439 443 444 446 981; ole32 460 461 493 494 499 505 506 509 517 519 869 985-989;
  rpcrt4 554-597 (23); combase 61 159 160 162 927; advapi32 123; oleaut32 547. Details in /var/tmp/fast-a/s6-STATE.md.

- 2026-10-14 (Fast A): netio.sys 5096 WskControlSocket, 5097 WskDisconnect/WskConnectEx, 5098 WskSocketConnect/WskGetNameInfo,
  5099 basic+stream sockets/WskListen, 5100 datagram sockets (1062 1064 1067 1175 1176 1178 implemented; 1061 no-ground-truth: the
  numeric WSK_FLAG_NODELAY/WAITALL/DRAIN values are in no allowed source). ntdll 5101 placeholder compatibility mode (367), 5102
  NtFlushProcessWriteBuffers (446), 5103 MemoryRegionInformation other process (443), 5104 SystemPowerCapabilities (55), 5105 XPRESS and
  XPRESS_HUFF from MS-XCA (360 361 362); escalated with proposals: 1 (volume lock), 11 (job classes), 395 (volume label), 405/981
  (cdrom ioctls), 439 (key write time), 444 (working set of other processes). ntdll full suites 64/32 vs base: no new failures (the
  32-bit ntdll.virtual syscall-thunk test needs a prefix made with the new build).
