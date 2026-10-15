# Decisions, round 2 (coordinator, 2026-10-14)

Source: the escalation register (1209 open escalations in 112 decision
groups, docs/stubs/ESCALATIONS.md; the machine;
group ids below are its ids). David: "We duplicate Windows as we can" and
"max compat with windows software". DECISIONS.md items 1-10 still stand.

## A. Implement now (the scope the register names)
MSH-CSSOM (toJSON, responseBody, window.frames, OmHistory_go), K-MISC
(resource/processor/working-set items), CRT (_heapmin, abnormal_termination),
RDS (WTSEnumerateProcessesExW, WTSWaitSystemEvent), M-MMCSS, SEC-CRED
(CredMarshal/Unmarshal, CredReadDomainCredentials), DEVICES (tdh property
functions), K-MM (Probe*, KeQueryPriorityThread, MmCopyVirtualMemory),
COM-DCOM (CoGetCallerTID, message-filter retry, CoRegisterSurrogate), N-MISC
(sensapi, connect, DisconnectEx), P-STUBPROG (subst, regini, extrac32 /A,
printui verbs), SYS-SETINFO, P-MISCINST, PRN (SetJob/SetPrinter; no real
printers in tests), COM-WSC (merge fast-a 3080/3096, re-triage), SH-INPUT
(keyboard layouts), M-CAPTURE (V4L2 video capture), K-CONALIAS, K-SETUPAPI
(local names, eject), G-D3D11OTHER (ReadFromSubresource, MinLOD), G-DINPUT
(uianimation, HID translate), W-OTHER (HttpContent.Headers,
TransportInformation), SH-MISC, RPC-MGMT, K-IRQL, G-DCOMP, G-DWRITE
(justification), K-PERF, G-X11 (ConfigureNotify), P-CAB, CLOUD, G-DDRAW,
SH-ADESK (the two dialogs), SYS-INPUT, SH-DEFAPP.

## B. Security groups (DECISIONS item 9) -- approved subsets
- SEC-ACL: approved now (A->W forms, AccessCheckByType fix, object ACEs,
  SetAclInformation, private object security). Mutants must cover wrong
  GRANT.
- SEC-AUDIT: approved now for the SeAuditPrivilege check and the documented
  outputs; event emission stays escalated.
- SEC-LOGON: approved now for LsaEnumerateLogonSessions /
  LsaGetLogonSessionData (read-only, apps such as VS Code call them).
- SEC-CRYPT32: approved now for CertRegister/UnregisterPhysicalStore and
  streamed messages.
- SEC-CRYPTOPOL: approved now for reading the FIPS setting.
- Everything else in SEC-*, K-OBSEC, RPC-SEC stays escalated (one security
  round later, with the token protocol change bundled).

## C. Implement next (phase 2, in this order; high compatibility value)
1. G-SHARED (cross-device shared resources: Chromium/Electron/OBS).
2. RPC-NDR (pipes and async stubs: Edge/WebView).
3. M-CAPTURE leftovers, G-VIDDEC (VA-API backend), W-MEDIA.
4. NET-META + NET-HOST (metadata dispenser, StrongName; unlocks regasm,
   regsvcs, tlbimp).
5. K-CALLBACKS (process/thread notify first: anti-cheat and AV drivers).
6. UIA, SH-DIALOGS, SH-PROPSTORE (screen readers, Explorer dialogs).
7. G-WINRTCOMP (WinUI/XAML apps), G-D2D, G-DWRITE rest, G-WIC.
8. RE-TOM, RE-OLE, COM-OLELINK (WordPad and Office interop), XML-PARSER
   (SOM/entities), MSH-LEGACY (control range, markup pointers, display
   services only).
9. The rest of "implement later" in the register, by item count.

## D. Won't do (keep the documented error answers)
K-TXF (deprecated by Microsoft; but Commit/Rollback get real handles and
errors), G-DPLAY (DirectPlay Voice/NAT), G-GL32 (unreachable), SEC-PSTORE
(removed in Vista), DBG-REMOTE (undocumented protocol), COM-MAIL CDO/MAPI
profile admin (MimeSecurity goes to phase 2), G-PROTECT (DRM), N-RRAS
(server role), G-DMUSIC composition engines, K-PORTIO (unless a hardware
tool needs it, admin-gated), SEC-EFS raw backup, G-TILED (for now),
G-WINED3D-VK (DXVK ships), SEC-EDP.
