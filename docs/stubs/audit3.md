# Stub audit 3 (2026-10-09)

Tree: the wine-sg full series (patches/series through sg/1695) applied to wine-10.0, in /var/tmp/stubs-agent/verify/wine-10.0.

Kinds: fixme-stub (a FIXME saying stub/semi-stub/partial/not implemented), notimpl (returns E_NOTIMPL, STATUS_NOT_IMPLEMENTED or ERROR_CALL_NOT_IMPLEMENTED), fake-success (a FIXME and a success return with no work), spec-stub (an "@ stub" export).

Ranking: runtime FIXME hits in 21862 logs of real program runs under /var/tmp (distinct logs, x8, capped), imports by 791 PE files of real programs (Chrome, Firefox, Edge, VS Code, Audacity, Paint.NET and its .NET/WPF runtime, Steam setup, DYMO, AnyDesk, 7-Zip; x60 per program), DLL tier (core +30, legacy/16-bit -40).

Totals: 40303 items: spec-stub 25769, fixme-stub 8708, notimpl 5459, fake-success 367

## Ranked (top 300)

| # | score | dll | function | kind | programs | logs | where | note |
|---|---|---|---|---|---|---|---|---|
| 1 | 620 | ntdll | NtQuerySystemInformation | fixme-stub | chrome firefox pdn2 vscode | 71 | dlls/ntdll/unix/system.c:3056 | stub info_class SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION |
| 2 | 568 | wtsapi32 | WTSRegisterSessionNotification | fixme-stub | Edge chrome firefox vscode | 36 | dlls/wtsapi32/wtsapi32.c:685 | [skip: WTS lock state (coordinator)] Stub %p 0x%08lx |
| 3 | 520 | ntdll | NtFsControlFile | fixme-stub |  | 80 | dlls/ntdll/unix/file.c:7354 | stub! return success - Unsupported fsctl %x (device=%x access=%x func=%x method=%x) |
| 4 | 424 | wtsapi32 | WTSUnRegisterSessionNotification | fixme-stub | Edge chrome firefox vscode | 18 | dlls/wtsapi32/wtsapi32.c:787 | [skip: WTS lock state (coordinator)] Stub %p |
| 5 | 325 | ntdll | NtQueryInformationProcess | fixme-stub | Edge chrome firefox pdn2 vscode | 20 | dlls/ntdll/unix/process.c:1411 | ProcessQuotaLimits (%p,%p,0x%08x,%p) stub |
| 6 | 308 | wtsapi32 | WTSQuerySessionInformationW | fixme-stub | Edge audacity chrome vscode | 21 | dlls/wtsapi32/wtsapi32.c:548 | returning partial WTSINFO |
| 7 | 296 | ntdll | RtlSetHeapInformation | fixme-stub |  | 32 | dlls/ntdll/heap.c:2581 | HeapCompatibilityInformation %lu not implemented! |
| 8 | 290 | ntdll | NtSetInformationThread | fixme-stub | pdn2 vscode | 25 | dlls/ntdll/unix/thread.c:2492 | info class %d not supported yet |
| 9 | 284 | advapi32 | SetSecurityInfo | fixme-stub | Edge chrome firefox vscode | 18 | dlls/advapi32/security.c:3503 | [partly DAVID: window station SDs] unimplemented type %u, returning success |
| 10 | 278 | kernelbase | Wow64GetThreadContext | notimpl | Edge audacity chrome vscode | 0 | dlls/kernelbase/thread.c:889 | STATUS_NOT_IMPLEMENTED |
| 11 | 250 | dcomp | DCompositionCreateDevice3 | notimpl | firefox | 19 | dlls/dcomp/device.c:780 | %p, %s, %p. |
| 12 | 232 | mscoree | parse_supported_runtime | fixme-stub |  | 24 | dlls/mscoree/config.c:362 | sku=%s not implemented |
| 13 | 195 | secur32 | GetUserNameExW | fixme-stub | Edge chrome firefox | 10 | dlls/secur32/secur32.c:1125 | NameFormat %d not implemented |
| 14 | 190 | kernelbase | FindFirstFileExW | fixme-stub | Edge audacity chrome firefox pdn2 vscode | 0 | dlls/kernelbase/file.c:1459 | search_op not implemented 0x%08x |
| 15 | 188 | bcrypt | BCryptOpenAlgorithmProvider | notimpl | Edge audacity chrome firefox pdn2 vscode | 0 | dlls/bcrypt/bcrypt_main.c:353 | algorithm %s not supported |
| 16 | 184 | ntdll | NtSetInformationProcess | fixme-stub |  | 18 | dlls/ntdll/unix/process.c:2087 | (%p,0x%08x,%p,0x%08x) stub |
| 17 | 184 | ntdll | NtSetInformationJobObject | fixme-stub |  | 18 | dlls/ntdll/unix/sync.c:921 | stub: %p %u %p %u |
| 18 | 182 | d3d11 | d3d11_device_CheckFeatureSupport | notimpl |  | 18 | dlls/d3d11/device.c:4784 | Returning fake threading support data. |
| 19 | 176 | windows.media | factory_QueryInterface | fixme-stub |  | 17 | dlls/windows.media/captions.c:38 | %s not implemented, returning E_NOINTERFACE. |
| 20 | 174 | ntdll | server_get_file_info | notimpl |  | 17 | dlls/ntdll/unix/file.c:2232 | Unsupported info class %x |
| 21 | 168 | dwmapi | DwmAttachMilContent | fixme-stub |  | 16 | dlls/dwmapi/dwmapi_main.c:636 | (%p) stub |
| 22 | 165 | ws2_32 | WSAIoctl | fixme-stub | audacity chrome firefox pdn2 vscode | 0 | dlls/ws2_32/socket.c:2559 | SIO_UDP_CONNRESET stub |
| 23 | 164 | wtsapi32 | WTSQueryUserToken | fixme-stub | Edge | 8 | dlls/wtsapi32/wtsapi32.c:646 | [DAVID: security model] %lu %p semi-stub! |
| 24 | 163 | setupapi | SetupDiOpenDevRegKey | notimpl | Edge audacity chrome firefox vscode | 0 | dlls/setupapi/devinst.c:4306 | Unhandled type %#lx. |
| 25 | 160 | advapi32 | LogonUserW | fixme-stub | chrome firefox | 0 | dlls/advapi32/advapi.c:396 | [DAVID: security model] %s %s %p 0x%08lx 0x%08lx %p - stub |
| 26 | 160 | advapi32 | CreateProcessWithTokenW | fixme-stub | Edge chrome | 0 | dlls/advapi32/security.c:3129 | [DAVID: security model] %p 0x%08lx %s %s 0x%08lx %p %s %p %p - semi-stub |
| 27 | 160 | credui | CredUIPromptForWindowsCredentialsW | fixme-stub | chrome firefox | 0 | dlls/credui/credui_main.c:1018 | (%p, %lu, %p, %p, %lu, %p, %p, %p, %08lx) stub |
| 28 | 160 | iphlpapi | IpReleaseAddress | fixme-stub | chrome vscode | 0 | dlls/iphlpapi/iphlpapi_main.c:3837 | Stub AdapterInfo %p |
| 29 | 160 | iphlpapi | IpRenewAddress | fixme-stub | chrome vscode | 0 | dlls/iphlpapi/iphlpapi_main.c:3863 | Stub AdapterInfo %p |
| 30 | 160 | oleaut32 | DispCallFunc | fixme-stub | Edge vscode | 0 | dlls/oleaut32/typelib.c:6970 | (%p, %ld, %d, %d, %d, %p, %p, %p (vt=%d)): not implemented for this CPU |
| 31 | 160 | powrprof | PowerDeterminePlatformRoleEx | fixme-stub | chrome vscode | 0 | dlls/powrprof/powrprof.c:374 | %lu stub. |
| 32 | 160 | secur32 | LsaLogonUser | fixme-stub | chrome firefox | 0 | dlls/secur32/lsa.c:212 | [DAVID: security model] %p %s %d %ld %p %ld %p %p %p %p %p %p %p %p stub |
| 33 | 160 | userenv | CreateAppContainerProfile | fixme-stub | Edge vscode | 0 | dlls/userenv/userenv_main.c:972 | (%s, %s, %s, %p, %ld, %p): stub |
| 34 | 160 | winusb | WinUsb_Free | fixme-stub | chrome vscode | 0 | dlls/winusb/main.c:32 | (%p) - stub |
| 35 | 150 | dcomp | DCompositionCreateSurfaceHandle | spec-stub | chrome vscode | 0 | dlls/dcomp/*.spec:0 |  |
| 36 | 150 | mf | MFCreateDeviceSource | spec-stub | chrome vscode | 0 | dlls/mf/*.spec:0 |  |
| 37 | 150 | mfplat | MFCreateMediaBufferWrapper | spec-stub | chrome vscode | 0 | dlls/mfplat/*.spec:0 |  |
| 38 | 150 | mfplat | MFSerializeAttributesToStream | spec-stub | chrome vscode | 0 | dlls/mfplat/*.spec:0 |  |
| 39 | 150 | winusb | WinUsb_ControlTransfer | spec-stub | chrome vscode | 0 | dlls/winusb/*.spec:0 |  |
| 40 | 150 | winusb | WinUsb_GetAssociatedInterface | spec-stub | chrome vscode | 0 | dlls/winusb/*.spec:0 |  |
| 41 | 150 | winusb | WinUsb_GetOverlappedResult | spec-stub | chrome vscode | 0 | dlls/winusb/*.spec:0 |  |
| 42 | 150 | winusb | WinUsb_Initialize | spec-stub | chrome vscode | 0 | dlls/winusb/*.spec:0 |  |
| 43 | 150 | winusb | WinUsb_ReadPipe | spec-stub | chrome vscode | 0 | dlls/winusb/*.spec:0 |  |
| 44 | 150 | winusb | WinUsb_ResetPipe | spec-stub | chrome vscode | 0 | dlls/winusb/*.spec:0 |  |
| 45 | 150 | winusb | WinUsb_SetCurrentAlternateSetting | spec-stub | chrome vscode | 0 | dlls/winusb/*.spec:0 |  |
| 46 | 150 | winusb | WinUsb_WritePipe | spec-stub | chrome vscode | 0 | dlls/winusb/*.spec:0 |  |
| 47 | 140 | advapi32 | GetSecurityInfo | fixme-stub | Edge chrome firefox vscode | 0 | dlls/advapi32/security.c:1786 | unimplemented type %u |
| 48 | 140 | ncrypt | NCryptExportKey | fixme-stub | Edge audacity chrome vscode | 0 | dlls/ncrypt/main.c:1224 | Key blob encryption not implemented |
| 49 | 140 | user32 | RegisterDeviceNotificationW | fixme-stub | audacity chrome firefox vscode | 0 | dlls/user32/input.c:575 | DBT_DEVTYP_HANDLE not implemented |
| 50 | 140 | wintrust | WinVerifyTrust | fixme-stub | Edge chrome firefox vscode | 0 | dlls/wintrust/wintrust_main.c:664 | unimplemented for %ld |
| 51 | 138 | bcrypt | BCryptDecrypt | notimpl | Edge audacity pdn2 vscode | 0 | dlls/bcrypt/bcrypt_main.c:2294 | flags %#lx not supported |
| 52 | 138 | ntdll | NtQueryObject | notimpl | Edge chrome firefox vscode | 0 | dlls/ntdll/unix/file.c:8422 | size %u too small |
| 53 | 136 | win32u | pack_message | fixme-stub |  | 12 | dlls/win32u/message.c:945 | WM_NCPAINT hdc packing not supported yet |
| 54 | 130 | dhcpcsvc | DhcpCApiInitialize | fixme-stub | chrome vscode | 0 | dlls/dhcpcsvc/dhcpcsvc.c:43 | : stub |
| 55 | 128 | crypt32 | CRYPT_RegControl | fixme-stub |  | 11 | dlls/crypt32/regstore.c:525 | %lu: stub |
| 56 | 128 | msctf | SetInputScope | fixme-stub |  | 11 | dlls/msctf/msctf.c:612 | STUB: %p %i |
| 57 | 128 | windows.media | captions_get_FontColor | fixme-stub |  | 11 | dlls/windows.media/captions.c:120 | iface %p, value %p semi-stub. |
| 58 | 128 | windows.media | captions_get_FontOpacity | fixme-stub |  | 11 | dlls/windows.media/captions.c:134 | iface %p, value %p semi-stub. |
| 59 | 128 | windows.media | captions_get_FontSize | fixme-stub |  | 11 | dlls/windows.media/captions.c:142 | iface %p, value %p semi-stub. |
| 60 | 128 | windows.media | captions_get_FontStyle | fixme-stub |  | 11 | dlls/windows.media/captions.c:150 | iface %p, value %p semi-stub. |
| 61 | 128 | windows.media | captions_get_FontEffect | fixme-stub |  | 11 | dlls/windows.media/captions.c:158 | iface %p, value %p semi-stub. |
| 62 | 128 | windows.media | captions_get_BackgroundColor | fixme-stub |  | 11 | dlls/windows.media/captions.c:166 | iface %p, value %p semi-stub. |
| 63 | 128 | windows.media | captions_get_BackgroundOpacity | fixme-stub |  | 11 | dlls/windows.media/captions.c:180 | iface %p, value %p semi-stub. |
| 64 | 128 | windows.media | captions_get_RegionColor | fixme-stub |  | 11 | dlls/windows.media/captions.c:188 | iface %p, value %p semi-stub. |
| 65 | 128 | windows.media | captions_get_RegionOpacity | fixme-stub |  | 11 | dlls/windows.media/captions.c:202 | iface %p, value %p semi-stub. |
| 66 | 124 | combase | global_options_Set | fake-success |  | 11 | dlls/combase/combase.c:556 | %p, %u, %Ix. |
| 67 | 122 | wintypes | api_information_statics_IsApiContractPresentByMajor | fixme-stub |  | 14 | dlls/wintypes/main.c:302 | iface %p, contract_name %s, major_version %u, value %p semi-stub. |
| 68 | 120 | bthprops.cpl | BluetoothEnableDiscovery | spec-stub | chrome vscode | 0 | dlls/bthprops.cpl/*.spec:0 |  |
| 69 | 120 | bthprops.cpl | BluetoothEnableIncomingConnections | spec-stub | chrome vscode | 0 | dlls/bthprops.cpl/*.spec:0 |  |
| 70 | 120 | bthprops.cpl | BluetoothIsConnectable | spec-stub | chrome vscode | 0 | dlls/bthprops.cpl/*.spec:0 |  |
| 71 | 120 | kernelbase | PerfCreateInstance | fixme-stub |  | 10 | dlls/kernelbase/main.c:220 | handle %p, guid %s, name %s, id %lu semi-stub. |
| 72 | 120 | kernelbase | PerfSetCounterSetInfo | fixme-stub |  | 10 | dlls/kernelbase/main.c:301 | handle %p, template %p, size %lu semi-stub. |
| 73 | 120 | kernelbase | PerfSetCounterRefValue | fixme-stub |  | 10 | dlls/kernelbase/main.c:346 | provider %p, instance %p, counterid %lu, address %p semi-stub. |
| 74 | 120 | kernelbase | PerfStartProviderEx | fixme-stub |  | 10 | dlls/kernelbase/main.c:390 | guid %s, context %p, provider %p semi-stub. |
| 75 | 118 | kernelbase | get_geo_info | notimpl |  | 10 | dlls/kernelbase/locale.c:1801 | type %u is not supported |
| 76 | 116 | wbemprox | client_security_Release | fake-success |  | 10 | dlls/wbemprox/services.c:73 | %p |
| 77 | 115 | comdlg32 | PrintDlgExW | fixme-stub | chrome firefox vscode | 0 | dlls/comdlg32/printdlg.c:4218 | (%p) semi-stub |
| 78 | 115 | ncrypt | NCryptImportKey | fixme-stub | audacity chrome vscode | 0 | dlls/ncrypt/main.c:1122 | Key blob decryption not implemented |
| 79 | 115 | netapi32 | NetUserGetInfo | fixme-stub | chrome firefox vscode | 0 | dlls/netapi32/netapi32.c:1386 | Level %ld is not implemented |
| 80 | 115 | ntdll | NtQueryInformationThread | fixme-stub | Edge chrome vscode | 0 | dlls/ntdll/unix/thread.c:2090 | ThreadIsIoPending info class not supported yet |
| 81 | 115 | ntdll | NtQueryValueKey | fixme-stub | Edge chrome firefox | 0 | dlls/ntdll/unix/registry.c:515 | Information class %d not implemented |
| 82 | 114 | crypt32 | CryptDecodeObjectEx | fixme-stub | Edge vscode | 3 | dlls/crypt32/decode.c:7057 | Unimplemented decoder for lpszStructType OID %d |
| 83 | 113 | wtsapi32 | WTSEnumerateSessionsW | fixme-stub | Edge | 6 | dlls/wtsapi32/wtsapi32.c:348 | %p 0x%08lx 0x%08lx %p %p semi-stub. |
| 84 | 112 | rpcrt4 | NdrClearOutParameters | fixme-stub |  | 9 | dlls/rpcrt4/ndr_marshall.c:4660 | (%p,%p,%p): stub |
| 85 | 103 | ntdll | NtSetInformationFile | notimpl | vscode | 5 | dlls/ntdll/unix/file.c:5399 | FILE_SKIP_SET_USER_EVENT_ON_FAST_IO not supported |
| 86 | 100 | advapi32 | LsaAddAccountRights | fixme-stub | chrome | 0 | dlls/advapi32/lsa.c:196 | (%p,%p,%p,0x%08lx) stub |
| 87 | 100 | advapi32 | GetEffectiveRightsFromAclW | fixme-stub | Edge | 0 | dlls/advapi32/security.c:277 | %p %p %p - stub |
| 88 | 100 | cfgmgr32 | CM_Register_Notification | fixme-stub | firefox | 0 | dlls/cfgmgr32/main.c:64 | %p %p %p %p stub! |
| 89 | 100 | combase | RoGetApartmentIdentifier | fixme-stub | Edge | 0 | dlls/combase/roapi.c:983 | (%p): stub |
| 90 | 100 | combase | RoRegisterForApartmentShutdown | fixme-stub | Edge | 0 | dlls/combase/roapi.c:997 | (%p, %p, %p): stub |
| 91 | 100 | combase | CoGetDefaultContext | fixme-stub | firefox | 0 | dlls/combase/combase.c:1268 | %d, %s, %p stub |
| 92 | 100 | combase | CoGetCallerTID | fixme-stub | firefox | 0 | dlls/combase/combase.c:3539 | stub! |
| 93 | 100 | d3d9 | D3DPERF_SetMarker | fixme-stub | vscode | 0 | dlls/d3d9/d3d9_main.c:247 | color 0x%08lx, name %s stub! |
| 94 | 100 | dnsapi | DnsWriteQuestionToBuffer_W | fixme-stub | audacity | 0 | dlls/dnsapi/main.c:197 | (%p, %p, %s, %d, %d, %d) stub |
| 95 | 100 | iphlpapi | GetCurrentThreadCompartmentId | fixme-stub | pdn2 | 0 | dlls/iphlpapi/iphlpapi_main.c:5439 | stub |
| 96 | 100 | iphlpapi | SetCurrentThreadCompartmentId | fixme-stub | pdn2 | 0 | dlls/iphlpapi/iphlpapi_main.c:5448 | (%x): stub |
| 97 | 100 | kernelbase | GetProcessWorkingSetSizeEx | fixme-stub | Edge | 0 | dlls/kernelbase/process.c:1401 | (%p,%p,%p,%p): stub |
| 98 | 100 | kernelbase | SetWaitableTimerEx | fixme-stub | firefox | 0 | dlls/kernelbase/sync.c:884 | (%p, %p, %ld, %p, %p, %p, %ld) semi-stub |
| 99 | 100 | kernelbase | CreateBoundaryDescriptorW | fixme-stub | Edge | 0 | dlls/kernelbase/security.c:948 | %s %lu - stub |
| 100 | 100 | netapi32 | NetShareEnum | fixme-stub | audacity | 0 | dlls/netapi32/netapi32.c:477 | Stub (%s %ld %p %ld %p %p %p) |
| 101 | 100 | ntdll | RtlEnumerateGenericTableWithoutSplaying | fixme-stub | firefox | 0 | dlls/ntdll/rtl.c:278 | (%p, %p) stub! |
| 102 | 100 | ntdll | RtlLookupElementGenericTable | fixme-stub | firefox | 0 | dlls/ntdll/rtl.c:308 | (%p, %p) stub! |
| 103 | 100 | powrprof | PowerGetActiveScheme | fixme-stub | chrome | 0 | dlls/powrprof/powrprof.c:289 | (%p,%p) stub! |
| 104 | 100 | powrprof | PowerReadDCValue | fixme-stub | chrome | 0 | dlls/powrprof/powrprof.c:301 | (%p,%s,%s,%s,%p,%p,%p) stub! |
| 105 | 100 | powrprof | PowerRegisterSuspendResumeNotification | fixme-stub | firefox | 0 | dlls/powrprof/powrprof.c:388 | (0x%08lx,%p,%p) stub! |
| 106 | 100 | powrprof | PowerUnregisterSuspendResumeNotification | fixme-stub | firefox | 0 | dlls/powrprof/powrprof.c:395 | (%p) stub! |
| 107 | 100 | secur32 | LsaEnumerateLogonSessions | fixme-stub | vscode | 0 | dlls/secur32/lsa.c:166 | %p %p stub |
| 108 | 100 | user32 | AnimateWindow | fixme-stub | firefox | 0 | dlls/user32/win.c:852 | partial stub |
| 109 | 100 | user32 | SetWindowCompositionAttribute | fixme-stub | firefox | 0 | dlls/user32/win.c:1769 | (%p, %p): stub |
| 110 | 100 | user32 | EvaluateProximityToRect | fixme-stub | firefox | 0 | dlls/user32/misc.c:797 | (%p,%p,%p): stub |
| 111 | 100 | user32 | UnloadKeyboardLayout | fixme-stub | firefox | 0 | dlls/user32/input.c:484 | layout %p, stub! |
| 112 | 100 | user32 | CreateSyntheticPointerDevice | fixme-stub | firefox | 0 | dlls/user32/input.c:953 | type %ld, max_count %ld, mode %d stub! |
| 113 | 100 | user32 | SetDisplayAutoRotationPreferences | fixme-stub | audacity | 0 | dlls/user32/sysparams.c:1035 | (%d): stub |
| 114 | 100 | user32 | ShutdownBlockReasonCreate | fixme-stub | vscode | 0 | dlls/user32/user_main.c:408 | (%p, %s): stub |
| 115 | 100 | user32 | ShutdownBlockReasonDestroy | fixme-stub | vscode | 0 | dlls/user32/user_main.c:418 | (%p): stub |
| 116 | 100 | userenv | UnloadUserProfile | fixme-stub | Edge | 0 | dlls/userenv/userenv_main.c:833 | (%p, %p): stub |
| 117 | 100 | wer | WerReportAddDump | fixme-stub | Edge | 0 | dlls/wer/main.c:154 | (%p, %p, %p, %d, %p, %p, %lu) :stub |
| 118 | 100 | wer | WerReportAddFile | fixme-stub | Edge | 0 | dlls/wer/main.c:180 | (%p, %s, %d, 0x%lx) :stub |
| 119 | 100 | wer | WerReportSetParameter | fixme-stub | Edge | 0 | dlls/wer/main.c:300 | (%p, %ld, %s, %s) :stub |
| 120 | 100 | wer | WerReportSubmit | fixme-stub | Edge | 0 | dlls/wer/main.c:324 | (%p, %d, 0x%lx, %p) :stub |
| 121 | 100 | wininet | DeleteUrlCacheContainerW | fixme-stub | Edge | 0 | dlls/wininet/urlcache.c:3325 | (0x%08lx, 0x%08lx) stub |
| 122 | 98 | bcrypt | BCryptEnumContextFunctions | notimpl | audacity | 0 | dlls/bcrypt/bcrypt_main.c:68 | %#lx, %s, %#lx, %p, %p |
| 123 | 98 | bcrypt | BCryptExportKey | notimpl | vscode | 0 | dlls/bcrypt/bcrypt_main.c:2065 | encryption of key not yet supported |
| 124 | 98 | propsys | PSGetPropertyKeyFromName | notimpl | chrome | 0 | dlls/propsys/propsys_main.c:235 | %s, %p |
| 125 | 98 | wlanapi | WlanEnumInterfaces | fixme-stub |  | 11 | dlls/wlanapi/main.c:72 | (%p, %p, %p) semi-stub |
| 126 | 97 | sechost | QueryServiceConfig2W | fixme-stub | Edge | 4 | dlls/sechost/service.c:1020 | Level %ld not implemented |
| 127 | 96 | ntdll | NtQueryWnfStateData | fake-success | firefox | 0 | dlls/ntdll/misc.c:293 | %p %p %p %p %p %p: no states are published |
| 128 | 96 | windows.networking.connectivity | factory_QueryInterface | fixme-stub |  | 7 | dlls/windows.networking.connectivity/network_information.c:40 | %s not implemented, returning E_NOINTERFACE. |
| 129 | 96 | windowscodecs | jpeg_decoder_get_metadata_blocks | fixme-stub |  | 7 | dlls/windowscodecs/libjpeg.c:317 | stub |
| 130 | 90 | combase | CoGetStdMarshalEx | spec-stub | Edge | 0 | dlls/combase/*.spec:0 |  |
| 131 | 90 | combase | IsErrorPropagationEnabled | spec-stub | firefox | 0 | dlls/combase/*.spec:0 |  |
| 132 | 90 | combase | RoReportFailedDelegate | spec-stub | firefox | 0 | dlls/combase/*.spec:0 |  |
| 133 | 90 | combase | RoUnregisterForApartmentShutdown | spec-stub | Edge | 0 | dlls/combase/*.spec:0 |  |
| 134 | 90 | crypt32 | CertVerifyCertificateChainPolicy | fixme-stub | Edge vscode | 0 | dlls/crypt32/chain.c:4060 | unimplemented for %d |
| 135 | 90 | crypt32 | CryptVerifyCertificateSignatureEx | fixme-stub | chrome vscode | 0 | dlls/crypt32/cert.c:3163 | CRYPT_VERIFY_CERT_SIGN_ISSUER_CHAIN: stub |
| 136 | 90 | gdi32 | PlayEnhMetaFileRecord | fixme-stub | chrome vscode | 0 | dlls/gdi32/enhmetafile.c:795 | type %d is unimplemented |
| 137 | 90 | mf | MFCreatePMPMediaSession | spec-stub | firefox | 0 | dlls/mf/*.spec:0 |  |
| 138 | 90 | mf | MFEnumDeviceSources | fixme-stub | chrome vscode | 0 | dlls/mf/main.c:750 | Not implemented for video capture devices. |
| 139 | 90 | ntdll | RtlActivateActivationContextUnsafeFast | spec-stub | firefox | 0 | dlls/ntdll/*.spec:0 |  |
| 140 | 90 | ntdll | RtlDeactivateActivationContextUnsafeFast | spec-stub | firefox | 0 | dlls/ntdll/*.spec:0 |  |
| 141 | 90 | ntdll | RtlDeleteElementGenericTable | spec-stub | firefox | 0 | dlls/ntdll/*.spec:0 |  |
| 142 | 90 | ntdll | RtlInsertElementGenericTable | spec-stub | firefox | 0 | dlls/ntdll/*.spec:0 |  |
| 143 | 90 | ntdll | RtlQueryInformationActiveActivationContext | spec-stub | firefox | 0 | dlls/ntdll/*.spec:0 |  |
| 144 | 90 | ntdll | NtOpenKeyEx | fixme-stub | Edge chrome | 0 | dlls/ntdll/unix/registry.c:159 | options %x not implemented |
| 145 | 90 | setupapi | SetupDiGetDeviceInterfaceAlias | spec-stub | audacity | 0 | dlls/setupapi/*.spec:0 |  |
| 146 | 90 | shcore | PathIsNetworkPathW | spec-stub | firefox | 0 | dlls/shcore/*.spec:0 |  |
| 147 | 90 | user32 | CopyImage | fixme-stub | firefox vscode | 0 | dlls/user32/cursoricon.c:2242 | The flag LR_COPYFROMRESOURCE is not implemented for bitmaps |
| 148 | 90 | user32 | DdeDisconnect | fixme-stub | audacity chrome | 0 | dlls/user32/dde_client.c:1365 | Not implemented yet for a server side conversation |
| 149 | 90 | user32 | DrawFrameControl | fixme-stub | chrome vscode | 0 | dlls/user32/uitools.c:1219 | DFC_POPUPMENU: not implemented |
| 150 | 90 | wininet | InternetSetOptionW | fixme-stub | Edge vscode | 0 | dlls/wininet/internet.c:3444 | Option INTERNET_OPTION_HTTP_VERSION(%ld,%ld): STUB |
| 151 | 90 | winspool.drv | GetPrinterW | fixme-stub | chrome vscode | 0 | dlls/winspool.drv/info.c:4566 | Unimplemented level %ld |
| 152 | 89 | mpr | WNetGetUniversalNameW | fixme-stub | audacity | 3 | dlls/mpr/wnet.c:2765 | (%s, 0x%08lX, %p, %p): stub |
| 153 | 88 | bcrypt | BCryptImportKeyPair | notimpl | Edge vscode | 0 | dlls/bcrypt/bcrypt_main.c:2173 | decryption of key not yet supported |
| 154 | 88 | ntdll | NtQueryVolumeInformationFile | notimpl | audacity vscode | 0 | dlls/ntdll/unix/file.c:8057 | %p: label info not supported |
| 155 | 88 | win32u | NtUserFlashWindowEx | fixme-stub |  | 6 | dlls/win32u/window.c:5174 | %p - semi-stub |
| 156 | 83 | oleacc | LresultFromObject | notimpl | chrome firefox vscode | 0 | dlls/oleacc/main.c:186 | unsupported wParam = %Ix |
| 157 | 82 | explorer | handle_appbarmessage | fixme-stub |  | 9 | programs/explorer/appbar.c:241 | SHAppBarMessage(ABM_GETSTATE): stub |
| 158 | 80 | kernelbase | SetCachedSigningLevel | fixme-stub |  | 5 | dlls/kernelbase/security.c:1544 | %p %lu %lu %p - stub |
| 159 | 78 | tbs | Tbsi_GetDeviceInfo | fixme-stub | chrome | 1 | dlls/tbs/tbs.c:33 | (%u, %p) stub |
| 160 | 70 | authz | AuthzInitializeResourceManager | fixme-stub | audacity | 0 | dlls/authz/authz.c:33 | (0x%lx,%p,%p,%p,%s,%p): stub |
| 161 | 70 | authz | AuthzAccessCheck | fixme-stub | audacity | 0 | dlls/authz/authz.c:68 | (0x%lx,%p,%p,%p,%p,%p,0x%lx,%p,%p): stub |
| 162 | 70 | authz | AuthzFreeContext | fixme-stub | audacity | 0 | dlls/authz/authz.c:85 | (%p): stub |
| 163 | 70 | authz | AuthzInitializeContextFromSid | fixme-stub | audacity | 0 | dlls/authz/authz.c:96 | (0x%lx,%p,%p,%p,%08lx:%08lx,%p,%p): stub |
| 164 | 70 | authz | AuthzInitializeContextFromToken | fixme-stub | audacity | 0 | dlls/authz/authz.c:110 | (0x%lx,%p,%p,%p,%08lx:%08lx,%p,%p): stub |
| 165 | 70 | ninput | SetInteractionConfigurationInteractionContext | fixme-stub | firefox | 0 | dlls/ninput/main.c:132 | context %p, count %u, configuration %p: stub!. |
| 166 | 70 | ninput | RegisterOutputCallbackInteractionContext | fixme-stub | firefox | 0 | dlls/ninput/main.c:149 | context %p, callback %p, data %p: stub!. |
| 167 | 70 | rstrtmgr | RmRegisterResources | fixme-stub | Edge | 0 | dlls/rstrtmgr/main.c:50 | %ld, %d, %p, %d, %p, %d, %p stub! |
| 168 | 70 | rstrtmgr | RmStartSession | fixme-stub | Edge | 0 | dlls/rstrtmgr/main.c:64 | %p, %ld, %p stub! |
| 169 | 70 | rstrtmgr | RmRestart | fixme-stub | Edge | 0 | dlls/rstrtmgr/main.c:75 | %lu, 0x%08lx, %p stub! |
| 170 | 70 | rstrtmgr | RmEndSession | fixme-stub | Edge | 0 | dlls/rstrtmgr/main.c:84 | %lu stub! |
| 171 | 70 | rstrtmgr | RmShutdown | fixme-stub | Edge | 0 | dlls/rstrtmgr/main.c:93 | %lu, 0x%08lx, %p stub! |
| 172 | 70 | shell32 | ApplicationAssociationRegistration_SetAppAsDefault | notimpl |  | 4 | dlls/shell32/assoc.c:1015 | (%p)->(%s, %s, %d) |
| 173 | 68 | authz | AuthzFreeResourceManager | notimpl | audacity | 0 | dlls/authz/authz.c:47 | %p |
| 174 | 65 | crypt32 | CryptBinaryToStringA | fixme-stub | vscode | 0 | dlls/crypt32/base64.c:302 | Unimplemented type %ld |
| 175 | 65 | crypt32 | CryptBinaryToStringW | fixme-stub | firefox | 0 | dlls/crypt32/base64.c:644 | Unimplemented type %ld |
| 176 | 65 | crypt32 | CryptStringToBinaryA | fixme-stub | vscode | 0 | dlls/crypt32/base64.c:1062 | Unimplemented type %ld |
| 177 | 65 | kernelbase | GetNamedPipeHandleStateW | fixme-stub | vscode | 0 | dlls/kernelbase/sync.c:1451 | %p %p %p %p %p %p %ld: semi-stub |
| 178 | 65 | kernelbase | UrlEscapeW | fixme-stub | Edge | 0 | dlls/kernelbase/path.c:3295 | Unimplemented flags: %08lx |
| 179 | 65 | kernelbase | UrlIsW | fixme-stub | Edge | 0 | dlls/kernelbase/path.c:4755 | (%s %d): stub |
| 180 | 65 | netapi32 | NetUserGetLocalGroups | fixme-stub | vscode | 0 | dlls/netapi32/netapi32.c:1558 | (%s, %s, %ld, %08lx, %p %ld, %p, %p) stub! |
| 181 | 65 | netapi32 | DsRoleGetPrimaryDomainInformation | fixme-stub | chrome | 0 | dlls/netapi32/netapi32.c:2389 | (%p, %d, %p) stub |
| 182 | 65 | ntdll | NtPowerInformation | fixme-stub | firefox | 0 | dlls/ntdll/unix/system.c:4594 | semi-stub: SystemPowerCapabilities |
| 183 | 65 | rpcrt4 | NdrStubCall2 | fixme-stub | Edge | 0 | dlls/rpcrt4/ndr_stubless.c:1228 | pipes not supported yet |
| 184 | 65 | secur32 | LsaGetLogonSessionData | fixme-stub | vscode | 0 | dlls/secur32/lsa.c:183 | %p %p semi-stub |
| 185 | 65 | user32 | LoadKeyboardLayoutW | fixme-stub | vscode | 0 | dlls/user32/input.c:417 | name %s, flags %x, semi-stub! |
| 186 | 65 | winmm | mixerGetLineInfoW | fixme-stub | audacity | 0 | dlls/winmm/waveform.c:4266 | TARGETTYPE flag not implemented! |
| 187 | 65 | wintrust | CryptCATAdminAcquireContext2 | fixme-stub | firefox | 0 | dlls/wintrust/crypt.c:125 | strong policy parameter is unimplemented |
| 188 | 64 | advapi32 | DecryptFileW | fixme-stub |  | 3 | dlls/advapi32/security.c:3272 | (%s, %08lx): stub |
| 189 | 64 | combase | CoRegisterSurrogate | fixme-stub |  | 3 | dlls/combase/combase.c:3615 | %p stub |
| 190 | 64 | qmgr | BackgroundCopyJob_SetPriority | notimpl |  | 7 | dlls/qmgr/job.c:942 | E_NOTIMPL |
| 191 | 63 | ntdll | NtQueryDirectoryFile | notimpl | vscode | 0 | dlls/ntdll/unix/file.c:3000 | Unsupported yet option |
| 192 | 63 | ntdll | NtQueryInformationFile | notimpl | vscode | 0 | dlls/ntdll/unix/file.c:4937 | Unsupported class (%d) |
| 193 | 63 | urlmon | CoInternetParseUrl | notimpl | firefox | 0 | dlls/urlmon/internet.c:325 | not supported action %d |
| 194 | 60 | bcp47langs | GetApplicationLanguages | spec-stub | firefox | 0 | dlls/bcp47langs/*.spec:0 |  |
| 195 | 60 | bcp47langs | GetFontFallbackLanguageList | spec-stub | firefox | 0 | dlls/bcp47langs/*.spec:0 |  |
| 196 | 60 | bcp47langs | LanguageListAsMuiForm | spec-stub | firefox | 0 | dlls/bcp47langs/*.spec:0 |  |
| 197 | 60 | coremessaging | CoreUICreate | spec-stub | firefox | 0 | dlls/coremessaging/*.spec:0 |  |
| 198 | 60 | esent | JetAttachDatabase2W | spec-stub | chrome | 0 | dlls/esent/*.spec:0 |  |
| 199 | 60 | esent | JetBeginSessionW | spec-stub | chrome | 0 | dlls/esent/*.spec:0 |  |
| 200 | 60 | esent | JetCloseTable | spec-stub | chrome | 0 | dlls/esent/*.spec:0 |  |
| 201 | 60 | esent | JetCreateInstanceW | spec-stub | chrome | 0 | dlls/esent/*.spec:0 |  |
| 202 | 60 | esent | JetGetTableColumnInfoW | spec-stub | chrome | 0 | dlls/esent/*.spec:0 |  |
| 203 | 60 | esent | JetInit | spec-stub | chrome | 0 | dlls/esent/*.spec:0 |  |
| 204 | 60 | esent | JetMove | spec-stub | chrome | 0 | dlls/esent/*.spec:0 |  |
| 205 | 60 | esent | JetOpenDatabaseW | spec-stub | chrome | 0 | dlls/esent/*.spec:0 |  |
| 206 | 60 | esent | JetOpenTableW | spec-stub | chrome | 0 | dlls/esent/*.spec:0 |  |
| 207 | 60 | esent | JetRetrieveColumn | spec-stub | chrome | 0 | dlls/esent/*.spec:0 |  |
| 208 | 60 | esent | JetSetSystemParameterW | spec-stub | chrome | 0 | dlls/esent/*.spec:0 |  |
| 209 | 60 | esent | JetTerm | spec-stub | chrome | 0 | dlls/esent/*.spec:0 |  |
| 210 | 60 | msvcp140 | ??0?$basic_streambuf@DU?$char_traits@D@std@@@std@@IEAA@AEBV01@@Z | spec-stub | audacity | 0 | dlls/msvcp140/*.spec:0 |  |
| 211 | 60 | msvcp140 | ?_ReportUnobservedException@details@Concurrency@@YAXXZ | spec-stub | audacity | 0 | dlls/msvcp140/*.spec:0 |  |
| 212 | 60 | ninput | BufferPointerPacketsInteractionContext | spec-stub | firefox | 0 | dlls/ninput/*.spec:0 |  |
| 213 | 60 | ninput | GetInteractionConfigurationInteractionContext | spec-stub | firefox | 0 | dlls/ninput/*.spec:0 |  |
| 214 | 60 | ninput | GetMouseWheelParameterInteractionContext | spec-stub | firefox | 0 | dlls/ninput/*.spec:0 |  |
| 215 | 60 | ninput | GetStateInteractionContext | spec-stub | firefox | 0 | dlls/ninput/*.spec:0 |  |
| 216 | 60 | ninput | ProcessBufferedPacketsInteractionContext | spec-stub | firefox | 0 | dlls/ninput/*.spec:0 |  |
| 217 | 60 | ninput | SetCrossSlideParametersInteractionContext | spec-stub | firefox | 0 | dlls/ninput/*.spec:0 |  |
| 218 | 60 | ninput | SetInertiaParameterInteractionContext | spec-stub | firefox | 0 | dlls/ninput/*.spec:0 |  |
| 219 | 60 | ninput | SetMouseWheelParameterInteractionContext | spec-stub | firefox | 0 | dlls/ninput/*.spec:0 |  |
| 220 | 60 | ninput | SetPivotInteractionContext | spec-stub | firefox | 0 | dlls/ninput/*.spec:0 |  |
| 221 | 60 | sspicli | QueryContextAttributesExW | spec-stub | pdn2 | 0 | dlls/sspicli/*.spec:0 |  |
| 222 | 60 | tbs | Tbsip_Submit_Command | spec-stub | chrome | 0 | dlls/tbs/*.spec:0 |  |
| 223 | 60 | tdh | TdhGetEventInformation | spec-stub | vscode | 0 | dlls/tdh/*.spec:0 |  |
| 224 | 60 | vcruntime140 | __GetPlatformExceptionInfo | spec-stub | Edge | 0 | dlls/vcruntime140/*.spec:0 |  |
| 225 | 56 | mscoree | corruntimehost_Stop | fixme-stub |  | 2 | dlls/mscoree/corruntimehost.c:614 | stub %p |
| 226 | 56 | mscoree | corruntimehost_UnloadDomain | fixme-stub |  | 2 | dlls/mscoree/corruntimehost.c:738 | stub %p |
| 227 | 56 | mscoree | parse_startup | fixme-stub |  | 2 | dlls/mscoree/config.c:324 | useLegacyV2RuntimeActivationPolicy=%s not implemented |
| 228 | 56 | msi | internal_ui_handler | fixme-stub |  | 2 | dlls/msi/package.c:1678 | internal UI not implemented for message 0x%08x (UI level = %x) |
| 229 | 54 | msctf | InputProcessorProfileMgr_GetActiveProfile | notimpl |  | 2 | dlls/msctf/inputprocessor.c:867 | (%p)->(%s %p) |
| 230 | 48 | combase | thread_context_info_QueryInterface | fixme-stub |  | 1 | dlls/combase/combase.c:2564 | interface not implemented %s |
| 231 | 48 | d3d11 | d3d10_multithread_SetMultithreadProtected | fixme-stub |  | 1 | dlls/d3d11/device.c:7669 | iface %p, enable %#x stub! |
| 232 | 48 | dnsapi | map_options | fixme-stub |  | 1 | dlls/dnsapi/libresolv.c:69 | option DNS_QUERY_DONT_RESET_TTL_VALUES not implemented |
| 233 | 48 | dxgi | dxgi_output_DuplicateOutput | fixme-stub |  | 1 | dlls/dxgi/output.c:568 | iface %p, device %p, output_duplication %p stub! |
| 234 | 48 | dxgi | dxgi_surface_GetDC | fixme-stub |  | 1 | dlls/dxgi/resource.c:203 | iface %p, discard %d, hdc %p semi-stub! |
| 235 | 48 | dxgi | dxgi_factory_IsCurrent | fixme-stub |  | 1 | dlls/dxgi/factory.c:251 | iface %p stub! |
| 236 | 48 | jscript | JScript_SetScriptState | fixme-stub |  | 1 | dlls/jscript/jscript.c:815 | unimplemented SCRIPTSTATE_INITIALIZED |
| 237 | 48 | msxml3 | ClassFactory_QueryInterface | fixme-stub |  | 1 | dlls/msxml3/factory.c:131 | interface %s not implemented |
| 238 | 48 | msxml3 | saxxmlreader_QueryInterface | fixme-stub |  | 1 | dlls/msxml3/saxreader.c:2877 | interface %s not implemented |
| 239 | 48 | shdocvw | IEParseDisplayNameWithBCW | fixme-stub |  | 1 | dlls/shdocvw/shdocvw_main.c:434 | stub: 0x%lx %s %p %p |
| 240 | 48 | shell32 | IQueryAssociations_fnGetString | fixme-stub |  | 1 | dlls/shell32/assoc.c:532 | %08lx: unimplemented flags |
| 241 | 48 | shell32 | IShellBrowser_fnSendControlMsg | fixme-stub |  | 1 | dlls/shell32/ebrowser.c:1434 | stub, %p (%d, %d, %Ix, %Ix, %p) |
| 242 | 48 | shell32 | IShellBrowser_fnOnViewWindowActive | fixme-stub |  | 1 | dlls/shell32/ebrowser.c:1460 | stub, %p (%p) |
| 243 | 48 | taskschd | read_actions | fixme-stub |  | 1 | dlls/taskschd/task.c:3173 | action %s is not implemented |
| 244 | 48 | taskschd | read_triggers | fixme-stub |  | 1 | dlls/taskschd/trigger.c:1234 | trigger %s is not implemented |
| 245 | 48 | uiautomationcore | msaa_provider_GetPropertyValue | fixme-stub |  | 1 | dlls/uiautomationcore/uia_provider.c:656 | Unimplemented propertyId %d |
| 246 | 48 | uiautomationcore | default_uia_provider_callback | fixme-stub |  | 1 | dlls/uiautomationcore/uia_client.c:3243 | Default ProviderType_NonClientArea provider unimplemented. |
| 247 | 48 | uiautomationcore | uia_get_providers_for_hwnd | fixme-stub |  | 1 | dlls/uiautomationcore/uia_client.c:3353 | Override provider callback currently unimplemented. |
| 248 | 48 | win32u | NtGdiDdDDIQueryAdapterInfo | fixme-stub |  | 1 | dlls/win32u/d3dkmt.c:334 | desc %p, type %d stub |
| 249 | 48 | win32u | NtUserSystemParametersInfo | fixme-stub |  | 1 | dlls/win32u/sysparams.c:5895 | Unimplemented action: %u (%s) |
| 250 | 48 | win32u | NtUserDisplayConfigGetDeviceInfo | fixme-stub |  | 1 | dlls/win32u/sysparams.c:7557 | DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_PREFERRED_MODE semi-stub. |
| 251 | 48 | windows.devices.enumeration | device_statics_FindAllAsyncAqsFilter | fixme-stub |  | 1 | dlls/windows.devices.enumeration/main.c:350 | iface %p, aqs %p, op %p stub! |
| 252 | 48 | windows.devices.enumeration | device_access_information_CurrentStatus | fixme-stub |  | 1 | dlls/windows.devices.enumeration/access.c:120 | iface %p, status %p stub. |
| 253 | 48 | windows.devices.enumeration | statics_CreateFromDeviceClass | fixme-stub |  | 1 | dlls/windows.devices.enumeration/access.c:265 | device_class %d, value %p stub. |
| 254 | 48 | windows.ui | uisettings_QueryInterface | fixme-stub |  | 1 | dlls/windows.ui/uisettings.c:81 | %s not implemented, returning E_NOINTERFACE. |
| 255 | 48 | wininet | query_global_option | fixme-stub |  | 1 | dlls/wininet/internet.c:2918 | INTERNET_OPTION_CONNECTED_STATE: semi-stub |
| 256 | 46 | shell32 | shellfolder_map_column_to_scid | notimpl |  | 1 | dlls/shell32/shlfolder.c:692 | missing property id for column %u. |
| 257 | 42 | appxdeploymentclient | package_manager_QueryInterface | fixme-stub |  | 4 | dlls/appxdeploymentclient/package.c:40 | %s not implemented, returning E_NOINTERFACE. |
| 258 | 42 | sti | stillimagew_GetDeviceList | fixme-stub |  | 4 | dlls/sti/sti.c:77 | (%p, %lu, 0x%lX, %p, %p): stub |
| 259 | 40 | activeds | ADsBuildEnumerator | fixme-stub |  | 0 | dlls/activeds/activeds_main.c:59 | (%p)->(%p)!stub |
| 260 | 40 | activeds | ADsFreeEnumerator | fixme-stub |  | 0 | dlls/activeds/activeds_main.c:68 | (%p)!stub |
| 261 | 40 | activeds | ADsEnumerateNext | fixme-stub |  | 0 | dlls/activeds/activeds_main.c:77 | (%p)->(%lu, %p, %p)!stub |
| 262 | 40 | activeds | ADsBuildVarArrayInt | fixme-stub |  | 0 | dlls/activeds/activeds_main.c:129 | (%p, %ld, %p)!stub |
| 263 | 40 | activeds | ADsSetLastError | fixme-stub |  | 0 | dlls/activeds/activeds_main.c:217 | (%ld,%p,%p)!stub |
| 264 | 40 | activeds | ADsGetLastError | fixme-stub |  | 0 | dlls/activeds/activeds_main.c:225 | (%p,%p,%ld,%p,%ld)!stub |
| 265 | 40 | activeds | ReallocADsStr | fixme-stub |  | 0 | dlls/activeds/activeds_main.c:278 | (%p,%p)!stub |
| 266 | 40 | activeds | ADsEncodeBinaryData | fixme-stub |  | 0 | dlls/activeds/activeds_main.c:287 | (%p,%ld,%p)!stub |
| 267 | 40 | activeds | path_QueryInterface | fixme-stub |  | 0 | dlls/activeds/pathname.c:50 | interface %s is not implemented |
| 268 | 40 | activeds | path_GetTypeInfoCount | fixme-stub |  | 0 | dlls/activeds/pathname.c:92 | %p,%p: stub |
| 269 | 40 | activeds | path_GetTypeInfo | fixme-stub |  | 0 | dlls/activeds/pathname.c:98 | %p,%u,%#lx,%p: stub |
| 270 | 40 | activeds | path_GetIDsOfNames | fixme-stub |  | 0 | dlls/activeds/pathname.c:104 | %p,%s,%p,%u,%lu,%p: stub |
| 271 | 40 | activeds | path_Invoke | fixme-stub |  | 0 | dlls/activeds/pathname.c:111 | %p,%ld,%s,%04lx,%04x,%p,%p,%p,%p: stub |
| 272 | 40 | activeds | path_Set | fixme-stub |  | 0 | dlls/activeds/pathname.c:183 | type %ld not implemented |
| 273 | 40 | activeds | path_SetDisplayType | fixme-stub |  | 0 | dlls/activeds/pathname.c:234 | %p,%ld: stub |
| 274 | 40 | activeds | path_Retrieve | fixme-stub |  | 0 | dlls/activeds/pathname.c:240 | type %ld not implemented |
| 275 | 40 | activeds | path_AddLeafElement | fixme-stub |  | 0 | dlls/activeds/pathname.c:355 | %p,%s: stub |
| 276 | 40 | activeds | path_RemoveLeafElement | fixme-stub |  | 0 | dlls/activeds/pathname.c:361 | %p: stub |
| 277 | 40 | activeds | path_CopyPath | fixme-stub |  | 0 | dlls/activeds/pathname.c:367 | %p,%p: stub |
| 278 | 40 | activeds | path_GetEscapedElement | fixme-stub |  | 0 | dlls/activeds/pathname.c:373 | %p,%ld,%s,%p: stub |
| 279 | 40 | activeds | path_get_EscapedMode | fixme-stub |  | 0 | dlls/activeds/pathname.c:379 | %p,%p: stub |
| 280 | 40 | activeds | path_put_EscapedMode | fixme-stub |  | 0 | dlls/activeds/pathname.c:385 | %p,%ld: stub |
| 281 | 40 | activeds | factory_QueryInterface | fixme-stub |  | 0 | dlls/activeds/pathname.c:454 | interface %s is not implemented |
| 282 | 40 | activeds | factory_LockServer | fixme-stub |  | 0 | dlls/activeds/pathname.c:510 | %p,%d: stub |
| 283 | 40 | activeds | DllGetClassObject | fixme-stub |  | 0 | dlls/activeds/pathname.c:543 | class %s/%s is not implemented |
| 284 | 40 | advapi32 | LsaCreateTrustedDomainEx | fixme-stub |  | 0 | dlls/advapi32/lsa.c:228 | (%p,%p,%p,0x%08lx,%p) stub |
| 285 | 40 | advapi32 | LsaDeleteTrustedDomain | fixme-stub |  | 0 | dlls/advapi32/lsa.c:244 | (%p,%p) stub |
| 286 | 40 | advapi32 | LsaEnumerateAccountRights | fixme-stub |  | 0 | dlls/advapi32/lsa.c:254 | (%p,%p,%p,%p) stub |
| 287 | 40 | advapi32 | LsaEnumerateAccounts | fixme-stub |  | 0 | dlls/advapi32/lsa.c:270 | (%p,%p,%p,%ld,%p) stub |
| 288 | 40 | advapi32 | LsaEnumerateAccountsWithUserRight | fixme-stub |  | 0 | dlls/advapi32/lsa.c:286 | (%p,%p,%p,%p) stub |
| 289 | 40 | advapi32 | LsaEnumerateTrustedDomains | fixme-stub |  | 0 | dlls/advapi32/lsa.c:318 | (%p,%p,%p,0x%08lx,%p) stub |
| 290 | 40 | advapi32 | LsaEnumerateTrustedDomainsEx | fixme-stub |  | 0 | dlls/advapi32/lsa.c:336 | (%p,%p,%p,0x%08lx,%p) stub |
| 291 | 40 | advapi32 | LsaLookupNames | fixme-stub |  | 0 | dlls/advapi32/lsa.c:386 | (%p,0x%08lx,%p,%p,%p) stub |
| 292 | 40 | advapi32 | LsaOpenTrustedDomainByName | fixme-stub |  | 0 | dlls/advapi32/lsa.c:780 | (%p,%p,0x%08lx,%p) stub |
| 293 | 40 | advapi32 | LsaQueryInformationPolicy | fixme-stub |  | 0 | dlls/advapi32/lsa.c:804 | category %d not implemented |
| 294 | 40 | advapi32 | LsaQueryTrustedDomainInfo | fixme-stub |  | 0 | dlls/advapi32/lsa.c:953 | (%p,%p,%d,%p) stub |
| 295 | 40 | advapi32 | LsaQueryTrustedDomainInfoByName | fixme-stub |  | 0 | dlls/advapi32/lsa.c:967 | (%p,%p,%d,%p) stub |
| 296 | 40 | advapi32 | LsaRegisterPolicyChangeNotification | fixme-stub |  | 0 | dlls/advapi32/lsa.c:981 | (%d,%p) stub |
| 297 | 40 | advapi32 | LsaRemoveAccountRights | fixme-stub |  | 0 | dlls/advapi32/lsa.c:993 | (%p,%p,%d,%p,0x%08lx) stub |
| 298 | 40 | advapi32 | LsaRetrievePrivateData | fixme-stub |  | 0 | dlls/advapi32/lsa.c:1018 | (%p,%p,%p) stub |
| 299 | 40 | advapi32 | LsaSetInformationPolicy | fixme-stub |  | 0 | dlls/advapi32/lsa.c:1041 | (%p,%s,%p) stub |
| 300 | 40 | advapi32 | LsaSetSecret | fixme-stub |  | 0 | dlls/advapi32/lsa.c:1065 | (%p,%p,%p) stub |

## Remaining per DLL

| dll | total | fixme-stub | notimpl | fake-success | spec-stub |
|---|---|---|---|---|---|
| mshtml | 2102 | 91 | 1963 | 43 | 5 |
| msvcp60 | 1338 | 1 | 0 | 0 | 1337 |
| msvcp70 | 1208 | 0 | 0 | 0 | 1208 |
| win32u | 1178 | 62 | 4 | 1 | 1111 |
| msvcp90 | 922 | 57 | 0 | 0 | 865 |
| msvcp80 | 898 | 0 | 0 | 0 | 898 |
| ntoskrnl.exe | 868 | 106 | 4 | 3 | 755 |
| msvcp71 | 816 | 0 | 0 | 0 | 816 |
| shell32 | 472 | 255 | 139 | 8 | 70 |
| msvcp110 | 431 | 0 | 0 | 0 | 431 |
| dbgeng | 430 | 377 | 52 | 0 | 1 |
| msvcr110 | 407 | 0 | 0 | 0 | 407 |
| msvcr120 | 407 | 0 | 0 | 0 | 407 |
| netio.sys | 402 | 10 | 5 | 0 | 387 |
| msvcp120 | 396 | 0 | 0 | 0 | 396 |
| msvcp120_app | 396 | 0 | 0 | 0 | 396 |
| d3drm | 393 | 384 | 9 | 0 | 0 |
| ieframe | 389 | 16 | 360 | 13 | 0 |
| msvcp_win | 388 | 0 | 0 | 0 | 388 |
| msvcp140 | 366 | 0 | 0 | 0 | 366 |
| msvcr120_app | 342 | 0 | 0 | 0 | 342 |
| esent | 336 | 0 | 0 | 0 | 336 |
| setupapi | 323 | 63 | 6 | 6 | 248 |
| msvcr100 | 316 | 0 | 0 | 0 | 316 |
| msxml3 | 312 | 154 | 154 | 4 | 0 |
| d3dx9_36 | 306 | 219 | 15 | 0 | 72 |
| msvcm80 | 302 | 6 | 0 | 0 | 296 |
| winegstreamer | 302 | 229 | 73 | 0 | 0 |
| msvcp100 | 280 | 0 | 0 | 0 | 280 |
| rpcrt4 | 279 | 62 | 2 | 1 | 214 |
| ndis.sys | 274 | 3 | 0 | 0 | 271 |
| netapi32 | 269 | 48 | 0 | 0 | 221 |
| msasn1 | 264 | 5 | 0 | 0 | 259 |
| ntdll | 257 | 121 | 22 | 5 | 109 |
| inetcomm | 253 | 100 | 58 | 8 | 87 |
| concrt140 | 249 | 4 | 0 | 0 | 245 |
| setupx.dll16 | 240 | 18 | 0 | 0 | 222 |
| riched20 | 227 | 160 | 67 | 0 | 0 |
| sapi | 220 | 217 | 3 | 0 | 0 |
| vssapi | 201 | 0 | 1 | 3 | 197 |
| d3d11 | 198 | 153 | 11 | 1 | 33 |
| ucrtbase | 196 | 0 | 0 | 0 | 196 |
| combase | 195 | 25 | 10 | 4 | 156 |
| uiautomationcore | 195 | 122 | 11 | 0 | 62 |
| quartz | 192 | 173 | 18 | 0 | 1 |
| adsldpc | 189 | 0 | 0 | 0 | 189 |
| olecli.dll16 | 175 | 10 | 0 | 0 | 165 |
| kernel32 | 171 | 71 | 0 | 3 | 97 |
| msvcm90 | 171 | 0 | 0 | 0 | 171 |
| wmp | 169 | 3 | 162 | 4 | 0 |
| apphelp | 167 | 12 | 0 | 0 | 155 |
| mscoree | 165 | 60 | 35 | 0 | 70 |
| windowscodecs | 165 | 134 | 26 | 0 | 5 |
| twinapi.appcore | 164 | 44 | 0 | 0 | 120 |
| shlwapi | 163 | 29 | 8 | 2 | 124 |
| propsys | 161 | 9 | 8 | 2 | 142 |
| fltmgr.sys | 160 | 7 | 0 | 0 | 153 |
| wdscore | 159 | 0 | 0 | 0 | 159 |
| msvcr80 | 157 | 0 | 0 | 0 | 157 |
| webservices | 156 | 10 | 96 | 0 | 50 |
| dmime | 156 | 111 | 45 | 0 | 0 |
| msado15 | 155 | 14 | 136 | 5 | 0 |
| urlmon | 153 | 49 | 80 | 1 | 23 |
| compobj.dll16 | 150 | 4 | 0 | 0 | 146 |
| fwpuclnt | 146 | 9 | 0 | 0 | 137 |
| wmvcore | 145 | 21 | 118 | 0 | 6 |
| msvcr90 | 145 | 0 | 0 | 0 | 145 |
| user.exe16 | 142 | 62 | 0 | 2 | 78 |
| kernelbase | 141 | 62 | 6 | 4 | 69 |
| gdi32 | 140 | 26 | 1 | 46 | 67 |
| dx8vb | 136 | 1 | 0 | 0 | 135 |
| ole32 | 135 | 62 | 36 | 5 | 32 |
| pdh | 135 | 13 | 0 | 3 | 119 |
| gdi.exe16 | 135 | 20 | 0 | 5 | 110 |
| advapi32 | 132 | 86 | 3 | 3 | 40 |
| mprapi | 132 | 3 | 0 | 0 | 129 |
| tapi32 | 131 | 129 | 0 | 0 | 2 |
| sechost | 125 | 5 | 0 | 0 | 120 |
| krnl386.exe16 | 125 | 43 | 0 | 0 | 82 |
| cfgmgr32 | 124 | 2 | 0 | 0 | 122 |
| wininet | 124 | 79 | 0 | 3 | 42 |
| dhtmled.ocx | 122 | 73 | 49 | 0 | 0 |
| shcore | 119 | 3 | 6 | 0 | 110 |
| windows.gaming.input | 117 | 117 | 0 | 0 | 0 |
| clusapi | 115 | 7 | 0 | 0 | 108 |
| dnsapi | 114 | 20 | 0 | 0 | 94 |
| mapi32 | 114 | 18 | 0 | 1 | 95 |
| rasapi32 | 114 | 46 | 0 | 0 | 68 |
| spoolss | 114 | 8 | 0 | 0 | 106 |
| shdocvw | 108 | 12 | 2 | 0 | 94 |
| ole2disp.dll16 | 108 | 5 | 0 | 0 | 103 |
| msctf | 107 | 65 | 21 | 0 | 21 |
| appxdeploymentclient | 106 | 22 | 6 | 1 | 77 |
| ksecdd.sys | 104 | 0 | 0 | 0 | 104 |
| windows.media.speech | 103 | 102 | 1 | 0 | 0 |
| mf | 101 | 5 | 42 | 1 | 53 |
| mapistub | 99 | 0 | 0 | 0 | 99 |
| mscorwks | 97 | 0 | 0 | 0 | 97 |
| mfplat | 96 | 6 | 23 | 1 | 66 |
| wuapi | 95 | 8 | 81 | 6 | 0 |
| winsta | 95 | 1 | 7 | 0 | 87 |
| iphlpapi | 94 | 32 | 0 | 1 | 61 |
| crypt32 | 93 | 57 | 2 | 0 | 34 |
| ntdsapi | 92 | 6 | 0 | 0 | 86 |
| ole2.dll16 | 92 | 13 | 0 | 0 | 79 |
| hnetcfg | 91 | 21 | 61 | 9 | 0 |
| w32skrnl | 91 | 0 | 0 | 0 | 91 |
| dpnet | 91 | 56 | 34 | 1 | 0 |
| msi | 90 | 20 | 16 | 25 | 29 |
| dmstyle | 90 | 79 | 11 | 0 | 0 |
| qedit | 90 | 69 | 21 | 0 | 0 |
| user32 | 89 | 60 | 1 | 2 | 26 |
| explorer | 89 | 8 | 81 | 0 | 0 |
| msdaps | 89 | 78 | 11 | 0 | 0 |
| bluetoothapis | 88 | 6 | 0 | 0 | 82 |
| dxgi | 86 | 82 | 3 | 1 | 0 |
| msdrm | 86 | 1 | 0 | 0 | 85 |
| msvcp140_2 | 84 | 0 | 0 | 0 | 84 |
| dplayx | 84 | 82 | 0 | 2 | 0 |
| qdvd | 83 | 4 | 79 | 0 | 0 |
| dbghelp | 82 | 21 | 0 | 2 | 59 |
| hal | 81 | 10 | 0 | 0 | 71 |
| ncrypt | 80 | 2 | 0 | 0 | 78 |
| msls31 | 79 | 0 | 0 | 0 | 79 |
| wintrust | 76 | 36 | 0 | 2 | 38 |
| windows.web | 76 | 71 | 5 | 0 | 0 |
| atmlib | 76 | 2 | 0 | 0 | 74 |
| d3dx10_43 | 75 | 45 | 4 | 0 | 26 |
| wer | 73 | 5 | 0 | 0 | 68 |
| winspool.drv | 72 | 16 | 4 | 6 | 46 |
| evr | 72 | 10 | 53 | 0 | 9 |
| d3dx9_26 | 72 | 0 | 0 | 0 | 72 |
| d3dx9_27 | 72 | 0 | 0 | 0 | 72 |
| d3dx9_28 | 72 | 0 | 0 | 0 | 72 |
| d3dx9_29 | 72 | 0 | 0 | 0 | 72 |
| d3dx9_30 | 72 | 0 | 0 | 0 | 72 |
| d3dx9_32 | 72 | 0 | 0 | 0 | 72 |
| d3dx9_33 | 72 | 0 | 0 | 0 | 72 |
| d3dx9_34 | 72 | 0 | 0 | 0 | 72 |
| d3dx9_35 | 72 | 0 | 0 | 0 | 72 |
| d3dx9_37 | 72 | 0 | 0 | 0 | 72 |
| d3dx9_38 | 72 | 0 | 0 | 0 | 72 |
| d3dx9_39 | 72 | 0 | 0 | 0 | 72 |
| d3dx9_40 | 72 | 0 | 0 | 0 | 72 |
| d3dx9_41 | 72 | 0 | 0 | 0 | 72 |
| bcp47langs | 71 | 0 | 0 | 0 | 71 |
| gdiplus | 71 | 58 | 0 | 0 | 13 |
| windows.applicationmodel | 71 | 69 | 2 | 0 | 0 |
| resutils | 71 | 0 | 0 | 0 | 71 |
| wiaservc | 70 | 16 | 0 | 0 | 54 |
| d3dx9_31 | 70 | 0 | 0 | 0 | 70 |
| localspl | 69 | 6 | 2 | 0 | 61 |
| mscms | 69 | 23 | 5 | 0 | 41 |
| regapi | 69 | 0 | 0 | 0 | 69 |
| wbemdisp | 68 | 5 | 62 | 1 | 0 |
| d3dx9_25 | 68 | 0 | 0 | 0 | 68 |
| oleaut32 | 67 | 32 | 6 | 2 | 27 |
| d3dx9_42 | 67 | 0 | 0 | 0 | 67 |
| d3dx9_43 | 67 | 0 | 0 | 0 | 67 |
| imm32 | 66 | 21 | 0 | 0 | 45 |
| qcap | 65 | 33 | 32 | 0 | 0 |
| d3dx9_24 | 65 | 0 | 0 | 0 | 65 |
| samlib | 65 | 0 | 0 | 0 | 65 |
| jscript | 63 | 28 | 35 | 0 | 0 |
| bcrypt | 61 | 9 | 29 | 0 | 23 |
| atl | 61 | 32 | 20 | 6 | 3 |
| d2d1 | 60 | 53 | 6 | 0 | 1 |
| npptools | 60 | 0 | 0 | 0 | 60 |
| windows.media.mediacontrol | 58 | 50 | 0 | 0 | 8 |
| msvcrt | 57 | 33 | 1 | 0 | 23 |
| odbc32 | 57 | 0 | 0 | 0 | 57 |
| wpcap | 56 | 3 | 0 | 1 | 52 |
| d3d8thk | 56 | 0 | 0 | 0 | 56 |
| tdi.sys | 56 | 0 | 0 | 0 | 56 |
| msscript.ocx | 55 | 3 | 47 | 4 | 1 |
| rtutils | 55 | 1 | 0 | 0 | 54 |
| itss | 55 | 0 | 54 | 0 | 1 |
| dcomp | 53 | 28 | 13 | 1 | 11 |
| vbscript | 53 | 7 | 46 | 0 | 0 |
| dsdmo | 52 | 40 | 12 | 0 | 0 |
| srvcli | 52 | 0 | 0 | 0 | 52 |
| imm.dll16 | 51 | 0 | 0 | 0 | 51 |
| bthprops.cpl | 50 | 0 | 0 | 0 | 50 |
| adsldp | 50 | 50 | 0 | 0 | 0 |
| dxva2 | 50 | 36 | 13 | 0 | 1 |
| feclient | 50 | 0 | 0 | 0 | 50 |
| windows.globalization | 49 | 49 | 0 | 0 | 0 |
| wsdapi | 49 | 3 | 15 | 0 | 31 |
| windows.devices.enumeration | 48 | 48 | 0 | 0 | 0 |
| wined3d | 48 | 44 | 4 | 0 | 0 |
| scsiport.sys | 48 | 0 | 0 | 0 | 48 |
| comdlg32 | 47 | 26 | 17 | 0 | 4 |
| mpr | 47 | 34 | 0 | 0 | 13 |
| sspicli | 47 | 0 | 0 | 0 | 47 |
| display.drv16 | 47 | 2 | 0 | 0 | 45 |
| dmusic | 47 | 41 | 6 | 0 | 0 |
| wtsapi32 | 46 | 43 | 1 | 0 | 2 |
| ddraw | 46 | 27 | 3 | 0 | 16 |
| olecli32 | 45 | 4 | 0 | 0 | 41 |
| scrobj | 45 | 14 | 30 | 1 | 0 |
| dwmapi | 44 | 8 | 0 | 0 | 36 |
| dhcpcsvc | 44 | 2 | 0 | 0 | 42 |
| wbemprox | 44 | 18 | 24 | 2 | 0 |
| coremessaging | 44 | 5 | 11 | 0 | 28 |
| windows.ui | 44 | 39 | 3 | 0 | 2 |
| dwrite | 43 | 21 | 20 | 2 | 0 |
| wsnmp32 | 43 | 0 | 0 | 0 | 43 |
| authz | 42 | 6 | 1 | 0 | 35 |
| diasymreader | 42 | 0 | 5 | 32 | 5 |
| win32s16.dll16 | 42 | 1 | 0 | 0 | 41 |
| wintab.dll16 | 42 | 42 | 0 | 0 | 0 |
| wow64 | 41 | 14 | 8 | 0 | 19 |
| gphoto2.ds | 41 | 41 | 0 | 0 | 0 |
| opengl32 | 41 | 30 | 11 | 0 | 0 |
| ktmw32 | 41 | 0 | 0 | 0 | 41 |
| wimgapi | 40 | 3 | 0 | 0 | 37 |
| amstream | 40 | 33 | 7 | 0 | 0 |
| secur32 | 39 | 16 | 4 | 0 | 19 |
| dmcompos | 39 | 31 | 8 | 0 | 0 |
| query | 39 | 0 | 1 | 0 | 38 |
| irprops.cpl | 39 | 0 | 0 | 0 | 39 |
| oleacc | 38 | 1 | 34 | 1 | 2 |
| comsvcs | 38 | 14 | 8 | 0 | 16 |
| msvideo.dll16 | 38 | 0 | 0 | 0 | 38 |
| cryptui | 37 | 4 | 0 | 0 | 33 |
| uxtheme | 37 | 8 | 0 | 0 | 29 |
| msdasql | 37 | 4 | 28 | 5 | 0 |
| slc | 37 | 1 | 0 | 0 | 36 |
| d3dx11_43 | 37 | 14 | 0 | 0 | 23 |
| msacm.dll16 | 37 | 28 | 0 | 0 | 9 |
| hlink | 36 | 2 | 21 | 1 | 12 |
| rasdlg | 36 | 1 | 0 | 0 | 35 |
| utildll | 36 | 0 | 0 | 0 | 36 |
| wlanapi | 35 | 8 | 0 | 0 | 27 |
| sppc | 35 | 4 | 0 | 0 | 31 |
| winscard | 35 | 3 | 0 | 0 | 32 |
| wmasf | 35 | 0 | 0 | 0 | 35 |
| tdh | 34 | 2 | 0 | 0 | 32 |
| dinput | 34 | 34 | 0 | 0 | 0 |
| mfmediaengine | 34 | 31 | 2 | 0 | 1 |
| activeds | 33 | 25 | 0 | 0 | 8 |
| services | 33 | 2 | 31 | 0 | 0 |
| msvcrtd | 33 | 0 | 0 | 0 | 33 |
| inseng | 32 | 1 | 24 | 1 | 6 |
| ntprint | 32 | 0 | 0 | 0 | 32 |
| d3d9 | 31 | 27 | 1 | 0 | 3 |
| taskschd | 31 | 30 | 0 | 0 | 1 |
| windows.storage | 31 | 29 | 2 | 0 | 0 |
| hhctrl.ocx | 31 | 1 | 30 | 0 | 0 |
| oledb32 | 31 | 20 | 11 | 0 | 0 |
| windows.networking.connectivity | 30 | 25 | 0 | 0 | 5 |
| d3d10 | 30 | 25 | 1 | 0 | 4 |
| mstask | 30 | 14 | 2 | 0 | 14 |
| fltlib | 29 | 5 | 0 | 0 | 24 |
| chakra | 29 | 0 | 0 | 0 | 29 |
| cng.sys | 29 | 0 | 0 | 0 | 29 |
| dpvoice | 28 | 5 | 23 | 0 | 0 |
| wldp | 28 | 0 | 0 | 2 | 26 |
| drmclien | 28 | 0 | 0 | 0 | 28 |
| odbcbcp | 28 | 0 | 0 | 0 | 28 |
| winhttp | 27 | 15 | 11 | 1 | 0 |
| cryptext | 27 | 2 | 0 | 0 | 25 |
| mssign32 | 27 | 5 | 0 | 0 | 22 |
| dmscript | 27 | 21 | 6 | 0 | 0 |
| windows.security.authentication.onlineid | 26 | 20 | 6 | 0 | 0 |
| msvcr70 | 26 | 0 | 0 | 0 | 26 |
| mswsock | 26 | 0 | 0 | 0 | 26 |
| dmsynth | 26 | 25 | 1 | 0 | 0 |
| msvcr71 | 26 | 0 | 0 | 0 | 26 |
| winnls.dll16 | 26 | 0 | 0 | 0 | 26 |
| powrprof | 25 | 25 | 0 | 0 | 0 |
| fusion | 25 | 13 | 3 | 1 | 8 |
| gamingtcui | 25 | 2 | 0 | 0 | 23 |
| inkobj | 25 | 21 | 3 | 1 | 0 |
| opcservices | 25 | 24 | 1 | 0 | 0 |
| wintab32 | 25 | 25 | 0 | 0 | 0 |
| slbcsp | 25 | 0 | 0 | 0 | 25 |
| usbd.sys | 25 | 1 | 0 | 0 | 24 |
| windows.storage.applicationdata | 24 | 23 | 0 | 0 | 1 |
| httpapi | 24 | 5 | 5 | 0 | 14 |
| sccbase | 24 | 0 | 0 | 0 | 24 |
| windows.security.authentication.web.core | 23 | 19 | 4 | 0 | 0 |
| geolocation | 23 | 23 | 0 | 0 | 0 |
| netcfgx | 23 | 1 | 6 | 4 | 12 |
| odbccp32 | 23 | 1 | 16 | 1 | 5 |
| wmiutils | 23 | 5 | 17 | 1 | 0 |
| packager | 23 | 0 | 23 | 0 | 0 |
| d3dx11_42 | 23 | 0 | 0 | 0 | 23 |
| gpkcsp | 23 | 0 | 0 | 0 | 23 |
| winusb | 22 | 1 | 0 | 0 | 21 |
| ninput | 22 | 3 | 2 | 0 | 17 |
| d3dcompiler_43 | 22 | 15 | 2 | 0 | 5 |
| inetcpl.cpl | 22 | 3 | 0 | 0 | 19 |
| printui | 22 | 1 | 0 | 0 | 21 |
| xolehlp | 22 | 16 | 3 | 0 | 3 |
| dhcpcsvc6 | 22 | 0 | 0 | 0 | 22 |
| olesvr.dll16 | 22 | 9 | 0 | 0 | 13 |
| wintypes | 21 | 15 | 0 | 1 | 5 |
| tbs | 21 | 3 | 0 | 0 | 18 |
| windows.networking.hostname | 21 | 15 | 0 | 0 | 6 |
| cldapi | 21 | 21 | 0 | 0 | 0 |
| pstorec | 21 | 1 | 18 | 0 | 2 |
| traffic | 21 | 2 | 0 | 0 | 19 |
| virtdisk | 21 | 3 | 0 | 0 | 18 |
| jsproxy | 21 | 0 | 18 | 0 | 3 |
| magnification | 21 | 0 | 0 | 0 | 21 |
| snmpapi | 21 | 0 | 0 | 0 | 21 |
| storage.dll16 | 21 | 2 | 0 | 0 | 19 |
| avifile.dll16 | 21 | 0 | 0 | 0 | 21 |
| comm.drv16 | 21 | 0 | 0 | 0 | 21 |
| cryptowinrt | 20 | 20 | 0 | 0 | 0 |
| dsuiext | 20 | 15 | 0 | 0 | 5 |
| dxtrans | 20 | 1 | 0 | 0 | 19 |
| imagehlp | 20 | 10 | 0 | 0 | 10 |
| threadpoolwinrt | 20 | 20 | 0 | 0 | 0 |
| rtworkq | 20 | 0 | 17 | 0 | 3 |
| svrapi | 20 | 0 | 0 | 0 | 20 |
| vdmdbg | 19 | 2 | 0 | 0 | 17 |
| davclnt | 19 | 0 | 0 | 0 | 19 |
| windows.media | 18 | 18 | 0 | 0 | 0 |
| oledlg | 18 | 18 | 0 | 0 | 0 |
| winevulkan | 18 | 8 | 0 | 0 | 10 |
| dciman32 | 18 | 0 | 1 | 0 | 17 |
| d3dim700 | 18 | 0 | 0 | 0 | 18 |
| windows.perception.stub | 17 | 14 | 1 | 0 | 2 |
| msimtf | 17 | 14 | 0 | 1 | 2 |
| t2embed | 17 | 3 | 0 | 0 | 14 |
| netutils | 17 | 0 | 0 | 0 | 17 |
| dmloader | 17 | 17 | 0 | 0 | 0 |
| vcruntime140 | 16 | 0 | 0 | 0 | 16 |
| sti | 16 | 15 | 0 | 1 | 0 |
| cryptnet | 16 | 4 | 0 | 0 | 12 |
| d3dxof | 16 | 16 | 0 | 0 | 0 |
| dataexchange | 16 | 16 | 0 | 0 | 0 |
| newdev | 16 | 4 | 0 | 0 | 12 |
| opencl | 16 | 16 | 0 | 0 | 0 |
| url | 16 | 3 | 0 | 0 | 13 |
| msv1_0 | 16 | 0 | 0 | 0 | 16 |
| sound.drv16 | 16 | 15 | 0 | 0 | 1 |
| nsi | 15 | 0 | 0 | 0 | 15 |
| prntvpt | 15 | 1 | 0 | 0 | 14 |
| sfc_os | 15 | 3 | 0 | 1 | 11 |
| xmllite | 15 | 8 | 7 | 0 | 0 |
| comctl32 | 14 | 9 | 0 | 1 | 4 |
| mmdevapi | 14 | 1 | 11 | 2 | 0 |
| browseui | 14 | 13 | 1 | 0 | 0 |
| connect | 14 | 1 | 0 | 0 | 13 |
| mfsrcsnk | 14 | 6 | 8 | 0 | 0 |
| qwave | 14 | 3 | 0 | 0 | 11 |
| wscript | 14 | 1 | 13 | 0 | 0 |
| mfplay | 14 | 0 | 12 | 0 | 2 |
| olethk32 | 14 | 0 | 0 | 0 | 14 |
| credui | 13 | 5 | 0 | 2 | 6 |
| windows.media.devices | 13 | 13 | 0 | 0 | 0 |
| mlang | 13 | 4 | 6 | 2 | 1 |
| websocket | 13 | 3 | 0 | 0 | 10 |
| d3dim | 13 | 0 | 0 | 0 | 13 |
| nsiproxy.sys | 13 | 12 | 1 | 0 | 0 |
| msvfw32 | 13 | 11 | 0 | 0 | 2 |
| toolhelp.dll16 | 13 | 8 | 0 | 0 | 5 |
| rstrtmgr | 12 | 8 | 0 | 0 | 4 |
| cryptdlg | 12 | 1 | 1 | 0 | 10 |
| difxapi | 12 | 12 | 0 | 0 | 0 |
| faultrep | 12 | 1 | 0 | 0 | 11 |
| winhlp32 | 12 | 2 | 0 | 10 | 0 |
| pwrshplugin | 12 | 0 | 0 | 0 | 12 |
| wuaueng | 12 | 0 | 0 | 0 | 12 |
| hidparse.sys | 12 | 1 | 0 | 0 | 11 |
| dmband | 12 | 9 | 3 | 0 | 0 |
| winmm | 11 | 10 | 0 | 0 | 1 |
| appxpackaging | 11 | 11 | 0 | 0 | 0 |
| gameux | 11 | 9 | 0 | 1 | 1 |
| loadperf | 11 | 2 | 0 | 1 | 8 |
| olesvr32 | 11 | 8 | 0 | 0 | 3 |
| wsock32 | 11 | 11 | 0 | 0 | 0 |
| schannel | 11 | 0 | 0 | 2 | 9 |
| coml2 | 11 | 0 | 0 | 0 | 11 |
| cryptdll | 11 | 0 | 0 | 0 | 11 |
| d3dcompiler_47 | 11 | 0 | 0 | 0 | 11 |
| msports | 11 | 0 | 0 | 0 | 11 |
| msvcp140_atomic_wait | 11 | 0 | 0 | 0 | 11 |
| srclient | 11 | 0 | 0 | 0 | 11 |
| winebus.sys | 11 | 2 | 9 | 0 | 0 |
| dsound | 10 | 9 | 1 | 0 | 0 |
| windows.devices.usb | 10 | 10 | 0 | 0 | 0 |
| windows.gaming.ui.gamebar | 10 | 10 | 0 | 0 | 0 |
| windows.security.credentials.ui.userconsentverifier | 10 | 10 | 0 | 0 | 0 |
| windows.security.enterprisedata | 10 | 9 | 1 | 0 | 0 |
| advpack | 10 | 10 | 0 | 0 | 0 |
| devenum | 10 | 9 | 1 | 0 | 0 |
| mp3dmod | 10 | 9 | 0 | 0 | 1 |
| qasf | 10 | 10 | 0 | 0 | 0 |
| sane.ds | 10 | 10 | 0 | 0 | 0 |
| uiribbon | 10 | 10 | 0 | 0 | 0 |
| wevtapi | 10 | 0 | 0 | 0 | 10 |
| msmpeg2vdec | 10 | 0 | 0 | 0 | 10 |
| vulkan-1 | 10 | 0 | 0 | 0 | 10 |
| ws2_32 | 9 | 7 | 0 | 0 | 2 |
| mfreadwrite | 9 | 0 | 8 | 1 | 0 |
| sxs | 9 | 0 | 9 | 0 | 0 |
| dsquery | 9 | 4 | 0 | 1 | 4 |
| iertutil | 9 | 9 | 0 | 0 | 0 |
| kerberos | 9 | 1 | 0 | 0 | 8 |
| mgmtapi | 9 | 2 | 0 | 0 | 7 |
| rometadata | 9 | 0 | 9 | 0 | 0 |
| crtdll | 9 | 0 | 0 | 0 | 9 |
| d3dcompiler_46 | 9 | 0 | 0 | 0 | 9 |
| vcomp | 9 | 0 | 0 | 0 | 9 |
| vcomp100 | 9 | 0 | 0 | 0 | 9 |
| vcomp110 | 9 | 0 | 0 | 0 | 9 |
| vcomp120 | 9 | 0 | 0 | 0 | 9 |
| vcomp140 | 9 | 0 | 0 | 0 | 9 |
| vcomp90 | 9 | 0 | 0 | 0 | 9 |
| xpssvcs | 9 | 0 | 0 | 0 | 9 |
| avifil32 | 9 | 9 | 0 | 0 | 0 |
| hid | 8 | 1 | 0 | 0 | 7 |
| windows.devices.bluetooth | 8 | 8 | 0 | 0 | 0 |
| dpwsockx | 8 | 6 | 0 | 0 | 2 |
| msdelta | 8 | 1 | 0 | 0 | 7 |
| xactengine3_7 | 8 | 4 | 4 | 0 | 0 |
| msident | 8 | 0 | 7 | 1 | 0 |
| amsi | 8 | 0 | 0 | 4 | 4 |
| atl80 | 8 | 0 | 0 | 0 | 8 |
| wofutil | 8 | 0 | 0 | 0 | 8 |
| keyboard.drv16 | 8 | 2 | 0 | 0 | 6 |
| acledit | 7 | 1 | 0 | 0 | 6 |
| hvsimanagementapi | 7 | 7 | 0 | 0 | 0 |
| winex11.drv | 7 | 6 | 1 | 0 | 0 |
| wmphoto | 7 | 7 | 0 | 0 | 0 |
| pidgen | 7 | 0 | 0 | 2 | 5 |
| atl100 | 7 | 0 | 0 | 0 | 7 |
| atl110 | 7 | 0 | 0 | 0 | 7 |
| atl90 | 7 | 0 | 0 | 0 | 7 |
| ksproxy.ax | 7 | 0 | 0 | 0 | 7 |
| mssip32 | 7 | 0 | 0 | 0 | 7 |
| commdlg.dll16 | 7 | 5 | 0 | 2 | 0 |
| d3d8 | 7 | 7 | 0 | 0 | 0 |
| mciavi32 | 7 | 7 | 0 | 0 | 0 |
| msacm32 | 7 | 6 | 0 | 0 | 1 |
| ole2conv.dll16 | 7 | 0 | 0 | 0 | 7 |
| rasapi16.dll16 | 7 | 0 | 0 | 0 | 7 |
| qmgr | 6 | 5 | 1 | 0 | 0 |
| windows.system.profile.systemmanufacturers | 6 | 6 | 0 | 0 | 0 |
| wpc | 6 | 0 | 5 | 0 | 1 |
| cryptxml | 6 | 6 | 0 | 0 | 0 |
| d3d10_1 | 6 | 1 | 0 | 0 | 5 |
| mspatcha | 6 | 6 | 0 | 0 | 0 |
| winemapi | 6 | 6 | 0 | 0 | 0 |
| wineps.drv | 6 | 5 | 1 | 0 | 0 |
| wlanui | 6 | 0 | 0 | 0 | 6 |
| ole2thk.dll16 | 6 | 0 | 0 | 0 | 6 |
| w32sys.dll16 | 6 | 0 | 0 | 0 | 6 |
| userenv | 5 | 3 | 0 | 2 | 0 |
| d3d12 | 5 | 1 | 0 | 0 | 4 |
| explorerframe | 5 | 3 | 2 | 0 | 0 |
| graphicscapture | 5 | 5 | 0 | 0 | 0 |
| hrtfapo | 5 | 1 | 0 | 0 | 4 |
| mciqtz32 | 5 | 5 | 0 | 0 | 0 |
| winemac.drv | 5 | 4 | 1 | 0 | 0 |
| updspapi | 5 | 0 | 0 | 1 | 4 |
| dssenh | 5 | 0 | 0 | 0 | 5 |
| mscat32 | 5 | 0 | 0 | 0 | 5 |
| scarddlg | 5 | 0 | 0 | 0 | 5 |
| strmdll | 5 | 0 | 0 | 0 | 5 |
| winnls32 | 5 | 0 | 0 | 0 | 5 |
| wmilib.sys | 5 | 0 | 0 | 0 | 5 |
| stress.dll16 | 5 | 2 | 0 | 0 | 3 |
| twain.dll16 | 5 | 1 | 0 | 0 | 4 |
| cabinet | 4 | 2 | 0 | 0 | 2 |
| ddrawex | 4 | 4 | 0 | 0 | 0 |
| msctfmonitor | 4 | 1 | 0 | 0 | 3 |
| msttsengine | 4 | 4 | 0 | 0 | 0 |
| rsaenh | 4 | 4 | 0 | 0 | 0 |
| winecoreaudio.drv | 4 | 3 | 1 | 0 | 0 |
| rpcss | 4 | 4 | 0 | 0 | 0 |
| appwiz.cpl | 4 | 0 | 4 | 0 | 0 |
| d3dx10_39 | 4 | 0 | 0 | 0 | 4 |
| ksuser | 4 | 0 | 0 | 0 | 4 |
| msdmo | 4 | 0 | 0 | 0 | 4 |
| msftedit | 4 | 0 | 0 | 0 | 4 |
| msisip | 4 | 0 | 0 | 0 | 4 |
| mmsystem.dll16 | 4 | 2 | 0 | 0 | 2 |
| wshom.ocx | 3 | 3 | 0 | 0 | 0 |
| cryptbase | 3 | 0 | 0 | 0 | 3 |
| nddeapi | 3 | 1 | 0 | 0 | 2 |
| uianimation | 3 | 2 | 0 | 1 | 0 |
| wineoss.drv | 3 | 2 | 1 | 0 | 0 |
| wldap32 | 3 | 2 | 0 | 0 | 1 |
| wnaspi32 | 3 | 1 | 0 | 1 | 1 |
| dllhost | 3 | 3 | 0 | 0 | 0 |
| klist | 3 | 3 | 0 | 0 | 0 |
| wusa | 3 | 2 | 0 | 1 | 0 |
| infosoft | 3 | 0 | 1 | 2 | 0 |
| d3dcompiler_38 | 3 | 0 | 0 | 0 | 3 |
| d3dcompiler_39 | 3 | 0 | 0 | 0 | 3 |
| d3dcompiler_40 | 3 | 0 | 0 | 0 | 3 |
| d3dcompiler_41 | 3 | 0 | 0 | 0 | 3 |
| d3dcompiler_42 | 3 | 0 | 0 | 0 | 3 |
| mmcndmgr | 3 | 0 | 0 | 0 | 3 |
| softpub | 3 | 0 | 0 | 0 | 3 |
| unicows | 3 | 0 | 0 | 0 | 3 |
| xpsprint | 3 | 0 | 0 | 0 | 3 |
| wineusb.sys | 3 | 0 | 3 | 0 | 0 |
| shell.dll16 | 3 | 1 | 0 | 0 | 2 |
| win87em.dll16 | 3 | 3 | 0 | 0 | 0 |
| mouse.drv16 | 3 | 0 | 0 | 0 | 3 |
| typelib.dll16 | 3 | 0 | 0 | 0 | 3 |
| scrrun | 2 | 1 | 0 | 0 | 1 |
| windows.networking | 2 | 1 | 0 | 0 | 1 |
| dmusic32 | 2 | 2 | 0 | 0 | 0 |
| dpnhpast | 2 | 1 | 1 | 0 | 0 |
| fontsub | 2 | 1 | 0 | 0 | 1 |
| icmui | 2 | 1 | 0 | 0 | 1 |
| midimap | 2 | 1 | 0 | 0 | 1 |
| msacm32.drv | 2 | 2 | 0 | 0 | 0 |
| msvidc32 | 2 | 2 | 0 | 0 | 0 |
| objsel | 2 | 2 | 0 | 0 | 0 |
| schedsvc | 2 | 2 | 0 | 0 | 0 |
| xaudio2_7 | 2 | 2 | 0 | 0 | 0 |
| wordpad | 2 | 1 | 1 | 0 | 0 |
| winprint | 2 | 0 | 1 | 0 | 1 |
| sensapi | 2 | 0 | 0 | 2 | 0 |
| cryptsp | 2 | 0 | 0 | 0 | 2 |
| d3d12core | 2 | 0 | 0 | 0 | 2 |
| d3dcompiler_33 | 2 | 0 | 0 | 0 | 2 |
| d3dcompiler_34 | 2 | 0 | 0 | 0 | 2 |
| d3dcompiler_35 | 2 | 0 | 0 | 0 | 2 |
| d3dcompiler_36 | 2 | 0 | 0 | 0 | 2 |
| d3dcompiler_37 | 2 | 0 | 0 | 0 | 2 |
| d3dx10_37 | 2 | 0 | 0 | 0 | 2 |
| directmanipulation | 2 | 0 | 0 | 0 | 2 |
| inetmib1 | 2 | 0 | 0 | 0 | 2 |
| initpki | 2 | 0 | 0 | 0 | 2 |
| msvcrt20 | 2 | 0 | 0 | 0 | 2 |
| msvcrt40 | 2 | 0 | 0 | 0 | 2 |
| vcruntime140_1 | 2 | 0 | 0 | 0 | 2 |
| http.sys | 2 | 1 | 1 | 0 | 0 |
| vwin32.vxd | 2 | 2 | 0 | 0 | 0 |
| winaspi.dll16 | 2 | 1 | 0 | 0 | 1 |
| windebug.dll16 | 2 | 1 | 0 | 0 | 1 |
| winsock.dll16 | 2 | 2 | 0 | 0 | 0 |
| mf3216 | 2 | 0 | 0 | 0 | 2 |
| ole2nls.dll16 | 2 | 0 | 0 | 0 | 2 |
| d3d10core | 1 | 0 | 1 | 0 | 0 |
| powershell | 1 | 1 | 0 | 0 | 0 |
| cdosys | 1 | 1 | 0 | 0 | 0 |
| dpnhupnp | 1 | 1 | 0 | 0 | 0 |
| iccvid | 1 | 1 | 0 | 0 | 0 |
| ir50_32 | 1 | 1 | 0 | 0 | 0 |
| mciwave | 1 | 1 | 0 | 0 | 0 |
| msimg32 | 1 | 1 | 0 | 0 | 0 |
| msnet32 | 1 | 1 | 0 | 0 | 0 |
| msrle32 | 1 | 1 | 0 | 0 | 0 |
| olepro32 | 1 | 1 | 0 | 0 | 0 |
| windowscodecsext | 1 | 1 | 0 | 0 | 0 |
| winealsa.drv | 1 | 1 | 0 | 0 | 0 |
| xinput1_3 | 1 | 1 | 0 | 0 | 0 |
| xinput1_4 | 1 | 0 | 0 | 0 | 1 |
| arp | 1 | 1 | 0 | 0 | 0 |
| aspnet_regiis | 1 | 1 | 0 | 0 | 0 |
| cacls | 1 | 1 | 0 | 0 | 0 |
| certutil | 1 | 1 | 0 | 0 | 0 |
| cmd | 1 | 1 | 0 | 0 | 0 |
| conhost | 1 | 1 | 0 | 0 | 0 |
| dism | 1 | 1 | 0 | 0 | 0 |
| dplaysvr | 1 | 1 | 0 | 0 | 0 |
| dpnsvr | 1 | 1 | 0 | 0 | 0 |
| dpvsetup | 1 | 1 | 0 | 0 | 0 |
| dxdiag | 1 | 1 | 0 | 0 | 0 |
| extrac32 | 1 | 1 | 0 | 0 | 0 |
| icacls | 1 | 1 | 0 | 0 | 0 |
| mofcomp | 1 | 1 | 0 | 0 | 0 |
| msiexec | 1 | 1 | 0 | 0 | 0 |
| msinfo32 | 1 | 1 | 0 | 0 | 0 |
| netsh | 1 | 1 | 0 | 0 | 0 |
| netstat | 1 | 1 | 0 | 0 | 0 |
| ngen | 1 | 1 | 0 | 0 | 0 |
| pnputil | 1 | 1 | 0 | 0 | 0 |
| regasm | 1 | 1 | 0 | 0 | 0 |
| regedit | 1 | 1 | 0 | 0 | 0 |
| regini | 1 | 1 | 0 | 0 | 0 |
| regsvcs | 1 | 1 | 0 | 0 | 0 |
| sdbinst | 1 | 1 | 0 | 0 | 0 |
| servicemodelreg | 1 | 1 | 0 | 0 | 0 |
| setx | 1 | 1 | 0 | 0 | 0 |
| subst | 1 | 1 | 0 | 0 | 0 |
| systeminfo | 1 | 1 | 0 | 0 | 0 |
| wevtutil | 1 | 1 | 0 | 0 | 0 |
| whoami | 1 | 1 | 0 | 0 | 0 |
| wmplayer | 1 | 1 | 0 | 0 | 0 |
| unidrv | 1 | 0 | 1 | 0 | 0 |
| unidrvui | 1 | 0 | 1 | 0 | 0 |
| winedmo | 1 | 0 | 1 | 0 | 0 |
| winewayland.drv | 1 | 0 | 1 | 0 | 0 |
| dxdiagn | 1 | 0 | 0 | 1 | 0 |
| ctl3d32 | 1 | 0 | 0 | 0 | 1 |
| d3dx10_38 | 1 | 0 | 0 | 0 | 1 |
| itircl | 1 | 0 | 0 | 0 | 1 |
| msvcirt | 1 | 0 | 0 | 0 | 1 |
| odbccu32 | 1 | 0 | 0 | 0 | 1 |
| photometadatahandler | 1 | 0 | 0 | 0 | 1 |
| sas | 1 | 0 | 0 | 0 | 1 |
| scardsvr | 1 | 0 | 0 | 0 | 1 |
| xinputuap | 1 | 0 | 0 | 0 | 1 |
| ifsmgr.vxd | 1 | 1 | 0 | 0 | 0 |
| mmdevldr.vxd | 1 | 1 | 0 | 0 | 0 |
| monodebg.vxd | 1 | 1 | 0 | 0 | 0 |
| vdhcp.vxd | 1 | 1 | 0 | 0 | 0 |
| vmm.vxd | 1 | 1 | 0 | 0 | 0 |
| vnbt.vxd | 1 | 1 | 0 | 0 | 0 |
| vnetbios.vxd | 1 | 1 | 0 | 0 | 0 |
| vtdapi.vxd | 1 | 1 | 0 | 0 | 0 |
| winebth.sys | 1 | 1 | 0 | 0 | 0 |
| winexinput.sys | 1 | 1 | 0 | 0 | 0 |
| ctl3d.dll16 | 1 | 0 | 0 | 0 | 1 |
| ctl3dv2.dll16 | 1 | 0 | 0 | 0 | 1 |
| ole2prox.dll16 | 1 | 0 | 0 | 0 | 1 |
| system.drv16 | 1 | 0 | 0 | 0 | 1 |
| ver.dll16 | 1 | 0 | 0 | 0 | 1 |
