# Stub audit 3 (2026-10-09)

Tree: the wine-sg full series (patches/series through sg/1695) applied to wine-10.0, in /var/tmp/audit5/wine-10.0.

Kinds: fixme-stub (a FIXME saying stub/semi-stub/partial/not implemented), notimpl (returns E_NOTIMPL, STATUS_NOT_IMPLEMENTED or ERROR_CALL_NOT_IMPLEMENTED), fake-success (a FIXME and a success return with no work), spec-stub (an "@ stub" export).

Ranking: runtime FIXME hits in 21862 logs of real program runs under /var/tmp (distinct logs, x8, capped), imports by 791 PE files of real programs (Chrome, Firefox, Edge, VS Code, Audacity, Paint.NET and its .NET/WPF runtime, Steam setup, DYMO, AnyDesk, 7-Zip; x60 per program), DLL tier (core +30, legacy/16-bit -40).

Totals: 36400 items: spec-stub 25314, fixme-stub 5888, notimpl 4851, fake-success 347

## Ranked (top 300)

| # | score | dll | function | kind | programs | logs | where | note |
|---|---|---|---|---|---|---|---|---|
| 1 | 620 | ntdll | NtQuerySystemInformation | fixme-stub | chrome firefox pdn2 vscode | 71 | dlls/ntdll/unix/system.c:3218 | stub info_class SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION |
| 2 | 520 | ntdll | NtFsControlFile | fixme-stub |  | 80 | dlls/ntdll/unix/file.c:7401 | stub! return success - Unsupported fsctl %x (device=%x access=%x func=%x method=%x) |
| 3 | 323 | ntdll | NtQueryInformationProcess | notimpl | Edge chrome firefox pdn2 vscode | 20 | dlls/ntdll/unix/process.c:1535 | (%p,info_class=%d,%p,0x%08x,%p) Unknown information class |
| 4 | 296 | ntdll | RtlSetHeapInformation | fixme-stub |  | 32 | dlls/ntdll/heap.c:2581 | HeapCompatibilityInformation %lu not implemented! |
| 5 | 290 | ntdll | NtSetInformationThread | fixme-stub | pdn2 vscode | 25 | dlls/ntdll/unix/thread.c:2522 | info class %d not supported yet |
| 6 | 284 | advapi32 | SetSecurityInfo | fixme-stub | Edge chrome firefox vscode | 18 | dlls/advapi32/security.c:3694 | [partly DAVID: window station SDs] unimplemented type %u, returning success |
| 7 | 278 | kernelbase | Wow64GetThreadContext | notimpl | Edge audacity chrome vscode | 0 | dlls/kernelbase/thread.c:889 | STATUS_NOT_IMPLEMENTED |
| 8 | 250 | dcomp | DCompositionCreateDevice3 | notimpl | firefox | 19 | dlls/dcomp/device.c:961 | %p, %s, %p. |
| 9 | 232 | mscoree | parse_supported_runtime | fixme-stub |  | 24 | dlls/mscoree/config.c:362 | sku=%s not implemented |
| 10 | 195 | secur32 | GetUserNameExW | fixme-stub | Edge chrome firefox | 10 | dlls/secur32/secur32.c:1125 | NameFormat %d not implemented |
| 11 | 190 | kernelbase | FindFirstFileExW | fixme-stub | Edge audacity chrome firefox pdn2 vscode | 0 | dlls/kernelbase/file.c:1479 | search_op not implemented 0x%08x |
| 12 | 188 | bcrypt | BCryptOpenAlgorithmProvider | notimpl | Edge audacity chrome firefox pdn2 vscode | 0 | dlls/bcrypt/bcrypt_main.c:307 | algorithm %s not supported |
| 13 | 184 | ntdll | NtSetInformationProcess | fixme-stub |  | 18 | dlls/ntdll/unix/process.c:2412 | (%p,0x%08x,%p,0x%08x) stub |
| 14 | 184 | ntdll | NtSetInformationJobObject | fixme-stub |  | 18 | dlls/ntdll/unix/sync.c:925 | stub: %p %u %p %u |
| 15 | 182 | d3d11 | d3d11_device_CheckFeatureSupport | notimpl |  | 18 | dlls/d3d11/device.c:4896 | Returning fake threading support data. |
| 16 | 174 | ntdll | server_get_file_info | notimpl |  | 17 | dlls/ntdll/unix/file.c:2232 | Unsupported info class %x |
| 17 | 168 | dwmapi | DwmAttachMilContent | fixme-stub |  | 16 | dlls/dwmapi/dwmapi_main.c:636 | (%p) stub |
| 18 | 165 | ws2_32 | WSAIoctl | fixme-stub | audacity chrome firefox pdn2 vscode | 0 | dlls/ws2_32/socket.c:2562 | SIO_UDP_CONNRESET stub |
| 19 | 163 | setupapi | SetupDiOpenDevRegKey | notimpl | Edge audacity chrome firefox vscode | 0 | dlls/setupapi/devinst.c:4345 | Unhandled type %#lx. |
| 20 | 160 | iphlpapi | IpReleaseAddress | fixme-stub | chrome vscode | 0 | dlls/iphlpapi/iphlpapi_main.c:3843 | Stub AdapterInfo %p |
| 21 | 160 | iphlpapi | IpRenewAddress | fixme-stub | chrome vscode | 0 | dlls/iphlpapi/iphlpapi_main.c:3869 | Stub AdapterInfo %p |
| 22 | 160 | oleaut32 | DispCallFunc | fixme-stub | Edge vscode | 0 | dlls/oleaut32/typelib.c:6970 | (%p, %ld, %d, %d, %d, %p, %p, %p (vt=%d)): not implemented for this CPU |
| 23 | 160 | secur32 | LsaLogonUser | fixme-stub | chrome firefox | 0 | dlls/secur32/lsa.c:212 | [DAVID: security model] %p %s %d %ld %p %ld %p %p %p %p %p %p %p %p stub |
| 24 | 160 | userenv | CreateAppContainerProfile | fixme-stub | Edge vscode | 0 | dlls/userenv/userenv_main.c:972 | (%s, %s, %s, %p, %ld, %p): stub |
| 25 | 160 | winusb | WinUsb_Free | fixme-stub | chrome vscode | 0 | dlls/winusb/main.c:32 | (%p) - stub |
| 26 | 150 | dcomp | DCompositionCreateSurfaceHandle | spec-stub | chrome vscode | 0 | dlls/dcomp/*.spec:0 |  |
| 27 | 150 | mf | MFCreateDeviceSource | spec-stub | chrome vscode | 0 | dlls/mf/*.spec:0 |  |
| 28 | 150 | winusb | WinUsb_ControlTransfer | spec-stub | chrome vscode | 0 | dlls/winusb/*.spec:0 |  |
| 29 | 150 | winusb | WinUsb_GetAssociatedInterface | spec-stub | chrome vscode | 0 | dlls/winusb/*.spec:0 |  |
| 30 | 150 | winusb | WinUsb_GetOverlappedResult | spec-stub | chrome vscode | 0 | dlls/winusb/*.spec:0 |  |
| 31 | 150 | winusb | WinUsb_Initialize | spec-stub | chrome vscode | 0 | dlls/winusb/*.spec:0 |  |
| 32 | 150 | winusb | WinUsb_ReadPipe | spec-stub | chrome vscode | 0 | dlls/winusb/*.spec:0 |  |
| 33 | 150 | winusb | WinUsb_ResetPipe | spec-stub | chrome vscode | 0 | dlls/winusb/*.spec:0 |  |
| 34 | 150 | winusb | WinUsb_SetCurrentAlternateSetting | spec-stub | chrome vscode | 0 | dlls/winusb/*.spec:0 |  |
| 35 | 150 | winusb | WinUsb_WritePipe | spec-stub | chrome vscode | 0 | dlls/winusb/*.spec:0 |  |
| 36 | 140 | advapi32 | GetSecurityInfo | fixme-stub | Edge chrome firefox vscode | 0 | dlls/advapi32/security.c:1788 | unimplemented type %u |
| 37 | 140 | ncrypt | NCryptExportKey | fixme-stub | Edge audacity chrome vscode | 0 | dlls/ncrypt/main.c:1251 | Parameter information not implemented |
| 38 | 140 | user32 | RegisterDeviceNotificationW | fixme-stub | audacity chrome firefox vscode | 0 | dlls/user32/input.c:569 | type %#lx not implemented |
| 39 | 140 | wintrust | WinVerifyTrust | fixme-stub | Edge chrome firefox vscode | 0 | dlls/wintrust/wintrust_main.c:668 | unimplemented for %ld |
| 40 | 138 | bcrypt | BCryptDecrypt | notimpl | Edge audacity pdn2 vscode | 0 | dlls/bcrypt/bcrypt_main.c:2271 | flags %#lx not supported |
| 41 | 138 | ntdll | NtQueryObject | notimpl | Edge chrome firefox vscode | 0 | dlls/ntdll/unix/file.c:8505 | size %u too small |
| 42 | 136 | win32u | pack_message | fixme-stub |  | 12 | dlls/win32u/message.c:945 | WM_NCPAINT hdc packing not supported yet |
| 43 | 128 | crypt32 | CRYPT_RegControl | fixme-stub |  | 11 | dlls/crypt32/regstore.c:582 | %lu: stub |
| 44 | 128 | msctf | SetInputScope | fixme-stub |  | 11 | dlls/msctf/msctf.c:651 | STUB: %p %i |
| 45 | 120 | bthprops.cpl | BluetoothEnableDiscovery | spec-stub | chrome vscode | 0 | dlls/bthprops.cpl/*.spec:0 |  |
| 46 | 120 | bthprops.cpl | BluetoothEnableIncomingConnections | spec-stub | chrome vscode | 0 | dlls/bthprops.cpl/*.spec:0 |  |
| 47 | 120 | bthprops.cpl | BluetoothIsConnectable | spec-stub | chrome vscode | 0 | dlls/bthprops.cpl/*.spec:0 |  |
| 48 | 118 | kernelbase | get_geo_info | notimpl |  | 10 | dlls/kernelbase/locale.c:1801 | type %u is not supported |
| 49 | 115 | comdlg32 | PrintDlgExW | fixme-stub | chrome firefox vscode | 0 | dlls/comdlg32/printdlg.c:4210 | (%p) semi-stub |
| 50 | 115 | ntdll | NtQueryInformationThread | fixme-stub | Edge chrome vscode | 0 | dlls/ntdll/unix/thread.c:2120 | ThreadIsIoPending info class not supported yet |
| 51 | 115 | ntdll | NtQueryValueKey | fixme-stub | Edge chrome firefox | 0 | dlls/ntdll/unix/registry.c:584 | Information class %d not implemented |
| 52 | 114 | crypt32 | CryptDecodeObjectEx | fixme-stub | Edge vscode | 3 | dlls/crypt32/decode.c:7057 | Unimplemented decoder for lpszStructType OID %d |
| 53 | 112 | rpcrt4 | NdrClearOutParameters | fixme-stub |  | 9 | dlls/rpcrt4/ndr_marshall.c:4660 | (%p,%p,%p): stub |
| 54 | 103 | ntdll | NtSetInformationFile | notimpl | vscode | 5 | dlls/ntdll/unix/file.c:5416 | FILE_DISPOSITION_FORCE_IMAGE_SECTION_CHECK not supported |
| 55 | 100 | advapi32 | LsaAddAccountRights | fixme-stub | chrome | 0 | dlls/advapi32/lsa.c:196 | (%p,%p,%p,0x%08lx) stub |
| 56 | 100 | advapi32 | GetEffectiveRightsFromAclW | fixme-stub | Edge | 0 | dlls/advapi32/security.c:279 | %p %p %p - stub |
| 57 | 100 | combase | CoGetDefaultContext | fixme-stub | firefox | 0 | dlls/combase/combase.c:1285 | %d, %s, %p stub |
| 58 | 100 | combase | CoGetCallerTID | fixme-stub | firefox | 0 | dlls/combase/combase.c:3622 | stub! |
| 59 | 100 | d3d9 | D3DPERF_SetMarker | fixme-stub | vscode | 0 | dlls/d3d9/d3d9_main.c:247 | color 0x%08lx, name %s stub! |
| 60 | 100 | kernelbase | SetWaitableTimerEx | fixme-stub | firefox | 0 | dlls/kernelbase/sync.c:900 | (%p, %p, %ld, %p, %p, %p, %ld) semi-stub |
| 61 | 100 | netapi32 | NetShareEnum | fixme-stub | audacity | 0 | dlls/netapi32/netapi32.c:477 | Stub (%s %ld %p %ld %p %p %p) |
| 62 | 100 | secur32 | LsaEnumerateLogonSessions | fixme-stub | vscode | 0 | dlls/secur32/lsa.c:166 | %p %p stub |
| 63 | 100 | user32 | UnloadKeyboardLayout | fixme-stub | firefox | 0 | dlls/user32/input.c:484 | layout %p, stub! |
| 64 | 100 | user32 | CreateSyntheticPointerDevice | fixme-stub | firefox | 0 | dlls/user32/input.c:955 | type %ld, max_count %ld, mode %d stub! |
| 65 | 100 | userenv | UnloadUserProfile | fixme-stub | Edge | 0 | dlls/userenv/userenv_main.c:833 | (%p, %p): stub |
| 66 | 100 | wer | WerReportAddDump | fixme-stub | Edge | 0 | dlls/wer/main.c:154 | (%p, %p, %p, %d, %p, %p, %lu) :stub |
| 67 | 100 | wer | WerReportAddFile | fixme-stub | Edge | 0 | dlls/wer/main.c:180 | (%p, %s, %d, 0x%lx) :stub |
| 68 | 100 | wer | WerReportSetParameter | fixme-stub | Edge | 0 | dlls/wer/main.c:300 | (%p, %ld, %s, %s) :stub |
| 69 | 100 | wer | WerReportSubmit | fixme-stub | Edge | 0 | dlls/wer/main.c:324 | (%p, %d, 0x%lx, %p) :stub |
| 70 | 100 | wininet | DeleteUrlCacheContainerW | fixme-stub | Edge | 0 | dlls/wininet/urlcache.c:3330 | (0x%08lx, 0x%08lx) stub |
| 71 | 98 | bcrypt | BCryptExportKey | notimpl | vscode | 0 | dlls/bcrypt/bcrypt_main.c:2042 | encryption of key not yet supported |
| 72 | 98 | wlanapi | WlanEnumInterfaces | fixme-stub |  | 11 | dlls/wlanapi/main.c:72 | (%p, %p, %p) semi-stub |
| 73 | 97 | sechost | QueryServiceConfig2W | fixme-stub | Edge | 4 | dlls/sechost/service.c:1177 | Level %ld not implemented |
| 74 | 96 | ntdll | NtQueryWnfStateData | fake-success | firefox | 0 | dlls/ntdll/misc.c:293 | %p %p %p %p %p %p: no states are published |
| 75 | 96 | windows.networking.connectivity | factory_QueryInterface | fixme-stub |  | 7 | dlls/windows.networking.connectivity/network_information.c:40 | %s not implemented, returning E_NOINTERFACE. |
| 76 | 90 | combase | CoGetStdMarshalEx | spec-stub | Edge | 0 | dlls/combase/*.spec:0 |  |
| 77 | 90 | combase | IsErrorPropagationEnabled | spec-stub | firefox | 0 | dlls/combase/*.spec:0 |  |
| 78 | 90 | combase | RoReportFailedDelegate | spec-stub | firefox | 0 | dlls/combase/*.spec:0 |  |
| 79 | 90 | crypt32 | CertVerifyCertificateChainPolicy | fixme-stub | Edge vscode | 0 | dlls/crypt32/chain.c:4060 | unimplemented for %d |
| 80 | 90 | crypt32 | CryptVerifyCertificateSignatureEx | fixme-stub | chrome vscode | 0 | dlls/crypt32/cert.c:3163 | CRYPT_VERIFY_CERT_SIGN_ISSUER_CHAIN: stub |
| 81 | 90 | gdi32 | PlayEnhMetaFileRecord | fixme-stub | chrome vscode | 0 | dlls/gdi32/enhmetafile.c:795 | type %d is unimplemented |
| 82 | 90 | mf | MFCreatePMPMediaSession | spec-stub | firefox | 0 | dlls/mf/*.spec:0 |  |
| 83 | 90 | ntdll | NtOpenKeyEx | fixme-stub | Edge chrome | 0 | dlls/ntdll/unix/registry.c:159 | options %x not implemented |
| 84 | 90 | shcore | PathIsNetworkPathW | spec-stub | firefox | 0 | dlls/shcore/*.spec:0 |  |
| 85 | 90 | user32 | CopyImage | fixme-stub | firefox vscode | 0 | dlls/user32/cursoricon.c:2242 | The flag LR_COPYFROMRESOURCE is not implemented for bitmaps |
| 86 | 90 | user32 | DdeDisconnect | fixme-stub | audacity chrome | 0 | dlls/user32/dde_client.c:1365 | Not implemented yet for a server side conversation |
| 87 | 90 | user32 | DrawFrameControl | fixme-stub | chrome vscode | 0 | dlls/user32/uitools.c:1219 | DFC_POPUPMENU: not implemented |
| 88 | 90 | wininet | InternetSetOptionW | fixme-stub | Edge vscode | 0 | dlls/wininet/internet.c:3543 | Option INTERNET_OPTION_HTTP_VERSION(%ld,%ld): STUB |
| 89 | 90 | winspool.drv | GetPrinterW | fixme-stub | chrome vscode | 0 | dlls/winspool.drv/info.c:4566 | Unimplemented level %ld |
| 90 | 89 | mpr | WNetGetUniversalNameW | fixme-stub | audacity | 3 | dlls/mpr/wnet.c:2765 | (%s, 0x%08lX, %p, %p): stub |
| 91 | 88 | bcrypt | BCryptImportKeyPair | notimpl | Edge vscode | 0 | dlls/bcrypt/bcrypt_main.c:2150 | decryption of key not yet supported |
| 92 | 88 | ntdll | NtQueryVolumeInformationFile | notimpl | audacity vscode | 0 | dlls/ntdll/unix/file.c:8104 | %p: object id info not supported |
| 93 | 88 | win32u | NtUserFlashWindowEx | fixme-stub |  | 6 | dlls/win32u/window.c:5205 | %p - semi-stub |
| 94 | 83 | oleacc | LresultFromObject | notimpl | chrome firefox vscode | 0 | dlls/oleacc/main.c:317 | stream size to big |
| 95 | 82 | explorer | handle_appbarmessage | fixme-stub |  | 9 | programs/explorer/appbar.c:241 | SHAppBarMessage(ABM_GETSTATE): stub |
| 96 | 80 | kernelbase | SetCachedSigningLevel | fixme-stub |  | 5 | dlls/kernelbase/security.c:1641 | %p %lu %lu %p - stub |
| 97 | 78 | tbs | Tbsi_GetDeviceInfo | fixme-stub | chrome | 1 | dlls/tbs/tbs.c:33 | (%u, %p) stub |
| 98 | 70 | authz | AuthzInitializeResourceManager | fixme-stub | audacity | 0 | dlls/authz/authz.c:33 | (0x%lx,%p,%p,%p,%s,%p): stub |
| 99 | 70 | authz | AuthzAccessCheck | fixme-stub | audacity | 0 | dlls/authz/authz.c:68 | (0x%lx,%p,%p,%p,%p,%p,0x%lx,%p,%p): stub |
| 100 | 70 | authz | AuthzFreeContext | fixme-stub | audacity | 0 | dlls/authz/authz.c:85 | (%p): stub |
| 101 | 70 | authz | AuthzInitializeContextFromSid | fixme-stub | audacity | 0 | dlls/authz/authz.c:96 | (0x%lx,%p,%p,%p,%08lx:%08lx,%p,%p): stub |
| 102 | 70 | authz | AuthzInitializeContextFromToken | fixme-stub | audacity | 0 | dlls/authz/authz.c:110 | (0x%lx,%p,%p,%p,%08lx:%08lx,%p,%p): stub |
| 103 | 70 | ninput | SetInteractionConfigurationInteractionContext | fixme-stub | firefox | 0 | dlls/ninput/main.c:132 | context %p, count %u, configuration %p: stub!. |
| 104 | 70 | ninput | RegisterOutputCallbackInteractionContext | fixme-stub | firefox | 0 | dlls/ninput/main.c:149 | context %p, callback %p, data %p: stub!. |
| 105 | 70 | rstrtmgr | RmRegisterResources | fixme-stub | Edge | 0 | dlls/rstrtmgr/main.c:50 | %ld, %d, %p, %d, %p, %d, %p stub! |
| 106 | 70 | rstrtmgr | RmStartSession | fixme-stub | Edge | 0 | dlls/rstrtmgr/main.c:64 | %p, %ld, %p stub! |
| 107 | 70 | rstrtmgr | RmRestart | fixme-stub | Edge | 0 | dlls/rstrtmgr/main.c:75 | %lu, 0x%08lx, %p stub! |
| 108 | 70 | rstrtmgr | RmEndSession | fixme-stub | Edge | 0 | dlls/rstrtmgr/main.c:84 | %lu stub! |
| 109 | 70 | rstrtmgr | RmShutdown | fixme-stub | Edge | 0 | dlls/rstrtmgr/main.c:93 | %lu, 0x%08lx, %p stub! |
| 110 | 70 | shell32 | ApplicationAssociationRegistration_SetAppAsDefault | notimpl |  | 4 | dlls/shell32/assoc.c:1123 | association type %d not supported |
| 111 | 68 | authz | AuthzFreeResourceManager | notimpl | audacity | 0 | dlls/authz/authz.c:47 | %p |
| 112 | 65 | crypt32 | CryptBinaryToStringA | fixme-stub | vscode | 0 | dlls/crypt32/base64.c:302 | Unimplemented type %ld |
| 113 | 65 | crypt32 | CryptBinaryToStringW | fixme-stub | firefox | 0 | dlls/crypt32/base64.c:644 | Unimplemented type %ld |
| 114 | 65 | crypt32 | CryptStringToBinaryA | fixme-stub | vscode | 0 | dlls/crypt32/base64.c:1062 | Unimplemented type %ld |
| 115 | 65 | kernelbase | GetNamedPipeHandleStateW | fixme-stub | vscode | 0 | dlls/kernelbase/sync.c:1716 | %p %p %p %p %p %p %ld: semi-stub |
| 116 | 65 | kernelbase | UrlEscapeW | fixme-stub | Edge | 0 | dlls/kernelbase/path.c:3295 | Unimplemented flags: %08lx |
| 117 | 65 | kernelbase | UrlIsW | fixme-stub | Edge | 0 | dlls/kernelbase/path.c:4758 | (%s %d): stub |
| 118 | 65 | netapi32 | NetUserGetLocalGroups | fixme-stub | vscode | 0 | dlls/netapi32/netapi32.c:1825 | (%s, %s, %ld, %08lx, %p %ld, %p, %p) stub! |
| 119 | 65 | netapi32 | DsRoleGetPrimaryDomainInformation | fixme-stub | chrome | 0 | dlls/netapi32/netapi32.c:2661 | (%p, %d, %p) stub |
| 120 | 65 | ntdll | NtPowerInformation | fixme-stub | firefox | 0 | dlls/ntdll/unix/system.c:5015 | semi-stub: SystemPowerCapabilities |
| 121 | 65 | rpcrt4 | NdrStubCall2 | fixme-stub | Edge | 0 | dlls/rpcrt4/ndr_stubless.c:1228 | pipes not supported yet |
| 122 | 65 | secur32 | LsaGetLogonSessionData | fixme-stub | vscode | 0 | dlls/secur32/lsa.c:183 | %p %p semi-stub |
| 123 | 65 | user32 | LoadKeyboardLayoutW | fixme-stub | vscode | 0 | dlls/user32/input.c:417 | name %s, flags %x, semi-stub! |
| 124 | 65 | wintrust | CryptCATAdminAcquireContext2 | fixme-stub | firefox | 0 | dlls/wintrust/crypt.c:157 | strong policy parameter is unimplemented |
| 125 | 64 | advapi32 | DecryptFileW | fixme-stub |  | 3 | dlls/advapi32/security.c:3463 | (%s, %08lx): stub |
| 126 | 64 | combase | CoRegisterSurrogate | fixme-stub |  | 3 | dlls/combase/combase.c:3698 | %p stub |
| 127 | 64 | qmgr | BackgroundCopyJob_SetPriority | notimpl |  | 7 | dlls/qmgr/job.c:942 | E_NOTIMPL |
| 128 | 63 | ntdll | NtQueryDirectoryFile | notimpl | vscode | 0 | dlls/ntdll/unix/file.c:3000 | Unsupported yet option |
| 129 | 63 | ntdll | NtQueryInformationFile | notimpl | vscode | 0 | dlls/ntdll/unix/file.c:4937 | Unsupported class (%d) |
| 130 | 63 | urlmon | CoInternetParseUrl | notimpl | firefox | 0 | dlls/urlmon/internet.c:384 | not supported action %d |
| 131 | 60 | bcp47langs | GetApplicationLanguages | spec-stub | firefox | 0 | dlls/bcp47langs/*.spec:0 |  |
| 132 | 60 | bcp47langs | GetFontFallbackLanguageList | spec-stub | firefox | 0 | dlls/bcp47langs/*.spec:0 |  |
| 133 | 60 | bcp47langs | LanguageListAsMuiForm | spec-stub | firefox | 0 | dlls/bcp47langs/*.spec:0 |  |
| 134 | 60 | coremessaging | CoreUICreate | spec-stub | firefox | 0 | dlls/coremessaging/*.spec:0 |  |
| 135 | 60 | esent | JetAttachDatabase2W | spec-stub | chrome | 0 | dlls/esent/*.spec:0 |  |
| 136 | 60 | esent | JetBeginSessionW | spec-stub | chrome | 0 | dlls/esent/*.spec:0 |  |
| 137 | 60 | esent | JetCloseTable | spec-stub | chrome | 0 | dlls/esent/*.spec:0 |  |
| 138 | 60 | esent | JetCreateInstanceW | spec-stub | chrome | 0 | dlls/esent/*.spec:0 |  |
| 139 | 60 | esent | JetGetTableColumnInfoW | spec-stub | chrome | 0 | dlls/esent/*.spec:0 |  |
| 140 | 60 | esent | JetInit | spec-stub | chrome | 0 | dlls/esent/*.spec:0 |  |
| 141 | 60 | esent | JetMove | spec-stub | chrome | 0 | dlls/esent/*.spec:0 |  |
| 142 | 60 | esent | JetOpenDatabaseW | spec-stub | chrome | 0 | dlls/esent/*.spec:0 |  |
| 143 | 60 | esent | JetOpenTableW | spec-stub | chrome | 0 | dlls/esent/*.spec:0 |  |
| 144 | 60 | esent | JetRetrieveColumn | spec-stub | chrome | 0 | dlls/esent/*.spec:0 |  |
| 145 | 60 | esent | JetSetSystemParameterW | spec-stub | chrome | 0 | dlls/esent/*.spec:0 |  |
| 146 | 60 | esent | JetTerm | spec-stub | chrome | 0 | dlls/esent/*.spec:0 |  |
| 147 | 60 | msvcp140 | ??0?$basic_streambuf@DU?$char_traits@D@std@@@std@@IEAA@AEBV01@@Z | spec-stub | audacity | 0 | dlls/msvcp140/*.spec:0 |  |
| 148 | 60 | msvcp140 | ?_ReportUnobservedException@details@Concurrency@@YAXXZ | spec-stub | audacity | 0 | dlls/msvcp140/*.spec:0 |  |
| 149 | 60 | ninput | BufferPointerPacketsInteractionContext | spec-stub | firefox | 0 | dlls/ninput/*.spec:0 |  |
| 150 | 60 | ninput | GetInteractionConfigurationInteractionContext | spec-stub | firefox | 0 | dlls/ninput/*.spec:0 |  |
| 151 | 60 | ninput | GetMouseWheelParameterInteractionContext | spec-stub | firefox | 0 | dlls/ninput/*.spec:0 |  |
| 152 | 60 | ninput | GetStateInteractionContext | spec-stub | firefox | 0 | dlls/ninput/*.spec:0 |  |
| 153 | 60 | ninput | ProcessBufferedPacketsInteractionContext | spec-stub | firefox | 0 | dlls/ninput/*.spec:0 |  |
| 154 | 60 | ninput | SetCrossSlideParametersInteractionContext | spec-stub | firefox | 0 | dlls/ninput/*.spec:0 |  |
| 155 | 60 | ninput | SetInertiaParameterInteractionContext | spec-stub | firefox | 0 | dlls/ninput/*.spec:0 |  |
| 156 | 60 | ninput | SetMouseWheelParameterInteractionContext | spec-stub | firefox | 0 | dlls/ninput/*.spec:0 |  |
| 157 | 60 | ninput | SetPivotInteractionContext | spec-stub | firefox | 0 | dlls/ninput/*.spec:0 |  |
| 158 | 60 | sspicli | QueryContextAttributesExW | spec-stub | pdn2 | 0 | dlls/sspicli/*.spec:0 |  |
| 159 | 60 | tbs | Tbsip_Submit_Command | spec-stub | chrome | 0 | dlls/tbs/*.spec:0 |  |
| 160 | 60 | tdh | TdhGetEventInformation | spec-stub | vscode | 0 | dlls/tdh/*.spec:0 |  |
| 161 | 60 | vcruntime140 | __GetPlatformExceptionInfo | spec-stub | Edge | 0 | dlls/vcruntime140/*.spec:0 |  |
| 162 | 56 | mscoree | corruntimehost_Stop | fixme-stub |  | 2 | dlls/mscoree/corruntimehost.c:614 | stub %p |
| 163 | 56 | mscoree | corruntimehost_UnloadDomain | fixme-stub |  | 2 | dlls/mscoree/corruntimehost.c:738 | stub %p |
| 164 | 56 | mscoree | parse_startup | fixme-stub |  | 2 | dlls/mscoree/config.c:324 | useLegacyV2RuntimeActivationPolicy=%s not implemented |
| 165 | 56 | msi | internal_ui_handler | fixme-stub |  | 2 | dlls/msi/package.c:1693 | internal UI not implemented for message 0x%08x (UI level = %x) |
| 166 | 54 | msctf | InputProcessorProfileMgr_GetActiveProfile | notimpl |  | 2 | dlls/msctf/inputprocessor.c:867 | (%p)->(%s %p) |
| 167 | 48 | combase | thread_context_info_QueryInterface | fixme-stub |  | 1 | dlls/combase/combase.c:2647 | interface not implemented %s |
| 168 | 48 | dnsapi | map_options | fixme-stub |  | 1 | dlls/dnsapi/libresolv.c:69 | option DNS_QUERY_DONT_RESET_TTL_VALUES not implemented |
| 169 | 48 | dxgi | dxgi_surface_GetDC | fixme-stub |  | 1 | dlls/dxgi/resource.c:203 | iface %p, discard %d, hdc %p semi-stub! |
| 170 | 48 | jscript | JScript_SetScriptState | fixme-stub |  | 1 | dlls/jscript/jscript.c:815 | unimplemented SCRIPTSTATE_INITIALIZED |
| 171 | 48 | msxml3 | ClassFactory_QueryInterface | fixme-stub |  | 1 | dlls/msxml3/factory.c:131 | interface %s not implemented |
| 172 | 48 | msxml3 | saxxmlreader_QueryInterface | fixme-stub |  | 1 | dlls/msxml3/saxreader.c:2877 | interface %s not implemented |
| 173 | 48 | shdocvw | IEParseDisplayNameWithBCW | fixme-stub |  | 1 | dlls/shdocvw/shdocvw_main.c:434 | stub: 0x%lx %s %p %p |
| 174 | 48 | shell32 | IQueryAssociations_fnGetString | fixme-stub |  | 1 | dlls/shell32/assoc.c:640 | %08lx: unimplemented flags |
| 175 | 48 | taskschd | read_actions | fixme-stub |  | 1 | dlls/taskschd/task.c:3173 | action %s is not implemented |
| 176 | 48 | taskschd | read_triggers | fixme-stub |  | 1 | dlls/taskschd/trigger.c:1234 | trigger %s is not implemented |
| 177 | 48 | uiautomationcore | msaa_provider_GetPropertyValue | fixme-stub |  | 1 | dlls/uiautomationcore/uia_provider.c:656 | Unimplemented propertyId %d |
| 178 | 48 | uiautomationcore | default_uia_provider_callback | fixme-stub |  | 1 | dlls/uiautomationcore/uia_client.c:3629 | Default ProviderType_NonClientArea provider unimplemented. |
| 179 | 48 | uiautomationcore | uia_get_providers_for_hwnd | fixme-stub |  | 1 | dlls/uiautomationcore/uia_client.c:3739 | Override provider callback currently unimplemented. |
| 180 | 48 | win32u | NtGdiDdDDIQueryAdapterInfo | fixme-stub |  | 1 | dlls/win32u/d3dkmt.c:334 | desc %p, type %d stub |
| 181 | 48 | win32u | NtUserSystemParametersInfo | fixme-stub |  | 1 | dlls/win32u/sysparams.c:5927 | Unimplemented action: %u (%s) |
| 182 | 48 | win32u | NtUserDisplayConfigGetDeviceInfo | fixme-stub |  | 1 | dlls/win32u/sysparams.c:7589 | DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_PREFERRED_MODE semi-stub. |
| 183 | 48 | windows.devices.enumeration | device_statics_FindAllAsyncAqsFilter | fixme-stub |  | 1 | dlls/windows.devices.enumeration/main.c:350 | iface %p, aqs %p, op %p stub! |
| 184 | 48 | windows.devices.enumeration | device_access_information_CurrentStatus | fixme-stub |  | 1 | dlls/windows.devices.enumeration/access.c:120 | iface %p, status %p stub. |
| 185 | 48 | windows.devices.enumeration | statics_CreateFromDeviceClass | fixme-stub |  | 1 | dlls/windows.devices.enumeration/access.c:265 | device_class %d, value %p stub. |
| 186 | 48 | windows.ui | uisettings_QueryInterface | fixme-stub |  | 1 | dlls/windows.ui/uisettings.c:81 | %s not implemented, returning E_NOINTERFACE. |
| 187 | 48 | wininet | query_global_option | fixme-stub |  | 1 | dlls/wininet/internet.c:3017 | INTERNET_OPTION_CONNECTED_STATE: semi-stub |
| 188 | 46 | shell32 | shellfolder_map_column_to_scid | notimpl |  | 1 | dlls/shell32/shlfolder.c:725 | missing property id for column %u. |
| 189 | 42 | appxdeploymentclient | package_manager_QueryInterface | fixme-stub |  | 4 | dlls/appxdeploymentclient/package.c:40 | %s not implemented, returning E_NOINTERFACE. |
| 190 | 42 | sti | stillimagew_GetDeviceList | fixme-stub |  | 4 | dlls/sti/sti.c:77 | (%p, %lu, 0x%lX, %p, %p): stub |
| 191 | 40 | activeds | ADsBuildEnumerator | fixme-stub |  | 0 | dlls/activeds/activeds_main.c:59 | (%p)->(%p)!stub |
| 192 | 40 | activeds | ADsFreeEnumerator | fixme-stub |  | 0 | dlls/activeds/activeds_main.c:68 | (%p)!stub |
| 193 | 40 | activeds | ADsEnumerateNext | fixme-stub |  | 0 | dlls/activeds/activeds_main.c:77 | (%p)->(%lu, %p, %p)!stub |
| 194 | 40 | activeds | ADsBuildVarArrayInt | fixme-stub |  | 0 | dlls/activeds/activeds_main.c:129 | (%p, %ld, %p)!stub |
| 195 | 40 | activeds | ADsSetLastError | fixme-stub |  | 0 | dlls/activeds/activeds_main.c:217 | (%ld,%p,%p)!stub |
| 196 | 40 | activeds | ADsGetLastError | fixme-stub |  | 0 | dlls/activeds/activeds_main.c:225 | (%p,%p,%ld,%p,%ld)!stub |
| 197 | 40 | activeds | ReallocADsStr | fixme-stub |  | 0 | dlls/activeds/activeds_main.c:278 | (%p,%p)!stub |
| 198 | 40 | activeds | ADsEncodeBinaryData | fixme-stub |  | 0 | dlls/activeds/activeds_main.c:287 | (%p,%ld,%p)!stub |
| 199 | 40 | activeds | path_QueryInterface | fixme-stub |  | 0 | dlls/activeds/pathname.c:50 | interface %s is not implemented |
| 200 | 40 | activeds | path_GetTypeInfoCount | fixme-stub |  | 0 | dlls/activeds/pathname.c:92 | %p,%p: stub |
| 201 | 40 | activeds | path_GetTypeInfo | fixme-stub |  | 0 | dlls/activeds/pathname.c:98 | %p,%u,%#lx,%p: stub |
| 202 | 40 | activeds | path_GetIDsOfNames | fixme-stub |  | 0 | dlls/activeds/pathname.c:104 | %p,%s,%p,%u,%lu,%p: stub |
| 203 | 40 | activeds | path_Invoke | fixme-stub |  | 0 | dlls/activeds/pathname.c:111 | %p,%ld,%s,%04lx,%04x,%p,%p,%p,%p: stub |
| 204 | 40 | activeds | path_Set | fixme-stub |  | 0 | dlls/activeds/pathname.c:183 | type %ld not implemented |
| 205 | 40 | activeds | path_SetDisplayType | fixme-stub |  | 0 | dlls/activeds/pathname.c:234 | %p,%ld: stub |
| 206 | 40 | activeds | path_Retrieve | fixme-stub |  | 0 | dlls/activeds/pathname.c:240 | type %ld not implemented |
| 207 | 40 | activeds | path_AddLeafElement | fixme-stub |  | 0 | dlls/activeds/pathname.c:355 | %p,%s: stub |
| 208 | 40 | activeds | path_RemoveLeafElement | fixme-stub |  | 0 | dlls/activeds/pathname.c:361 | %p: stub |
| 209 | 40 | activeds | path_CopyPath | fixme-stub |  | 0 | dlls/activeds/pathname.c:367 | %p,%p: stub |
| 210 | 40 | activeds | path_GetEscapedElement | fixme-stub |  | 0 | dlls/activeds/pathname.c:373 | %p,%ld,%s,%p: stub |
| 211 | 40 | activeds | path_get_EscapedMode | fixme-stub |  | 0 | dlls/activeds/pathname.c:379 | %p,%p: stub |
| 212 | 40 | activeds | path_put_EscapedMode | fixme-stub |  | 0 | dlls/activeds/pathname.c:385 | %p,%ld: stub |
| 213 | 40 | activeds | factory_QueryInterface | fixme-stub |  | 0 | dlls/activeds/pathname.c:454 | interface %s is not implemented |
| 214 | 40 | activeds | factory_LockServer | fixme-stub |  | 0 | dlls/activeds/pathname.c:510 | %p,%d: stub |
| 215 | 40 | activeds | DllGetClassObject | fixme-stub |  | 0 | dlls/activeds/pathname.c:543 | class %s/%s is not implemented |
| 216 | 40 | advapi32 | LsaCreateTrustedDomainEx | fixme-stub |  | 0 | dlls/advapi32/lsa.c:228 | (%p,%p,%p,0x%08lx,%p) stub |
| 217 | 40 | advapi32 | LsaDeleteTrustedDomain | fixme-stub |  | 0 | dlls/advapi32/lsa.c:244 | (%p,%p) stub |
| 218 | 40 | advapi32 | LsaEnumerateAccountRights | fixme-stub |  | 0 | dlls/advapi32/lsa.c:254 | (%p,%p,%p,%p) stub |
| 219 | 40 | advapi32 | LsaEnumerateAccounts | fixme-stub |  | 0 | dlls/advapi32/lsa.c:270 | (%p,%p,%p,%ld,%p) stub |
| 220 | 40 | advapi32 | LsaEnumerateAccountsWithUserRight | fixme-stub |  | 0 | dlls/advapi32/lsa.c:286 | (%p,%p,%p,%p) stub |
| 221 | 40 | advapi32 | LsaEnumerateTrustedDomains | fixme-stub |  | 0 | dlls/advapi32/lsa.c:318 | (%p,%p,%p,0x%08lx,%p) stub |
| 222 | 40 | advapi32 | LsaEnumerateTrustedDomainsEx | fixme-stub |  | 0 | dlls/advapi32/lsa.c:336 | (%p,%p,%p,0x%08lx,%p) stub |
| 223 | 40 | advapi32 | LsaLookupNames | fixme-stub |  | 0 | dlls/advapi32/lsa.c:386 | (%p,0x%08lx,%p,%p,%p) stub |
| 224 | 40 | advapi32 | LsaOpenTrustedDomainByName | fixme-stub |  | 0 | dlls/advapi32/lsa.c:780 | (%p,%p,0x%08lx,%p) stub |
| 225 | 40 | advapi32 | LsaQueryInformationPolicy | fixme-stub |  | 0 | dlls/advapi32/lsa.c:804 | category %d not implemented |
| 226 | 40 | advapi32 | LsaQueryTrustedDomainInfo | fixme-stub |  | 0 | dlls/advapi32/lsa.c:953 | (%p,%p,%d,%p) stub |
| 227 | 40 | advapi32 | LsaQueryTrustedDomainInfoByName | fixme-stub |  | 0 | dlls/advapi32/lsa.c:967 | (%p,%p,%d,%p) stub |
| 228 | 40 | advapi32 | LsaRegisterPolicyChangeNotification | fixme-stub |  | 0 | dlls/advapi32/lsa.c:981 | (%d,%p) stub |
| 229 | 40 | advapi32 | LsaRemoveAccountRights | fixme-stub |  | 0 | dlls/advapi32/lsa.c:993 | (%p,%p,%d,%p,0x%08lx) stub |
| 230 | 40 | advapi32 | LsaRetrievePrivateData | fixme-stub |  | 0 | dlls/advapi32/lsa.c:1018 | (%p,%p,%p) stub |
| 231 | 40 | advapi32 | LsaSetInformationPolicy | fixme-stub |  | 0 | dlls/advapi32/lsa.c:1041 | (%p,%s,%p) stub |
| 232 | 40 | advapi32 | LsaSetSecret | fixme-stub |  | 0 | dlls/advapi32/lsa.c:1065 | (%p,%p,%p) stub |
| 233 | 40 | advapi32 | LsaSetTrustedDomainInfoByName | fixme-stub |  | 0 | dlls/advapi32/lsa.c:1079 | (%p,%p,%d,%p) stub |
| 234 | 40 | advapi32 | LsaSetTrustedDomainInformation | fixme-stub |  | 0 | dlls/advapi32/lsa.c:1093 | (%p,%p,%d,%p) stub |
| 235 | 40 | advapi32 | LsaStorePrivateData | fixme-stub |  | 0 | dlls/advapi32/lsa.c:1117 | (%p,%p,%p) stub |
| 236 | 40 | advapi32 | LsaUnregisterPolicyChangeNotification | fixme-stub |  | 0 | dlls/advapi32/lsa.c:1130 | (%d,%p) stub |
| 237 | 40 | advapi32 | CredReadDomainCredentialsW | fixme-stub |  | 0 | dlls/advapi32/cred.c:1946 | (%p, 0x%lx, %p, %p) stub |
| 238 | 40 | advapi32 | CredMarshalCredentialW | fixme-stub |  | 0 | dlls/advapi32/cred.c:2198 | BinaryBlobCredential not implemented |
| 239 | 40 | advapi32 | CredUnmarshalCredentialW | fixme-stub |  | 0 | dlls/advapi32/cred.c:2328 | BinaryBlobCredential not implemented |
| 240 | 40 | advapi32 | CloseEncryptedFileRaw | fixme-stub |  | 0 | dlls/advapi32/crypt.c:338 | (%p): stub |
| 241 | 40 | advapi32 | OpenEncryptedFileRawA | fixme-stub |  | 0 | dlls/advapi32/crypt.c:2255 | (%s, %lx, %p): stub |
| 242 | 40 | advapi32 | OpenEncryptedFileRawW | fixme-stub |  | 0 | dlls/advapi32/crypt.c:2274 | (%s, %lx, %p): stub |
| 243 | 40 | advapi32 | ReadEncryptedFileRaw | fixme-stub |  | 0 | dlls/advapi32/crypt.c:2293 | (%p, %p, %p): stub |
| 244 | 40 | advapi32 | SystemFunction035 | fixme-stub |  | 0 | dlls/advapi32/crypt.c:2379 | %s: stub |
| 245 | 40 | advapi32 | WriteEncryptedFileRaw | fixme-stub |  | 0 | dlls/advapi32/crypt.c:2660 | (%p, %p, %p): stub |
| 246 | 40 | advapi32 | GetCurrentHwProfileA | fixme-stub |  | 0 | dlls/advapi32/advapi.c:117 | (%p) semi-stub |
| 247 | 40 | advapi32 | OpenThreadWaitChainSession | fixme-stub |  | 0 | dlls/advapi32/advapi.c:639 | flags %ld, callback %p stub! |
| 248 | 40 | advapi32 | GetThreadWaitChain | fixme-stub |  | 0 | dlls/advapi32/advapi.c:646 | handle %p, ctx %Ix, flags %ld, thread_id %ld, node_count %p, node_info_arr %p, is_cycle %p |
| 249 | 40 | advapi32 | PerfCloseQueryHandle | fixme-stub |  | 0 | dlls/advapi32/advapi.c:655 | query %p stub. |
| 250 | 40 | advapi32 | PerfAddCounters | fixme-stub |  | 0 | dlls/advapi32/advapi.c:672 | query %p, id %p, size %lu stub. |
| 251 | 40 | advapi32 | PerfQueryCounterData | fixme-stub |  | 0 | dlls/advapi32/advapi.c:682 | query %p, data %p, data_size %lu, size_needed %p stub. |
| 252 | 40 | advapi32 | WmiExecuteMethodA | fixme-stub |  | 0 | dlls/advapi32/wmi.c:41 |  %p %s %lu %lu %p %p %p: stub |
| 253 | 40 | advapi32 | WmiExecuteMethodW | fixme-stub |  | 0 | dlls/advapi32/wmi.c:52 | %p %s %lu %lu %p %p %p: stub |
| 254 | 40 | advapi32 | WmiFreeBuffer | fixme-stub |  | 0 | dlls/advapi32/wmi.c:63 | %p: stub |
| 255 | 40 | advapi32 | WmiMofEnumerateResourcesA | fixme-stub |  | 0 | dlls/advapi32/wmi.c:71 | %p %p %p: stub |
| 256 | 40 | advapi32 | WmiMofEnumerateResourcesW | fixme-stub |  | 0 | dlls/advapi32/wmi.c:80 | %p %p %p: stub |
| 257 | 40 | advapi32 | WmiNotificationRegistrationA | fixme-stub |  | 0 | dlls/advapi32/wmi.c:89 | %s %u %p 0x%Ix 0x%08lx: stub |
| 258 | 40 | advapi32 | WmiNotificationRegistrationW | fixme-stub |  | 0 | dlls/advapi32/wmi.c:99 | %s %u %p 0x%Ix 0x%08lx: stub |
| 259 | 40 | advapi32 | WmiOpenBlock | fixme-stub |  | 0 | dlls/advapi32/wmi.c:109 | %s %lu %p: stub |
| 260 | 40 | advapi32 | WmiQueryAllDataA | fixme-stub |  | 0 | dlls/advapi32/wmi.c:118 | %p %p %p: stub |
| 261 | 40 | advapi32 | WmiQueryAllDataW | fixme-stub |  | 0 | dlls/advapi32/wmi.c:127 | %p %p %p: stub |
| 262 | 40 | advapi32 | WmiQueryGuidInformation | fixme-stub |  | 0 | dlls/advapi32/wmi.c:136 | %p %p: stub |
| 263 | 40 | advapi32 | WmiSetSingleInstanceA | fixme-stub |  | 0 | dlls/advapi32/wmi.c:145 | %p %s %lu %lu %p: stub |
| 264 | 40 | advapi32 | WmiSetSingleInstanceW | fixme-stub |  | 0 | dlls/advapi32/wmi.c:155 | %p %s %lu %lu %p: stub |
| 265 | 40 | advapi32 | WmiSetSingleItemA | fixme-stub |  | 0 | dlls/advapi32/wmi.c:165 | %p %s %lu %lu %lu %p: stub |
| 266 | 40 | advapi32 | WmiSetSingleItemW | fixme-stub |  | 0 | dlls/advapi32/wmi.c:175 | %p %s %lu %lu %lu %p: stub |
| 267 | 40 | advapi32 | GetEffectiveRightsFromAclA | fixme-stub |  | 0 | dlls/advapi32/security.c:270 | %p %p %p - stub |
| 268 | 40 | advapi32 | BuildSecurityDescriptorA | fixme-stub |  | 0 | dlls/advapi32/security.c:329 | (%p,%p,%ld,%p,%ld,%p,%p,%p,%p) stub! |
| 269 | 40 | advapi32 | QueryWindows31FilesMigration | fixme-stub |  | 0 | dlls/advapi32/security.c:1229 | (%ld):stub |
| 270 | 40 | advapi32 | SynchronizeWindows31FilesAndWindowsNTRegistry | fixme-stub |  | 0 | dlls/advapi32/security.c:1245 | (0x%08lx,0x%08lx,0x%08lx,0x%08lx):stub |
| 271 | 40 | advapi32 | NotifyBootConfigStatus | fixme-stub |  | 0 | dlls/advapi32/security.c:1259 | (0x%08d):stub |
| 272 | 40 | advapi32 | AccessCheckAndAuditAlarmA | fixme-stub |  | 0 | dlls/advapi32/security.c:1716 | stub (%s,%p,%s,%s,%p,%08lx,%p,%x,%p,%p,%p) |
| 273 | 40 | advapi32 | ObjectCloseAuditAlarmA | fixme-stub |  | 0 | dlls/advapi32/security.c:1728 | stub (%s,%p,%x) |
| 274 | 40 | advapi32 | ObjectOpenAuditAlarmA | fixme-stub |  | 0 | dlls/advapi32/security.c:1735 | stub (%s,%p,%s,%s,%p,%p,0x%08lx,0x%08lx,%p,%x,%x,%p) |
| 275 | 40 | advapi32 | ObjectPrivilegeAuditAlarmA | fixme-stub |  | 0 | dlls/advapi32/security.c:1748 | stub (%s,%p,%p,0x%08lx,%p,%x) |
| 276 | 40 | advapi32 | PrivilegedServiceAuditAlarmA | fixme-stub |  | 0 | dlls/advapi32/security.c:1757 | stub (%s,%s,%p,%p,%x) |
| 277 | 40 | advapi32 | GetSecurityInfoExA | fixme-stub |  | 0 | dlls/advapi32/security.c:1926 | stub! |
| 278 | 40 | advapi32 | GetSecurityInfoExW | fixme-stub |  | 0 | dlls/advapi32/security.c:1940 | stub! |
| 279 | 40 | advapi32 | trustee_to_sid | fixme-stub |  | 0 | dlls/advapi32/security.c:2353 | TRUSTEE_IS_OBJECTS_AND_SID unimplemented |
| 280 | 40 | advapi32 | GetNamedSecurityInfoExW | fixme-stub |  | 0 | dlls/advapi32/security.c:3439 | (%s, %d, %ld, %s, %s, %p, %p, %p, %p) stub |
| 281 | 40 | advapi32 | GetNamedSecurityInfoExA | fixme-stub |  | 0 | dlls/advapi32/security.c:3451 | (%s, %d, %ld, %s, %s, %p, %p, %p, %p) stub |
| 282 | 40 | advapi32 | DecryptFileA | fixme-stub |  | 0 | dlls/advapi32/security.c:3472 | (%s, %08lx): stub |
| 283 | 40 | advapi32 | EncryptFileW | fixme-stub |  | 0 | dlls/advapi32/security.c:3481 | (%s): stub |
| 284 | 40 | advapi32 | EncryptFileA | fixme-stub |  | 0 | dlls/advapi32/security.c:3490 | (%s): stub |
| 285 | 40 | advapi32 | FileEncryptionStatusW | fixme-stub |  | 0 | dlls/advapi32/security.c:3499 | (%s %p): stub |
| 286 | 40 | advapi32 | FileEncryptionStatusA | fixme-stub |  | 0 | dlls/advapi32/security.c:3511 | (%s %p): stub |
| 287 | 40 | advapi32 | SaferSetLevelInformation | fixme-stub |  | 0 | dlls/advapi32/security.c:4423 | (%p %u %p %lu) stub |
| 288 | 40 | advapi32 | LookupSecurityDescriptorPartsA | fixme-stub |  | 0 | dlls/advapi32/security.c:4433 | (%p %p %p %p %p %p %p) stub |
| 289 | 40 | advapi32 | LookupSecurityDescriptorPartsW | fixme-stub |  | 0 | dlls/advapi32/security.c:4445 | (%p %p %p %p %p %p %p) stub |
| 290 | 40 | advapi32 | RegQueryReflectionKey | fixme-stub |  | 0 | dlls/advapi32/registry.c:367 | %p, %p stub |
| 291 | 40 | advapi32 | RegReplaceKeyW | fixme-stub |  | 0 | dlls/advapi32/registry.c:565 | (%p,%s,%s,%s): stub |
| 292 | 40 | advapi32 | RegEnableReflectionKey | fixme-stub |  | 0 | dlls/advapi32/registry.c:733 | %p: stub |
| 293 | 40 | advapi32 | RegDisableReflectionKey | fixme-stub |  | 0 | dlls/advapi32/registry.c:744 | %p: stub |
| 294 | 40 | apphelp | ApphelpCheckInstallShieldPackage | fixme-stub |  | 0 | dlls/apphelp/apphelp.c:42 | stub: %p %s |
| 295 | 40 | apphelp | ApphelpCheckMsiPackage | fixme-stub |  | 0 | dlls/apphelp/apphelp.c:48 | stub: %p %s |
| 296 | 40 | apphelp | SdbCreateDatabase | fixme-stub |  | 0 | dlls/apphelp/apphelp.c:54 | stub: %s %u |
| 297 | 40 | apphelp | ShimFlushCache | fixme-stub |  | 0 | dlls/apphelp/apphelp.c:68 | stub: %p %p %s %u |
| 298 | 40 | apphelp | SdbInitDatabase | fixme-stub |  | 0 | dlls/apphelp/apphelp.c:75 | stub: %08lx %s |
| 299 | 40 | apphelp | SdbOpenDatabase | fixme-stub |  | 0 | dlls/apphelp/apphelp.c:82 | stub: %s %08x |
| 300 | 40 | apphelp | SdbGetFirstChild | fixme-stub |  | 0 | dlls/apphelp/apphelp.c:89 | stub: %p %ld |

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
| msvcp110 | 431 | 0 | 0 | 0 | 431 |
| dbgeng | 430 | 377 | 52 | 0 | 1 |
| shell32 | 417 | 216 | 137 | 8 | 56 |
| msvcr110 | 407 | 0 | 0 | 0 | 407 |
| msvcr120 | 407 | 0 | 0 | 0 | 407 |
| netio.sys | 402 | 10 | 5 | 0 | 387 |
| msvcp120 | 396 | 0 | 0 | 0 | 396 |
| msvcp120_app | 396 | 0 | 0 | 0 | 396 |
| ieframe | 389 | 16 | 360 | 13 | 0 |
| msvcp_win | 388 | 0 | 0 | 0 | 388 |
| msvcp140 | 366 | 0 | 0 | 0 | 366 |
| msvcr120_app | 342 | 0 | 0 | 0 | 342 |
| esent | 336 | 0 | 0 | 0 | 336 |
| msvcr100 | 316 | 0 | 0 | 0 | 316 |
| msvcm80 | 302 | 6 | 0 | 0 | 296 |
| msvcp100 | 280 | 0 | 0 | 0 | 280 |
| rpcrt4 | 279 | 62 | 2 | 1 | 214 |
| ndis.sys | 274 | 3 | 0 | 0 | 271 |
| netapi32 | 268 | 47 | 0 | 0 | 221 |
| msxml3 | 264 | 106 | 154 | 4 | 0 |
| msasn1 | 264 | 5 | 0 | 0 | 259 |
| concrt140 | 249 | 4 | 0 | 0 | 245 |
| ntdll | 245 | 115 | 23 | 5 | 102 |
| setupx.dll16 | 240 | 18 | 0 | 0 | 222 |
| setupapi | 235 | 33 | 5 | 6 | 191 |
| d3dx9_36 | 217 | 133 | 12 | 0 | 72 |
| inetcomm | 205 | 71 | 41 | 7 | 86 |
| vssapi | 201 | 0 | 1 | 3 | 197 |
| ucrtbase | 196 | 0 | 0 | 0 | 196 |
| adsldpc | 189 | 0 | 0 | 0 | 189 |
| combase | 179 | 23 | 10 | 3 | 143 |
| olecli.dll16 | 175 | 10 | 0 | 0 | 165 |
| msvcm90 | 171 | 0 | 0 | 0 | 171 |
| kernel32 | 169 | 69 | 0 | 3 | 97 |
| apphelp | 167 | 12 | 0 | 0 | 155 |
| twinapi.appcore | 164 | 44 | 0 | 0 | 120 |
| fltmgr.sys | 160 | 7 | 0 | 0 | 153 |
| wdscore | 159 | 0 | 0 | 0 | 159 |
| d3drm | 159 | 152 | 7 | 0 | 0 |
| msvcr80 | 157 | 0 | 0 | 0 | 157 |
| webservices | 154 | 9 | 96 | 0 | 49 |
| d3d11 | 150 | 106 | 10 | 1 | 33 |
| compobj.dll16 | 150 | 4 | 0 | 0 | 146 |
| mscoree | 149 | 60 | 35 | 0 | 54 |
| fwpuclnt | 146 | 9 | 0 | 0 | 137 |
| msvcr90 | 145 | 0 | 0 | 0 | 145 |
| user.exe16 | 142 | 62 | 0 | 2 | 78 |
| riched20 | 141 | 107 | 34 | 0 | 0 |
| gdi32 | 140 | 26 | 1 | 46 | 67 |
| urlmon | 138 | 31 | 83 | 1 | 23 |
| dx8vb | 136 | 1 | 0 | 0 | 135 |
| pdh | 135 | 13 | 0 | 3 | 119 |
| gdi.exe16 | 135 | 20 | 0 | 5 | 110 |
| ole32 | 133 | 61 | 35 | 5 | 32 |
| mprapi | 132 | 3 | 0 | 0 | 129 |
| advapi32 | 129 | 83 | 3 | 3 | 40 |
| kernelbase | 127 | 51 | 6 | 4 | 66 |
| krnl386.exe16 | 125 | 43 | 0 | 0 | 82 |
| wininet | 122 | 77 | 0 | 3 | 42 |
| dhtmled.ocx | 122 | 73 | 49 | 0 | 0 |
| shlwapi | 116 | 13 | 2 | 0 | 101 |
| sechost | 115 | 5 | 0 | 0 | 110 |
| clusapi | 115 | 7 | 0 | 0 | 108 |
| mapi32 | 114 | 18 | 0 | 1 | 95 |
| rasapi32 | 114 | 46 | 0 | 0 | 68 |
| spoolss | 114 | 8 | 0 | 0 | 106 |
| shcore | 112 | 3 | 6 | 0 | 103 |
| shdocvw | 108 | 12 | 2 | 0 | 94 |
| ole2disp.dll16 | 108 | 5 | 0 | 0 | 103 |
| windowscodecs | 107 | 78 | 24 | 0 | 5 |
| appxdeploymentclient | 106 | 22 | 6 | 1 | 77 |
| msctf | 105 | 65 | 21 | 0 | 19 |
| ksecdd.sys | 104 | 0 | 0 | 0 | 104 |
| dnsapi | 100 | 9 | 0 | 0 | 91 |
| msado15 | 100 | 15 | 80 | 5 | 0 |
| mapistub | 99 | 0 | 0 | 0 | 99 |
| mscorwks | 97 | 0 | 0 | 0 | 97 |
| wuapi | 95 | 8 | 81 | 6 | 0 |
| winsta | 95 | 1 | 7 | 0 | 87 |
| ntdsapi | 92 | 6 | 0 | 0 | 86 |
| ole2.dll16 | 92 | 13 | 0 | 0 | 79 |
| hnetcfg | 91 | 21 | 61 | 9 | 0 |
| w32skrnl | 91 | 0 | 0 | 0 | 91 |
| msi | 90 | 20 | 16 | 25 | 29 |
| explorer | 89 | 8 | 81 | 0 | 0 |
| cfgmgr32 | 89 | 0 | 0 | 0 | 89 |
| msdaps | 89 | 78 | 11 | 0 | 0 |
| bluetoothapis | 88 | 6 | 0 | 0 | 82 |
| msdrm | 86 | 1 | 0 | 0 | 85 |
| msvcp140_2 | 84 | 0 | 0 | 0 | 84 |
| winegstreamer | 83 | 6 | 77 | 0 | 0 |
| mf | 82 | 2 | 27 | 0 | 53 |
| dbghelp | 82 | 21 | 0 | 2 | 59 |
| iphlpapi | 81 | 20 | 0 | 0 | 61 |
| uiautomationcore | 81 | 37 | 13 | 0 | 31 |
| hal | 81 | 10 | 0 | 0 | 71 |
| msls31 | 79 | 0 | 0 | 0 | 79 |
| atmlib | 76 | 2 | 0 | 0 | 74 |
| user32 | 75 | 47 | 1 | 2 | 25 |
| d3dx10_43 | 75 | 45 | 4 | 0 | 26 |
| wer | 73 | 5 | 0 | 0 | 68 |
| winspool.drv | 72 | 16 | 4 | 6 | 46 |
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
| windows.applicationmodel | 71 | 69 | 2 | 0 | 0 |
| resutils | 71 | 0 | 0 | 0 | 71 |
| wiaservc | 70 | 16 | 0 | 0 | 54 |
| d3dx9_31 | 70 | 0 | 0 | 0 | 70 |
| ncrypt | 69 | 1 | 0 | 0 | 68 |
| localspl | 69 | 6 | 2 | 0 | 61 |
| mscms | 69 | 23 | 5 | 0 | 41 |
| regapi | 69 | 0 | 0 | 0 | 69 |
| wbemdisp | 68 | 5 | 62 | 1 | 0 |
| d3dx9_25 | 68 | 0 | 0 | 0 | 68 |
| oleaut32 | 67 | 32 | 6 | 2 | 27 |
| d3dx9_42 | 67 | 0 | 0 | 0 | 67 |
| d3dx9_43 | 67 | 0 | 0 | 0 | 67 |
| crypt32 | 66 | 55 | 2 | 0 | 9 |
| imm32 | 66 | 21 | 0 | 0 | 45 |
| qcap | 65 | 33 | 32 | 0 | 0 |
| d3dx9_24 | 65 | 0 | 0 | 0 | 65 |
| samlib | 65 | 0 | 0 | 0 | 65 |
| jscript | 63 | 28 | 35 | 0 | 0 |
| gdiplus | 63 | 50 | 0 | 0 | 13 |
| atl | 61 | 32 | 20 | 6 | 3 |
| npptools | 60 | 0 | 0 | 0 | 60 |
| wintrust | 59 | 27 | 0 | 2 | 30 |
| mfplat | 59 | 1 | 7 | 0 | 51 |
| msvcrt | 57 | 33 | 1 | 0 | 23 |
| odbc32 | 57 | 0 | 0 | 0 | 57 |
| windows.web | 56 | 45 | 11 | 0 | 0 |
| wpcap | 56 | 3 | 0 | 1 | 52 |
| d3d8thk | 56 | 0 | 0 | 0 | 56 |
| tdi.sys | 56 | 0 | 0 | 0 | 56 |
| msscript.ocx | 55 | 3 | 47 | 4 | 1 |
| rtutils | 55 | 1 | 0 | 0 | 54 |
| itss | 55 | 0 | 54 | 0 | 1 |
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
| mpr | 47 | 34 | 0 | 0 | 13 |
| sspicli | 47 | 0 | 0 | 0 | 47 |
| display.drv16 | 47 | 2 | 0 | 0 | 45 |
| ddraw | 46 | 27 | 3 | 0 | 16 |
| dmime | 46 | 1 | 45 | 0 | 0 |
| olecli32 | 45 | 4 | 0 | 0 | 41 |
| scrobj | 45 | 14 | 30 | 1 | 0 |
| dwmapi | 44 | 8 | 0 | 0 | 36 |
| coremessaging | 44 | 5 | 11 | 0 | 28 |
| windows.ui | 44 | 39 | 3 | 0 | 2 |
| wsnmp32 | 43 | 0 | 0 | 0 | 43 |
| authz | 42 | 6 | 1 | 0 | 35 |
| diasymreader | 42 | 0 | 5 | 32 | 5 |
| win32s16.dll16 | 42 | 1 | 0 | 0 | 41 |
| wintab.dll16 | 42 | 42 | 0 | 0 | 0 |
| dcomp | 41 | 16 | 13 | 1 | 11 |
| gphoto2.ds | 41 | 41 | 0 | 0 | 0 |
| opengl32 | 41 | 30 | 11 | 0 | 0 |
| ktmw32 | 41 | 0 | 0 | 0 | 41 |
| dwrite | 40 | 17 | 21 | 2 | 0 |
| wow64 | 40 | 13 | 8 | 0 | 19 |
| wimgapi | 40 | 3 | 0 | 0 | 37 |
| dhcpcsvc | 40 | 0 | 0 | 0 | 40 |
| query | 39 | 0 | 1 | 0 | 38 |
| irprops.cpl | 39 | 0 | 0 | 0 | 39 |
| bcrypt | 38 | 3 | 28 | 0 | 7 |
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
| d2d1 | 35 | 30 | 4 | 0 | 1 |
| sppc | 35 | 4 | 0 | 0 | 31 |
| winscard | 35 | 3 | 0 | 0 | 32 |
| wmasf | 35 | 0 | 0 | 0 | 35 |
| tdh | 34 | 2 | 0 | 0 | 32 |
| activeds | 33 | 25 | 0 | 0 | 8 |
| propsys | 33 | 6 | 6 | 1 | 20 |
| services | 33 | 2 | 31 | 0 | 0 |
| msvcrtd | 33 | 0 | 0 | 0 | 33 |
| inseng | 32 | 1 | 24 | 1 | 6 |
| ntprint | 32 | 0 | 0 | 0 | 32 |
| d3d9 | 31 | 27 | 1 | 0 | 3 |
| taskschd | 31 | 30 | 0 | 0 | 1 |
| windows.storage | 31 | 29 | 2 | 0 | 0 |
| hhctrl.ocx | 31 | 1 | 30 | 0 | 0 |
| oledb32 | 31 | 20 | 11 | 0 | 0 |
| secur32 | 30 | 16 | 4 | 0 | 10 |
| windows.networking.connectivity | 30 | 25 | 0 | 0 | 5 |
| d3d10 | 30 | 25 | 1 | 0 | 4 |
| mstask | 30 | 14 | 2 | 0 | 14 |
| fltlib | 29 | 5 | 0 | 0 | 24 |
| chakra | 29 | 0 | 0 | 0 | 29 |
| cng.sys | 29 | 0 | 0 | 0 | 29 |
| comdlg32 | 28 | 7 | 17 | 0 | 4 |
| dpvoice | 28 | 5 | 23 | 0 | 0 |
| wldp | 28 | 0 | 0 | 2 | 26 |
| drmclien | 28 | 0 | 0 | 0 | 28 |
| odbcbcp | 28 | 0 | 0 | 0 | 28 |
| winhttp | 27 | 15 | 11 | 1 | 0 |
| cryptext | 27 | 2 | 0 | 0 | 25 |
| mssign32 | 27 | 5 | 0 | 0 | 22 |
| windows.security.authentication.onlineid | 26 | 20 | 6 | 0 | 0 |
| msvcr70 | 26 | 0 | 0 | 0 | 26 |
| mswsock | 26 | 0 | 0 | 0 | 26 |
| msvcr71 | 26 | 0 | 0 | 0 | 26 |
| winnls.dll16 | 26 | 0 | 0 | 0 | 26 |
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
| dmusic | 24 | 0 | 24 | 0 | 0 |
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
| wintypes | 20 | 14 | 0 | 1 | 5 |
| rtworkq | 20 | 0 | 17 | 0 | 3 |
| svrapi | 20 | 0 | 0 | 0 | 20 |
| quartz | 20 | 0 | 19 | 0 | 1 |
| vdmdbg | 19 | 2 | 0 | 0 | 17 |
| davclnt | 19 | 0 | 0 | 0 | 19 |
| dmstyle | 19 | 0 | 18 | 1 | 0 |
| oledlg | 18 | 18 | 0 | 0 | 0 |
| winevulkan | 18 | 8 | 0 | 0 | 10 |
| dciman32 | 18 | 0 | 1 | 0 | 17 |
| d3dim700 | 18 | 0 | 0 | 0 | 18 |
| windows.perception.stub | 17 | 14 | 1 | 0 | 2 |
| msimtf | 17 | 14 | 0 | 1 | 2 |
| t2embed | 17 | 3 | 0 | 0 | 14 |
| netutils | 17 | 0 | 0 | 0 | 17 |
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
| wtsapi32 | 15 | 14 | 1 | 0 | 0 |
| nsi | 15 | 0 | 0 | 0 | 15 |
| prntvpt | 15 | 1 | 0 | 0 | 14 |
| sfc_os | 15 | 3 | 0 | 1 | 11 |
| xmllite | 15 | 8 | 7 | 0 | 0 |
| mmdevapi | 14 | 1 | 11 | 2 | 0 |
| browseui | 14 | 13 | 1 | 0 | 0 |
| connect | 14 | 1 | 0 | 0 | 13 |
| qwave | 14 | 3 | 0 | 0 | 11 |
| wscript | 14 | 1 | 13 | 0 | 0 |
| olethk32 | 14 | 0 | 0 | 0 | 14 |
| mlang | 13 | 4 | 6 | 2 | 1 |
| websocket | 13 | 3 | 0 | 0 | 10 |
| evr | 13 | 0 | 11 | 0 | 2 |
| d3dim | 13 | 0 | 0 | 0 | 13 |
| nsiproxy.sys | 13 | 12 | 1 | 0 | 0 |
| toolhelp.dll16 | 13 | 8 | 0 | 0 | 5 |
| rstrtmgr | 12 | 8 | 0 | 0 | 4 |
| cryptdlg | 12 | 1 | 1 | 0 | 10 |
| difxapi | 12 | 12 | 0 | 0 | 0 |
| faultrep | 12 | 1 | 0 | 0 | 11 |
| winhlp32 | 12 | 2 | 0 | 10 | 0 |
| pwrshplugin | 12 | 0 | 0 | 0 | 12 |
| wuaueng | 12 | 0 | 0 | 0 | 12 |
| hidparse.sys | 12 | 1 | 0 | 0 | 11 |
| dmsynth | 12 | 4 | 8 | 0 | 0 |
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
| amstream | 11 | 0 | 11 | 0 | 0 |
| oleacc | 10 | 1 | 8 | 1 | 0 |
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
| wmp | 10 | 0 | 10 | 0 | 0 |
| msmpeg2vdec | 10 | 0 | 0 | 0 | 10 |
| vulkan-1 | 10 | 0 | 0 | 0 | 10 |
| ws2_32 | 9 | 7 | 0 | 0 | 2 |
| comctl32 | 9 | 4 | 0 | 1 | 4 |
| sxs | 9 | 0 | 9 | 0 | 0 |
| dsquery | 9 | 4 | 0 | 1 | 4 |
| iertutil | 9 | 9 | 0 | 0 | 0 |
| kerberos | 9 | 1 | 0 | 0 | 8 |
| mgmtapi | 9 | 2 | 0 | 0 | 7 |
| wmvcore | 9 | 2 | 1 | 0 | 6 |
| dmcompos | 9 | 0 | 9 | 0 | 0 |
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
| wbemprox | 8 | 7 | 1 | 0 | 0 |
| windows.devices.bluetooth | 8 | 8 | 0 | 0 | 0 |
| mfreadwrite | 8 | 0 | 8 | 0 | 0 |
| dpwsockx | 8 | 6 | 0 | 0 | 2 |
| msdelta | 8 | 1 | 0 | 0 | 7 |
| msident | 8 | 0 | 7 | 1 | 0 |
| amsi | 8 | 0 | 0 | 4 | 4 |
| atl80 | 8 | 0 | 0 | 0 | 8 |
| wofutil | 8 | 0 | 0 | 0 | 8 |
| dmscript | 8 | 1 | 7 | 0 | 0 |
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
| dxgi | 6 | 5 | 1 | 0 | 0 |
| windows.system.profile.systemmanufacturers | 6 | 6 | 0 | 0 | 0 |
| windows.gaming.input | 6 | 0 | 6 | 0 | 0 |
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
| dinput | 5 | 5 | 0 | 0 | 0 |
| explorerframe | 5 | 3 | 2 | 0 | 0 |
| graphicscapture | 5 | 5 | 0 | 0 | 0 |
| hrtfapo | 5 | 1 | 0 | 0 | 4 |
| mciqtz32 | 5 | 5 | 0 | 0 | 0 |
| winemac.drv | 5 | 4 | 1 | 0 | 0 |
| mfmediaengine | 5 | 0 | 4 | 0 | 1 |
| updspapi | 5 | 0 | 0 | 1 | 4 |
| dssenh | 5 | 0 | 0 | 0 | 5 |
| mscat32 | 5 | 0 | 0 | 0 | 5 |
| scarddlg | 5 | 0 | 0 | 0 | 5 |
| strmdll | 5 | 0 | 0 | 0 | 5 |
| winnls32 | 5 | 0 | 0 | 0 | 5 |
| wmilib.sys | 5 | 0 | 0 | 0 | 5 |
| stress.dll16 | 5 | 2 | 0 | 0 | 3 |
| twain.dll16 | 5 | 1 | 0 | 0 | 4 |
| windows.media.mediacontrol | 4 | 3 | 1 | 0 | 0 |
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
| msvfw32 | 4 | 2 | 0 | 0 | 2 |
| wshom.ocx | 3 | 3 | 0 | 0 | 0 |
| cryptbase | 3 | 0 | 0 | 0 | 3 |
| mfsrcsnk | 3 | 2 | 1 | 0 | 0 |
| nddeapi | 3 | 1 | 0 | 0 | 2 |
| tapi32 | 3 | 1 | 0 | 0 | 2 |
| uianimation | 3 | 2 | 0 | 1 | 0 |
| wineoss.drv | 3 | 2 | 1 | 0 | 0 |
| wldap32 | 3 | 2 | 0 | 0 | 1 |
| wnaspi32 | 3 | 1 | 0 | 1 | 1 |
| dllhost | 3 | 3 | 0 | 0 | 0 |
| klist | 3 | 3 | 0 | 0 | 0 |
| wusa | 3 | 2 | 0 | 1 | 0 |
| infosoft | 3 | 0 | 1 | 2 | 0 |
| mfplay | 3 | 0 | 2 | 0 | 1 |
| sapi | 3 | 0 | 3 | 0 | 0 |
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
| dmband | 3 | 0 | 3 | 0 | 0 |
| qedit | 3 | 0 | 3 | 0 | 0 |
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
| powrprof | 1 | 1 | 0 | 0 | 0 |
| d3d10core | 1 | 0 | 1 | 0 | 0 |
| windows.media | 1 | 0 | 1 | 0 | 0 |
| windows.media.devices | 1 | 0 | 1 | 0 | 0 |
| windows.media.speech | 1 | 0 | 1 | 0 | 0 |
| powershell | 1 | 1 | 0 | 0 | 0 |
| cdosys | 1 | 1 | 0 | 0 | 0 |
| credui | 1 | 0 | 0 | 0 | 1 |
| dpnhupnp | 1 | 1 | 0 | 0 | 0 |
| iccvid | 1 | 1 | 0 | 0 | 0 |
| ir50_32 | 1 | 1 | 0 | 0 | 0 |
| mciwave | 1 | 1 | 0 | 0 | 0 |
| msimg32 | 1 | 1 | 0 | 0 | 0 |
| msnet32 | 1 | 1 | 0 | 0 | 0 |
| msrle32 | 1 | 1 | 0 | 0 | 0 |
| olepro32 | 1 | 1 | 0 | 0 | 0 |
| qdvd | 1 | 1 | 0 | 0 | 0 |
| windowscodecsext | 1 | 1 | 0 | 0 | 0 |
| winealsa.drv | 1 | 1 | 0 | 0 | 0 |
| winmm | 1 | 0 | 0 | 0 | 1 |
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
| dplayx | 1 | 1 | 0 | 0 | 0 |
| ctl3d.dll16 | 1 | 0 | 0 | 0 | 1 |
| ctl3dv2.dll16 | 1 | 0 | 0 | 0 | 1 |
| ole2prox.dll16 | 1 | 0 | 0 | 0 | 1 |
| system.drv16 | 1 | 0 | 0 | 0 | 1 |
| ver.dll16 | 1 | 0 | 0 | 0 | 1 |
