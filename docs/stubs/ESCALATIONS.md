# Escalation decision register (stubs program)

Generated 2026-10-14 by Fast B from the triage files named in the brief. Nothing in the tsv files was changed. Same dll+function in several files: an implemented verdict anywhere removes it from this register; otherwise it is listed once with every source. No round-3 branches (stubs-5f, stubs-6f, stubs-7c) exist on origin, so none were applied. The ESCALATE.md / escalate-*.md files were read for proposals: their text is quoted inside the tsv evidence columns and is merged into each group below.

## 1. Summary

**Open escalations: 1209** (unique dll+function; 112 decision groups).

| Subsystem | Open items |
|---|---|
| graphics/D3D | 230 |
| shell/Explorer | 38 |
| COM/RPC | 84 |
| networking | 67 |
| security/crypto | 193 |
| printing | 12 |
| media | 78 |
| kernel/drivers | 175 |
| .NET | 43 |
| WinRT | 18 |
| installers | 39 |
| other | 232 |
| **Total** | **1209** |

Notes: 'other' holds MSHTML (58), RichEdit (61), MSXML/ADO, dbgeng, UI Automation, C runtime and similar. Items whose group spans two subsystems are counted under the group's primary subsystem. Of the total, 24 items are escalated in one file but carry a different verdict (windows-matches / not-applicable / no-ground-truth) in another; they are kept here and flagged in part 3 so the coordinator can confirm.

Already-decided items still showing as escalated: cldapi (DECISIONS 1), dcomp surfaces (2), shell32 SetAppAsDefault (5), sti/wiaservc (4). Security-model items (DECISIONS 9) stay escalated; each group row below carries a concrete Windows-matching proposal.

### Top 15 groups by item count

| # | Group | Items | Rec |
|---|---|---|---|
| 1 | G-SHARED: Cross-device shared resources: D3DKMT allocations/keyed mutex/sync objects, D3D1 | 42 | implement later |
| 2 | RE-TOM: RichEdit Text Object Model (TOM/TOM2): units, formatted text, SetPoint, properti | 37 | implement later (accessibility/automation, WordPad) |
| 3 | MSH-LEGACY: MSHTML legacy IE DHTML (behaviors/HTC, filters, expressions, recordset binding,  | 35 | implement later for editing/markup APIs (Outlook, old intranet apps) |
| 4 | K-TXF: Transactions: TxF *Transacted file APIs and KTM (NtCreateTransaction, transacted | 33 | won't do (deprecated) |
| 5 | SEC-WINTRUST: Authenticode trust: WinVerifyTrust UI/policies, SIP subjects, driver and HTTPS p | 30 | ESCALATED (DECISIONS 9): implement-later with a prefix policy switch ( |
| 6 | G-DPLAY: DirectPlay Voice and NAT helpers/servers | 30 | won't do for now (very low demand) |
| 7 | SEC-ACL: ACL and security-descriptor APIs (GetSecurityInfo/Ex, object ACEs, effective rig | 26 | ESCALATED per DECISIONS 9 but low risk: recommend the integrator appro |
| 8 | XML-PARSER: MSXML3: IXMLParser (IE5), SOM schema, DOMDocumentType entities | 26 | implement later: SOM/entities first, IXMLParser won't do (deprecated) |
| 9 | RE-OLE: RichEdit OLE embedding: IRichEditOle, IOleInPlaceSite, drop target | 24 | implement later (WordPad interop) |
| 10 | MSH-CSSOM: MSHTML CSS object model and new web APIs (rule styles, XDomainRequest, XHR uploa | 23 | implement now: toJSON x3, responseBody, window.frames, OmHistory_go |
| 11 | SEC-LSA: LSA policy store: account rights, trusted domains, secrets, private data, policy | 22 | ESCALATED (DECISIONS 9): recommend implement-later in the security-int |
| 12 | UIA: UI Automation core: patterns, proxy factories, nested providers, text events | 20 | implement later (screen readers, test tools) |
| 13 | G-WINRTCOMP: Windows.UI.Composition compositor: key-frame/expression animations, effect facto | 19 | implement later: gates WinUI/XAML apps |
| 14 | SEC-LOGON: LSA logon sessions, credential store, security packages, Negotiate/Kerberos mess | 19 | ESCALATED (DECISIONS 9): implement-later |
| 15 | G-GL32: 32-bit OpenGL vendor-extension thunks passing client pointers (wow64) | 19 | won't do (extensions are withheld |

## 2. Decision groups

Effort: S under 1 day, M 1-3 days, L a week, XL more. Recommendation values: implement now / implement later / won't do.

### G-SHARED: Cross-device shared resources: D3DKMT allocations/keyed mutex/sync objects, D3D11/D3D9 OpenSharedResource, shared fences, DXGI composition

- Items: 42   Subsystem: graphics/D3D   DLLs: d3d11, d3d9, dxgi, win32u
- What Windows does: Cross-device resource sharing: D3DKMT allocations/keyed mutex/sync objects (win32u), D3D11 OpenSharedResource*/fences, D3D9Ex shared handles. Browsers (Chrome/Edge GPU process), OBS, game capture and compositors rely on it.
- Proposal (merged from the drafters): Implement a D3DKMT shared-resource table in win32u backed by Vulkan external memory (dma-buf/opaque fd handles) keyed by NT handle/global handle, keyed mutex via shared semaphores, and let wined3d/dxgi open them on another device.
- Dependencies: winevulkan external memory/semaphore; wined3d resource import; fits 'Chrome GPU in the web' work.
- Effort: XL   Risk to existing behaviour: medium
- DECISIONS.md: DECISIONS 2 covers only dcomp surfaces
- RECOMMENDATION: implement later; high value for Chromium/Electron/OBS compat
- Examples: d3d10_device_OpenSharedResource, d3d11_device_OpenSharedFence, d3d11_device_OpenSharedResource1, d3d11_device_OpenSharedResourceByName, d3d11_device_context_Wait, d3d9_device_CreateCubeTexture ...

### RE-TOM: RichEdit Text Object Model (TOM/TOM2): units, formatted text, SetPoint, properties

- Items: 37   Subsystem: other   DLLs: riched20
- What Windows does: Text Object Model (TOM/TOM2): ITextRange/ITextSelection unit movement beyond char/word, formatted text get/set, SetPoint hit-test, TOM2 stories/displays/fonts.
- Proposal (merged from the drafters): Implement unit machinery (line, paragraph, sentence...), then TOM2 interfaces from the public TOM2 docs.
- Dependencies: riched20 editor
- Effort: XL   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement later (accessibility/automation, WordPad)
- Examples: IOleClientSite_fnGetContainer, IOleClientSite_fnGetMoniker, IOleClientSite_fnSaveObject, ITextDocument2Old_fnAttachMsgFilter, ITextDocument2Old_fnGetPreferredFont, ITextDocument2_fnGetActiveStory ...

### MSH-LEGACY: MSHTML legacy IE DHTML (behaviors/HTC, filters, expressions, recordset binding, namespaces, createControlRange, editsvcs)

- Items: 35   Subsystem: other   DLLs: mshtml
- What Windows does: MSHTML legacy IE DHTML: behaviors (HTC), filters, CSS expressions, ADO data binding, namespaces, control ranges, edit services, display services, markup pointers.
- Proposal (merged from the drafters): Each is IE-only (document modes <9); implement the ones MSHTML in IE11 still honours (control range, markup pointer unit moves, display services) on top of Gecko layout; skip filters/expressions/DSO.
- Dependencies: Gecko-based mshtml
- Effort: XL   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement later for editing/markup APIs (Outlook, old intranet apps); filters/expressions/DSO won't do
- Examples: DisplayServices_CreateDisplayPointer, DisplayServices_GetCaret, DisplayServices_GetComputedStyle, HTMLDocument4_createRenderStyle, HTMLDocument6_get_compatible, HTMLElement2_addBehavior ...

### K-TXF: Transactions: TxF *Transacted file APIs and KTM (NtCreateTransaction, transacted registry keys)

- Items: 33   Subsystem: kernel/drivers   DLLs: kernel32, ntdll
- What Windows does: Transactional NTFS (CreateFileTransacted & co) and the Kernel Transaction Manager: a rollback undoes all file/registry changes of the transaction.
- Proposal (merged from the drafters): Microsoft deprecated TxF. Real transactions would need a staging layer: implement a minimal KTM object (create/commit/rollback handles, STATUS errors) plus a journal that records created/renamed/deleted files and reverses them on rollback; read-only *Transacted calls read the live namespace.
- Dependencies: wineserver transaction object; per-prefix journal.
- Effort: XL   Risk to existing behaviour: medium
- DECISIONS.md: none
- RECOMMENDATION: won't do (deprecated); keep documented non-transacted behaviour but give real handles/errors for Commit/Rollback (S)
- Examples: CopyFileTransactedA, CopyFileTransactedW, CreateDirectoryTransactedA, CreateDirectoryTransactedW, CreateFileTransactedA, CreateFileTransactedW ...

### SEC-WINTRUST: Authenticode trust: WinVerifyTrust UI/policies, SIP subjects, driver and HTTPS providers, countersigners, catalog admin

- Items: 30   Subsystem: security/crypto   DLLs: advapi32, cryptdlg, cryptui, kernelbase, mssign32, wintrust
- What Windows does: WinVerifyTrust runs the action provider with UI choices, SIP-based subject handling (PE, catalogs, CAB, MSI, scripts), driver-signing and HTTPS providers, timestamp countersigner chains, catalog admin context 2.
- Proposal (merged from the drafters): Finish SIP create/get/put/remove/verify for the subject types Windows ships (PE, CAB, catalog, flat file), make Driver*/HTTPS* providers verify the chain instead of returning S_OK, honour WTD_UI_* with a message instead of silent success, keep UNKNOWN actions returning TRUST_E_ACTION_UNKNOWN. Unsigned/untrusted must fail the way Windows fails; do not add trust anchors beyond the system root store.
- Dependencies: crypt32 chain policy (SEC-CRYPT32); root store; SG Defender/Authenticode policy.
- Effort: XL   Risk to existing behaviour: high: stricter verification can make installers that rely on the current permissive answer fail; ship behind a policy value
- DECISIONS.md: DECISIONS 9 (security model: stays escalated)
- RECOMMENDATION: ESCALATED (DECISIONS 9): implement-later with a prefix policy switch (Windows-strict by default only after a compatibility sweep)
- Examples: CRYPT_VerifyImage, SaferSetLevelInformation, SystemFunction035, CertTrustFinalPolicy, CryptUIWizDigitalSign, SetCachedSigningLevel ...

### G-DPLAY: DirectPlay Voice and NAT helpers/servers

- Items: 30   Subsystem: media   DLLs: dplaysvr, dpnhpast, dpnhupnp, dpnsvr, dpvoice, dpvsetup
- What Windows does: DirectPlay Voice client/server over DirectPlay8, NAT helpers (UPnP/PAST), dplay servers and setup wizards - used by 2000s multiplayer games.
- Proposal (merged from the drafters): Implement the DVClient/DVServer sessions over dpnet with PCM codec transport; UPnP helper via IGD (miniupnpc). Games rarely use voice today.
- Dependencies: dpnet, miniupnpc
- Effort: XL   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: won't do for now (very low demand); GetCompressionTypes checks that already pass can be marked
- Examples: wmain, DirectPlayNATHelpCreate, DirectPlayNATHelpCreate, wmain, dpvclient_Connect, dpvclient_Create3DSoundBuffer ...

### SEC-ACL: ACL and security-descriptor APIs (GetSecurityInfo/Ex, object ACEs, effective rights, private object SD, AccessCheckByType, SetAclInformation)

- Items: 26   Subsystem: security/crypto   DLLs: advapi32, kernelbase, ntdll, sechost
- What Windows does: Provider-aware GetNamedSecurityInfoEx/GetSecurityInfoEx, GetEffectiveRightsFromAcl, LookupSecurityDescriptorParts, object-type ACEs and SetAclInformation work on real ACLs; GetSecurityInfo/SetSecurityInfo on arbitrary handle types follow the documented error table.
- Proposal (merged from the drafters): Implement over the existing W paths and ntdll Rtl* ACL code: A forms convert and call W; GetEffectiveRights walks the DACL with the documented trustee algorithm; Rtl*ObjectAce use the documented ACCESS_*_OBJECT_ACE layout; AccessCheckByType fixes the inverted status; Set/GetSecurityInfo use the handle's server-side SD. Unknown provider -> ERROR_BAD_PROVIDER as documented.
- Dependencies: wineserver SD ops (exist); sechost parse_acl already has object ACEs (patch 3661).
- Effort: M   Risk to existing behaviour: low (only replaces fake/over-granting results; AccessCheckByType is a plain bug)
- DECISIONS.md: DECISIONS 9 (security model: stays escalated)
- RECOMMENDATION: ESCALATED per DECISIONS 9 but low risk: recommend the integrator approves implement-now; the A->W forms and AccessCheckByType are mis-filed small work (see part 4)
- Examples: BuildSecurityDescriptorA, GetEffectiveRightsFromAclA, GetEffectiveRightsFromAclW, GetNamedSecurityInfoExA, GetNamedSecurityInfoExW, GetSecurityInfo ...

### XML-PARSER: MSXML3: IXMLParser (IE5), SOM schema, DOMDocumentType entities

- Items: 26   Subsystem: other   DLLs: msxml3
- What Windows does: MSXML3: the IE5 IXMLParser, SOM schema cache (ISchema), IXMLDOMDocumentType entities/notations.
- Proposal (merged from the drafters): IXMLParser is deprecated (22 methods); SOM is documented in the MSXML SDK; implement over libxml2.
- Dependencies: libxml2
- Effort: L   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement later: SOM/entities first, IXMLParser won't do (deprecated)
- Examples: domdoctype_get_entities, domdoctype_get_notations, schema_cache_getDeclaration, schema_cache_getSchema, xmlparser_Abort, xmlparser_ExpandEntity ...

### RE-OLE: RichEdit OLE embedding: IRichEditOle, IOleInPlaceSite, drop target

- Items: 24   Subsystem: other   DLLs: riched20
- What Windows does: RichEdit OLE embedding: IRichEditOle activation, IOleInPlaceSite family, object conversion, drag/drop of objects.
- Proposal (merged from the drafters): Implement in-place activation site and RichEdit OLE object lifecycle per docs (embedded Word/Paint objects).
- Dependencies: ole32 linking
- Effort: XL   Risk to existing behaviour: medium (editor regressions)
- DECISIONS.md: none
- RECOMMENDATION: implement later (WordPad interop)
- Examples: IOleInPlaceSite_fnCanInPlaceActivate, IOleInPlaceSite_fnContextSensitiveHelp, IOleInPlaceSite_fnDeactivateAndUndo, IOleInPlaceSite_fnDiscardUndoState, IOleInPlaceSite_fnGetWindow, IOleInPlaceSite_fnGetWindowContext ...

### MSH-CSSOM: MSHTML CSS object model and new web APIs (rule styles, XDomainRequest, XHR upload/response, toJSON, SVG, window.frames)

- Items: 23   Subsystem: other   DLLs: mshtml
- What Windows does: MSHTML CSSOM and web APIs: rule styles, imports, ownerRule, XDomainRequest, XHR upload/response document/responseBody, performance toJSON, SVG, window.frames.
- Proposal (merged from the drafters): Most are thin wrappers over Gecko DOM interfaces missing from nsiface.idl; toJSON needs a script Object helper.
- Dependencies: Gecko IDL additions
- Effort: L   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement now: toJSON x3, responseBody, window.frames, OmHistory_go; the rest later
- Examples: DocNodeOleInPlaceObjectWindowless_GetDropTarget, DocObjOleInPlaceObjectWindowless_GetDropTarget, HTMLObjectElement2_put_classid, HTMLPerformanceNavigation_toJSON, HTMLPerformanceTiming_toJSON, HTMLPerformance_toJSON ...

### SEC-LSA: LSA policy store: account rights, trusted domains, secrets, private data, policy notification

- Items: 22   Subsystem: security/crypto   DLLs: advapi32
- What Windows does: LSA keeps policy, account rights (SeXxxRight assignments), trusted-domain objects, secrets and private data; LsaAddAccountRights/Enumerate* drive who may log on and which privileges a new token holds.
- Proposal (merged from the drafters): Back it by a registry store in the prefix owned by SYSTEM (HKLM\SECURITY-like), enumerate real accounts from sg-admind for LsaEnumerateAccounts, store rights per SID, keep trusted domains empty (documented STATUS_NO_MORE_ENTRIES / OBJECT_NAME_NOT_FOUND) but stop faking success for Create/Set/Delete; admin-gated via the policy handle's access mask; SetSecret/StorePrivateData persist encrypted blobs.
- Dependencies: sg-admind user verbs; DECISIONS 7 (local accounts); token creation reading rights.
- Effort: L   Risk to existing behaviour: medium: rights stored but not yet enforced at logon could surprise; enforce incrementally
- DECISIONS.md: DECISIONS 9 (security model: stays escalated)
- RECOMMENDATION: ESCALATED (DECISIONS 9): recommend implement-later in the security-integrator round, after DECISIONS 7 lands
- Examples: LsaAddAccountRights, LsaCreateTrustedDomainEx, LsaDeleteTrustedDomain, LsaEnumerateAccountRights, LsaEnumerateAccounts, LsaEnumerateAccountsWithUserRight ...

### UIA: UI Automation core: patterns, proxy factories, nested providers, text events

- Items: 20   Subsystem: other   DLLs: uiautomationcore
- What Windows does: UI Automation core: patterns beyond Invoke/Value/Toggle, proxy factory mapping, nested providers, text events, caching scopes.
- Proposal (merged from the drafters): Implement the remaining patterns as client wrappers and provider forwards; documented per pattern.
- Dependencies: oleacc bridge
- Effort: L   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement later (screen readers, test tools)
- Examples: UiaEventRemoveWindow, UiaGetPropertyValue, UiaGetUpdatedCache, bridge_GetIDsOfNames, bridge_GetTypeInfo, bridge_Invoke ...

### G-WINRTCOMP: Windows.UI.Composition compositor: key-frame/expression animations, effect factories, property sets, scoped batches

- Items: 19   Subsystem: graphics/D3D   DLLs: dcomp
- What Windows does: Windows.UI.Composition compositor: keyframe/expression/easing animations, effect factories, property sets, scoped batches, commit batch - used by XAML/WinUI apps and OneDrive's UI.
- Proposal (merged from the drafters): Implement an animation engine in dcomp (time-driven property interpolation, easing functions, expression evaluation of the documented expression grammar) on top of the existing visual tree.
- Dependencies: Windows.UI.Xaml wall (see OneDrive bring-up).
- Effort: XL   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement later: gates WinUI/XAML apps
- Examples: compositor_CreateColorKeyFrameAnimation, compositor_CreateCubicBezierEasingFunction, compositor_CreateEffectFactory, compositor_CreateEffectFactoryWithProperties, compositor_CreateExpressionAnimation, compositor_CreateExpressionAnimationWithExpression ...

### SEC-LOGON: LSA logon sessions, credential store, security packages, Negotiate/Kerberos messages, klist

- Items: 19   Subsystem: security/crypto   DLLs: kerberos, klist, secur32
- What Windows does: LSA logon sessions, authentication packages, the credential store behind SSPI, Negotiate/Kerberos message dispatch; klist shows the Kerberos ticket cache.
- Proposal (merged from the drafters): Keep a logon-session table (LUID -> user, package, time) seeded from sg-session, make LsaEnumerateLogonSessions/GetLogonSessionData return SYSTEM + real interactive sessions, LsaLogonUser create a real token via the existing logon path only for administrators holding SeTcbPrivilege, AddSecurityPackage admin-only. Kerberos ticket verbs return the documented no-tickets answers until a KDC client exists.
- Dependencies: wineserver token/logon support; sg-session; a Kerberos client for domain use (see N-DOMAIN).
- Effort: XL   Risk to existing behaviour: medium-high: logon paths affect sign-in
- DECISIONS.md: DECISIONS 9 (security model: stays escalated)
- RECOMMENDATION: ESCALATED (DECISIONS 9): implement-later; start with Enumerate/GetLogonSessionData (apps such as VS Code hit them)
- Examples: kerberos_LsaApCallPackageUntrusted, get, purge, tgt, AddSecurityPackageA, AddSecurityPackageW ...

### G-GL32: 32-bit OpenGL vendor-extension thunks passing client pointers (wow64)

- Items: 19   Subsystem: graphics/D3D   DLLs: opengl32, win32u
- What Windows does: 32-bit OpenGL vendor extensions that pass client pointers/host handles (IBM_vertex_array_lists, INTEL_parallel_arrays, ATI_map_object_buffer...).
- Proposal (merged from the drafters): Already withheld from wow64 clients by patch 3635 (not advertised): leave unadvertised; no application of the 32-bit era needs them.
- Dependencies: none
- Effort: S   Risk to existing behaviour: none
- DECISIONS.md: none
- RECOMMENDATION: won't do (extensions are withheld; stubs are unreachable)
- Examples: wow64_ext_glColorPointerListIBM, wow64_ext_glColorPointervINTEL, wow64_ext_glCreateSyncFromCLeventARB, wow64_ext_glEdgeFlagPointerListIBM, wow64_ext_glFogCoordPointerListIBM, wow64_ext_glGetVkProcAddrNV ...

### SEC-PSTORE: Protected Storage (pstorec IPStore)

- Items: 19   Subsystem: security/crypto   DLLs: pstorec
- What Windows does: Protected Storage (pstorec) is the pre-Vista per-user secret store (IPStore: types, subtypes, items, access rulesets). It was removed in Windows Vista; only legacy apps (old IE autocomplete readers, password recovery tools) call it.
- Proposal (merged from the drafters): Registry-backed store under the user's hive encrypted with DPAPI (CryptProtectData), implementing the 19 IPStore methods with the documented PST_E_* errors.
- Dependencies: DPAPI (crypt32).
- Effort: M   Risk to existing behaviour: low
- DECISIONS.md: DECISIONS 9 (security model: stays escalated)
- RECOMMENDATION: won't do for now: removed from Windows 6+; revisit only if an XP-era app needs it
- Examples: PStore_fnCloseItem, PStore_fnCreateSubtype, PStore_fnCreateType, PStore_fnDeleteItem, PStore_fnDeleteSubtype, PStore_fnDeleteType ...

### K-MISC: Misc core gaps: working-set watch, I/O rings, MUI resource enumeration flags, logical-processor info >64 CPUs, MemoryWorkingSetEx, directory cache, cross-process window messages, virtual disks

- Items: 18   Subsystem: kernel/drivers   DLLs: advapi32, kernelbase, ntdll, virtdisk, win32u
- What Windows does: Scattered core gaps: cross-process WM_SETTEXT-style message packing, symlink fixes, MUI resource enumeration flags, >64-CPU processor info, MemoryWorkingSetEx, working-set watch, I/O rings, wait chain, VHD attach, directory cache.
- Proposal (merged from the drafters): Per item: EnumResource*Ex honours RESOURCE_ENUM_MUI (look in .mui); processor groups >64; working set from /proc/self/pagemap; QueryIoRingCapabilities returns the documented version struct; VHD via loop device through sg-admind (admin).
- Dependencies: varies.
- Effort: M-L   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement now for the resource/processor/working-set items (documented); VHD/WCT later
- Examples: GetThreadWaitChain, OpenThreadWaitChainSession, EnumResourceLanguagesExA, EnumResourceLanguagesExW, EnumResourceNamesExA, EnumResourceNamesExW ...

### CRT: C/C++ runtime internals (concrt, msvcp facets, msvcrt SEH, debug heap)

- Items: 18   Subsystem: other   DLLs: concrt140/msvcr1x0, msvcp, msvcp100+, msvcp140, msvcp90, msvcrt, msvcrtd
- What Windows does: C/C++ runtime internals: ConcRT scheduler objects, num_get::_Getffldx, locale facets for msvcp100+, SEH _abnormal_termination, debug heap, _heapmin.
- Proposal (merged from the drafters): _heapmin and abnormal_termination are small; _Getffldx and msvcrtd debug heap design; ConcRT private layouts are only in public headers (reimplement).
- Dependencies: none
- Effort: M overall   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement now for _heapmin (RtlCompactHeap), abnormal_termination; others later; _Getffldx uses licensing-restricted source: reimplement from the C++ standard
- Examples: CreateResourceManager, _ConcRT_* hooks, location, set_task_execution_resources, agent, _Atexit; NFS_GetLineSize, num_get::_Getffldx (round 4), messages/money facets for _Make*loc (round 4), std::filesystem conformance failures, locale__Locimp__Makeushloc, locale__Locimp__Makewloc ...

### SEC-TOKEN: Token information classes, group adjust, compare, lowbox, anonymous impersonation, winlogon registrations, DDE QoS

- Items: 17   Subsystem: security/crypto   DLLs: kernelbase, ntdll, user32, win32u, wow64
- What Windows does: Token classes (groups, session id, UI access), NtCompareTokens, NtAdjustGroupsToken, lowbox tokens, anonymous-token impersonation; winlogon-only registrations in user32; DDE QoS.
- Proposal (merged from the drafters): Implement Compare (same SIDs/privileges/groups/restrictions), AdjustGroupsToken (enable/disable non-mandatory groups, return previous state), SessionId set (SeTcb), UIAccess from the token; lowbox: real token with package SID; anonymous: server-side anonymous token. user32 winlogon calls: require SeTcbPrivilege and store the registration instead of faking success.
- Dependencies: wineserver token requests (new request => full protocol rebuild, see wine-protocol-renumbering).
- Effort: L   Risk to existing behaviour: medium (protocol change)
- DECISIONS.md: DECISIONS 9 (security model: stays escalated)
- RECOMMENDATION: ESCALATED (DECISIONS 9): implement-later, bundle all token requests in one protocol change
- Examples: RegOpenUserClassesRoot, NtAdjustGroupsToken, NtCompareTokens, NtImpersonateAnonymousToken, NtQueryInformationToken, NtSetInformationToken ...

### SYS-PRIV: Privilege-gated system operations that fake success today (paging file, ReplaceKey, shutdown, SetSystemInformation, driver load/unload, debug filter, power action, firmware variables)

- Items: 16   Subsystem: kernel/drivers   DLLs: advapi32, kernel32, ntdll, services
- What Windows does: Privilege-gated system operations: create paging file, replace key, shutdown, set system information, load/unload driver, set debug filter, power action, firmware variables and boot-config acceptance. Windows demands the matching privilege (SeCreatePagefilePrivilege, SeRestorePrivilege, SeShutdownPrivilege, SeLoadDriverPrivilege, SeSystemEnvironmentPrivilege...), else STATUS_PRIVILEGE_NOT_HELD.
- Proposal (merged from the drafters): Return STATUS_PRIVILEGE_NOT_HELD for callers lacking the privilege (exists in tokens); with the privilege: NtShutdownSystem/NtInitiatePowerAction -> sg-session shutdown/reboot/suspend verbs, NtReplaceKey -> swap prefix hive file, NtLoadDriver/Unload -> wine driver service start/stop, firmware vars -> read-only EFI vars via efivarfs (write only for administrators), NtCreatePagingFile -> STATUS_NOT_IMPLEMENTED-like documented failure. Never fake success.
- Dependencies: sg-admind verbs for shutdown/driver; token privilege model.
- Effort: M-L   Risk to existing behaviour: medium (shutdown/power must be gated)
- DECISIONS.md: DECISIONS 9 (security model: stays escalated); DECISIONS 6 pattern (admin-only through sg-admind)
- RECOMMENDATION: implement later: replace fake success with privilege checks first (S), real actions via sg-admind afterwards
- Examples: NotifyBootConfigStatus, RegReplaceKeyW, SetFirmwareEnvironmentVariableA, SetFirmwareEnvironmentVariableW, NtCreatePagingFile, NtInitiatePowerAction ...

### NET-HOST: CLR hosting, strong naming, C++/CLI runtime (msvcm80)

- Items: 16   Subsystem: .NET   DLLs: mscoree, msvcm80
- What Windows does: CLR hosting: appdomain unload, host control, strong name verification/signing (ICLRStrongName), C++/CLI startup (msvcm80), CLR runtime host GetInterface.
- Proposal (merged from the drafters): StrongName APIs are documented (SHA1 + RSA); msvcm80 managed-startup helpers need the CLR to run delegates/exceptions through Mono embedding.
- Dependencies: Wine Mono embedding
- Effort: L   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement later: StrongName first (S-M)
- Examples: CLRRuntimeHost_ExecuteApplication, CLRRuntimeHost_GetCLRControl, CLRRuntimeHost_SetHostControl, CLRRuntimeHost_UnloadAppDomain, CoEEShutDownCOM, RuntimeHost_GetInterface ...

### K-CALLBACKS: ntoskrnl notification/filter callbacks (process/thread notify, registry callbacks, FS filters, ObRegisterCallbacks, device attach, WMI)

- Items: 16   Subsystem: kernel/drivers   DLLs: ntoskrnl.exe
- What Windows does: Driver callback registration: process/thread create notify, registry callbacks (CmRegisterCallback), file-system filters, ObRegisterCallbacks (needs signed driver), device attach, WMI registration.
- Proposal (merged from the drafters): Registration tables in ntoskrnl invoked from wineserver events (process/thread notifications are directly feasible; registry/file filters need hooks in server registry code); ObRegisterCallbacks requires the signing check (SEC-WINTRUST).
- Dependencies: wineserver event notifications to drivers.
- Effort: L-XL   Risk to existing behaviour: medium
- DECISIONS.md: none
- RECOMMENDATION: implement later: process/thread notify first (anti-cheat/AV drivers)
- Examples: CmRegisterCallback, CmUnRegisterCallback, FsRtlRegisterFileSystemFilterCallbacks, FsRtlRegisterUncProvider, IoAttachDevice, IoGetDeviceObjectPointer ...

### SEC-AUDIT: Audit-alarm APIs and audit policy (SeAuditPrivilege gating)

- Items: 15   Subsystem: security/crypto   DLLs: advapi32, kernelbase, ntdll
- What Windows does: Audit alarms require SeAuditPrivilege (ERROR_PRIVILEGE_NOT_HELD), evaluate the SACL, set generate-on-close, and write security events 4673/4674 only when auditing is enabled; AuditQuerySystemPolicy needs SeSecurityPrivilege.
- Proposal (merged from the drafters): Gate on the token privilege, evaluate the SACL, return the documented outputs, write nothing unless the audit policy (registry-backed in the prefix) says so; A forms convert to W.
- Dependencies: token privileges (exist), event log (wevtapi store).
- Effort: S-M   Risk to existing behaviour: low
- DECISIONS.md: DECISIONS 9 (security model: stays escalated)
- RECOMMENDATION: ESCALATED (DECISIONS 9); recommend implement-now of the privilege check + outputs, event emission later
- Examples: AccessCheckAndAuditAlarmA, AuditQuerySystemPolicy, GetAuditedPermissionsFromAclA, GetAuditedPermissionsFromAclW, ObjectCloseAuditAlarmA, ObjectOpenAuditAlarmA ...

### G-D2D: Direct2D device-context gaps (color glyph runs, BlendImage, effect rects, print control, gradient mesh, metafile, DXGI sources)

- Items: 15   Subsystem: graphics/D3D   DLLs: d2d1
- What Windows does: Direct2D: colour bitmap/SVG glyph runs, BlendImage (26 modes), effect rect propagation, print control, gradient mesh edge modes, metafile src rect, multi-plane DXGI sources.
- Proposal (merged from the drafters): Implement per method with the documented algorithms (blend modes are formulas); print control writes XPS via IPrintDocumentPackageTarget.
- Dependencies: d2d1 effect graph
- Effort: L   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement later; blend modes and src_rect are S
- Examples: d2d_device_CreatePrintControl, d2d_device_context_BlendImage, d2d_device_context_CreateImageSourceFromDxgi, d2d_device_context_DrawColorBitmapGlyphRun, d2d_device_context_DrawGdiMetafile, d2d_device_context_DrawGradientMesh ...

### G-D3D11CLASS: D3D11 shader class linkage (SM5 interfaces)

- Items: 15   Subsystem: graphics/D3D   DLLs: d3d11
- What Windows does: D3D11 shader class linkage / interfaces (SM5): ID3D11ClassLinkage, class instances, SetShader with instances.
- Proposal (merged from the drafters): Implement ID3D11ClassLinkage/ClassInstance objects and have the shader translator lower interface calls to switch-based dispatch (as dxbc does); wined3d needs interface slot support.
- Dependencies: wined3d shader backends / vkd3d-shader.
- Effort: XL   Risk to existing behaviour: medium
- DECISIONS.md: none
- RECOMMENDATION: implement later (rare in games; some D3D11 samples use it)
- Examples: d3d11_class_linkage_CreateClassInstance, d3d11_class_linkage_GetClassInstance, d3d11_device_CreateComputeShader, d3d11_device_CreateDomainShader, d3d11_device_CreateGeometryShader, d3d11_device_CreateGeometryShaderWithStreamOutput ...

### NET-DBG: Wine Mono debug agent: ICorDebug process/thread/value inspection

- Items: 15   Subsystem: .NET   DLLs: mscoree
- What Windows does: Wine Mono debug agent: ICorDebug process/thread/value inspection so Visual Studio / dnSpy / MS debuggers can attach to managed code.
- Proposal (merged from the drafters): Implement the CLR debugger API shim over the Mono soft-debugger protocol (SDB agent): ICorDebugProcess enumerate threads/appdomains/objects, stop/continue, exception clear, log switches.
- Dependencies: Wine Mono fork (/var/tmp/wine-mono-src), Mono debugger agent
- Effort: XL   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement later: needed for .NET debugging in VS; large
- Examples: CLRCreateInstance, CorDebug_DebugActiveProcess, cordebugprocess_ClearCurrentException, cordebugprocess_EnableLogMessages, cordebugprocess_EnumerateAppDomains, cordebugprocess_EnumerateObjects ...

### K-LPC: Legacy LPC ports (NtCreatePort, NtConnectPort ... and wow64 thunks)

- Items: 15   Subsystem: kernel/drivers   DLLs: ntdll, wow64
- What Windows does: Legacy LPC ports (NtCreatePort...), used by csrss/lsass clients and some security software.
- Proposal (merged from the drafters): Implement over Unix sockets in wineserver: named port objects, connect/listen/accept/complete, request/reply messages with the PORT_MESSAGE header.
- Dependencies: wineserver new objects (protocol rebuild).
- Effort: XL   Risk to existing behaviour: medium
- DECISIONS.md: none
- RECOMMENDATION: implement later (tests exist; few user programs need it); wow64 thunks follow
- Examples: NtAcceptConnectPort, NtCompleteConnectPort, NtConnectPort, NtCreatePort, NtListenPort, NtRegisterThreadTerminatePort ...

### RDS: Remote Desktop Services: virtual channels (wtsapi32), session shadow/connect

- Items: 15   Subsystem: networking   DLLs: wtsapi32
- What Windows does: RDS API: virtual channels, session shadowing, connect/disconnect, WTSWaitSystemEvent, process enumeration per session.
- Proposal (merged from the drafters): Single-session-per-user model: channels never open (documented failure), shadowing returns ERROR_NOT_SUPPORTED, WTSEnumerateProcessesEx honours session id (sg-session knows the owner), WTSWaitSystemEvent waits on session events from sg-session.
- Dependencies: sg-session session table.
- Effort: M   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement now for WTSEnumerateProcessesExW and WTSWaitSystemEvent; channels won't do (no RDP stack)
- Examples: WTSConnectSessionA, WTSConnectSessionW, WTSEnumerateProcessesExW, WTSStartRemoteControlSessionA, WTSStartRemoteControlSessionW, WTSStopRemoteControlSession ...

### DBG-REMOTE: dbgeng remote engine: servers, process servers, session connect, kernel attach

- Items: 14   Subsystem: other   DLLs: dbgeng
- What Windows does: dbgeng remote engine: debugging servers, process servers, session connect, kernel attach.
- Proposal (merged from the drafters): Wire protocols not documented (DbgSrv); only process-server TCP basics documented.
- Dependencies: none
- Effort: XL   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: won't do (undocumented protocol)
- Examples: debugclient_AttachKernel, debugclient_AttachKernelWide, debugclient_ConnectProcessServer, debugclient_ConnectProcessServerWide, debugclient_ConnectSession, debugclient_DisconnectProcessServer ...

### COM-SCRIPT: Script control, DHTML editing control, scripting engines (msscript, dhtmled, vbscript debug, jscript accessors)

- Items: 14   Subsystem: COM/RPC   DLLs: dhtmled.ocx, jscript, msscript.ocx, vbscript
- What Windows does: Script control persistence/draw, DHTML editing control, VBScript debug code contexts, JScript accessors on non-function objects.
- Proposal (merged from the drafters): Script control Timeout enforcement + persistence format are unspecified (stream format unknown): return E_NOTIMPL as Windows would not; DHTMLEdit commands map to mshtml editing; IActiveScriptDebug is documented.
- Dependencies: mshtml editing
- Effort: L   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement later (low demand)
- Examples: DHTMLEdit_ExecCommand, DHTMLEdit_PrintDocument, DHTMLEdit_QueryStatus, DHTMLEdit_SetContextMenu, OleInPlaceObjectWindowless_GetDropTarget, ViewObjectEx_Draw ...

### M-MMCSS: MMCSS/deadline work queues (rtworkq)

- Items: 14   Subsystem: media   DLLs: rtworkq
- What Windows does: Media Foundation platform work queues with MMCSS class/priority, deadlines, I/O-completion joins.
- Proposal (merged from the drafters): Implement MMCSS class bookkeeping (store task id/priority, map to nice/RT), deadlines as timers, Join/Unjoin via an IO completion association.
- Dependencies: none
- Effort: M   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement now (documented, self-contained; Rtwq* deadline items are mis-filed small)
- Examples: RtwqBeginRegisterWorkQueueWithMMCSS, RtwqBeginUnregisterWorkQueueWithMMCSS, RtwqCancelDeadline, RtwqEndRegisterWorkQueueWithMMCSS, RtwqEndUnregisterWorkQueueWithMMCSS, RtwqGetWorkQueueMMCSSClass ...

### SEC-CRED: Credential marshal / Windows Hello KeyCredential / online ID / web account manager

- Items: 13   Subsystem: security/crypto   DLLs: advapi32, cryptowinrt, windows.security.authentication.onlineid, windows.security.authentication.web.core
- What Windows does: Credential marshalling of blob credentials, domain credential read, Windows Hello KeyCredentialManager, online ID authenticator, WebAccount sign-out / token cache.
- Proposal (merged from the drafters): CredMarshal/Unmarshal blob forms are documented (CRED_MARSHAL_TYPE encoding) and implementable now; KeyCredential and web-account items need a TPM/Hello provider and MS account broker (return the documented 'unavailable' codes).
- Dependencies: No TPM/AAD broker (see Office/OneDrive notes).
- Effort: S (marshal) / XL (Hello, AAD)   Risk to existing behaviour: low
- DECISIONS.md: DECISIONS 9 (security model: stays escalated)
- RECOMMENDATION: implement now for CredMarshal/Unmarshal and CredReadDomainCredentials (small, documented); Hello/AAD items won't do
- Examples: CredMarshalCredentialW, CredReadDomainCredentialsW, CredUnmarshalCredentialW, credentials_statics_DeleteAsync, credentials_statics_OpenAsync, credentials_statics_RenewAttestationAsync ...

### SEC-CRYPT32: crypt32 chain policy, CTL usage, physical stores, streamed messages, key-identifier properties, CRL/CTL dialogs

- Items: 13   Subsystem: security/crypto   DLLs: crypt32, cryptext, cryptnet, cryptui
- What Windows does: Chain building honours issuance policy usages, CTL usage verification, physical cert store registration, streamed PKCS#7 encode/decode callbacks, key-identifier property storage.
- Proposal (merged from the drafters): Implement physical store registration in the registry layout Windows uses, streamed message output via pfnStreamOutput, CertVerifyCTLUsage over existing CTL parsing, key-identifier properties in the registry; chain policy verification (AUTHENTICODE/ BASIC/ SSL flags) per the documented CERT_CHAIN_POLICY_* status codes.
- Dependencies: SEC-WINTRUST for policy semantics.
- Effort: L   Risk to existing behaviour: medium
- DECISIONS.md: DECISIONS 9 (security model: stays escalated)
- RECOMMENDATION: ESCALATED (DECISIONS 9): implement-later; CertRegister/UnregisterPhysicalStore and streamed messages are separable and low risk
- Examples: CDecodeMsg_Update, CEnvelopedEncodeMsg_Update, CRYPT_CheckUsages, CSignedEncodeMsg_Update, CertRegisterPhysicalStore, CertUnregisterPhysicalStore ...

### DEVICES: Still Image / WIA / tdh / ETW formatting / misc

- Items: 13   Subsystem: other   DLLs: several, sti, tdh
- What Windows does: Still image (sti) device create/state, ETW trace data helper (tdh Format/GetProperty/map info), misc.
- Proposal (merged from the drafters): Per David 2026-10-13 sti/wiaservc stay read-only (DECISIONS 4); tdh: implement property parsing per the documented TRACE_EVENT_INFO and EVENT_MAP_INFO using manifests already parsed.
- Dependencies: tdh manifest parser
- Effort: M   Risk to existing behaviour: low
- DECISIONS.md: DECISIONS 4 (sti: read-only)
- RECOMMENDATION: implement now for tdh property functions; sti won't do
- Examples: "Not done" list (engines, dialogs, undocumented signatures), stillimagew_CreateDevice, stillimagew_EnableHwNotifications, stillimagew_LaunchApplicationForDevice, stillimagew_RefreshDeviceBus, stillimagew_SetDeviceValue ...

### COM-MAIL: CDO, MAPI profiles/forms/address dialogs, inetcomm MimeSecurity/MimeMessage

- Items: 12   Subsystem: COM/RPC   DLLs: cdosys, inetcomm, mapi32, winemapi
- What Windows does: CDO message component, MAPI profile admin/forms/address dialogs, inetcomm MIME security (S/MIME) and web page creation.
- Proposal (merged from the drafters): MIME S/MIME sign/encrypt on top of crypt32 messages; MAPI address/details dialogs belong to the mail client (return MAPI_E_NOT_SUPPORTED); CDO is a full COM library (large).
- Dependencies: crypt32 messages
- Effort: XL   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: won't do for CDO/MAPI profile admin; implement later for MimeSecurity
- Examples: DllGetClassObject, MimeMessage_CreateWebPage, MimeMessage_GetRootMoniker, MimeMessage_SplitMessage, MimeSecurity_DecodeBody, MimeSecurity_DecodeMessage ...

### G-PROTECT: D3D11 content protection (crypto sessions, authenticated channels, protected video)

- Items: 12   Subsystem: graphics/D3D   DLLs: d3d11
- What Windows does: D3D11 content protection: crypto sessions/authenticated channels for protected (PlayReady/HDCP) video.
- Proposal (merged from the drafters): Report no content-protection capability (current) and return the documented unsupported errors; do not emulate DRM.
- Dependencies: none
- Effort: S   Risk to existing behaviour: none
- DECISIONS.md: none
- RECOMMENDATION: won't do (DRM; documented unsupported answers are enough)
- Examples: d3d11_video_context_ConfigureAuthenticatedChannel, d3d11_video_context_DecryptionBlt, d3d11_video_context_EncryptionBlt, d3d11_video_context_FinishSessionKeyRefresh, d3d11_video_context_GetEncryptionBltKey, d3d11_video_context_NegotiateAuthenticatedChannelKeyExchange ...

### DBG-DUMP: dbgeng dump targets, assembler, extension APIs, advanced requests

- Items: 12   Subsystem: other   DLLs: dbgeng
- What Windows does: dbgeng dump files targets, assembler, extension APIs, advanced requests.
- Proposal (merged from the drafters): Dump target: minidump reader (format documented in dbghelp headers) and use the existing target abstraction; assembler needs keystone/asm library.
- Dependencies: capstone/keystone
- Effort: L   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement later: dump files first
- Examples: DebugConnect, debugadvanced_Request, debugclient_AddDumpInformationFile, debugclient_AddDumpInformationFileWide, debugclient_AttachProcess, debugclient_CreateProcess ...

### NET-META: Metadata dispenser / importer / emitter (IMetaDataDispenser, rometadata, GetAssemblyMDImport)

- Items: 12   Subsystem: .NET   DLLs: mscoree, rometadata
- What Windows does: CLR metadata APIs: IMetaDataDispenser, IMetaDataImport/Emit, rometadata, GetAssemblyMDImport (ECMA-335 readers/writers).
- Proposal (merged from the drafters): Implement a managed-metadata reader (ECMA-335 is open) and emit; large but spec-complete; shared by compilers, ILSpy, regasm and WinRT metadata (rometadata).
- Dependencies: Wine Mono / own reader
- Effort: XL   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement later (unlocks regasm/regsvcs/tlbimp too)
- Examples: GetAssemblyMDImport, MetaDataDispenser_DefineScope, MetaDataDispenser_FindAssembly, MetaDataDispenser_FindAssemblyModule, MetaDataDispenser_GetCORSystemDirectory, MetaDataDispenser_GetOption ...

### K-MM: ntoskrnl physical memory, IO-space mapping, process attach, Probe*, cross-process copy

- Items: 12   Subsystem: kernel/drivers   DLLs: ntoskrnl.exe
- What Windows does: ntoskrnl memory manager: MmMapIoSpace, contiguous/physical allocation, MmGetPhysicalAddress, KeStackAttachProcess, Probe*, MmCopyVirtualMemory.
- Proposal (merged from the drafters): Probe* and MmCopyVirtualMemory/KeQueryPriorityThread are documented and portable (S); physical memory APIs map to /dev/mem only through an admin helper; stack-attach switches the thread's process context in wineserver.
- Dependencies: wineserver process attach; admin helper.
- Effort: L   Risk to existing behaviour: medium
- DECISIONS.md: none
- RECOMMENDATION: implement now for Probe*, KeQueryPriorityThread, MmCopyVirtualMemory; the rest later/won't do
- Examples: KeQueryPriorityThread, KeStackAttachProcess, KeUnstackDetachProcess, MmAllocateContiguousMemory, MmAllocateContiguousMemorySpecifyCache, MmAllocatePagesForMdl ...

### G-WINED3D-GL: wined3d GL gaps: render-to-buffer, depth load locations, SM2 predication, D3DRS_ANTIALIAS

- Items: 12   Subsystem: graphics/D3D   DLLs: wined3d
- What Windows does: wined3d GL gaps: render-to-buffer RTVs, depth load to renderbuffer/drawable, SM2 predication, D3DRS_ANTIALIAS.
- Proposal (merged from the drafters): Implement per GL path; since DXVK ships, wined3d is the fallback only.
- Dependencies: none
- Effort: L   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement later / low priority
- Examples: context_generate_rt_mask_from_resource, cpu_blitter_clear_rtv, draw_primitive, shader_generate_code, wined3d_context_gl_set_fbo_key_for_render_target, wined3d_device_apply_stateblock ...

### P-APPX: Appx packaging (block map, bundle writer, package writer, newer interfaces) and OPC signatures

- Items: 11   Subsystem: installers   DLLs: appxpackaging, opcservices
- What Windows does: Appx/MSIX packaging readers/writers: block map reader, bundle/package writers, newer interface IIDs, device capabilities, OPC digital signature manager.
- Proposal (merged from the drafters): Blocked on missing IDL definitions (IAppxBlockMapReader etc.): the headers exist publicly in the Windows SDK (not Wine). Reimplement interface definitions ourselves from docs, then implement the readers (open OPC/MSIX format).
- Dependencies: OPC (opcservices), SIP for AXBM
- Effort: L   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement later (Store/MSIX installs)
- Examples: appx_factory_CreateBlockMapReader, appx_factory_CreatePackageWriter, appx_factory_CreateValidatedBlockMapReader, appx_factory_QueryInterface, bundle_GetBlockMap, bundle_factory_CreateBundleWriter ...

### M-LEGACY: Legacy multimedia dialogs (acmFilterChoose, AVI clipboard, MCI configure, msvfw previews, midiConnect), SAPI voice

- Items: 11   Subsystem: media   DLLs: avifil32, mciavi32, msacm32, msttsengine, msvfw32, winmm
- What Windows does: Legacy multimedia dialogs and APIs: acmFilterChoose, AVIGetFromClipboard, MCI configure, preview file dialogs, midiConnect, SAPI voices.
- Proposal (merged from the drafters): acmFilterChoose is a dialog with documented controls; clipboard AVI formats documented; midiConnect needs MIDI thru; SAPI voice engine needs a TTS backend (espeak-ng/piper).
- Dependencies: TTS backend
- Effort: M overall   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement later / low demand
- Examples: AVIGetFromClipboard, AVIMakeStreamFromClipboard, AVIPutFileOnClipboard, MCIAVI_mciConfigure, acmFilterChooseA, acmFilterChooseW ...

### COM-DCOM: DCOM core: secure private refs, handler/OBJREF marshalling, surrogate, message-filter retry, TID, debug hook, connected state

- Items: 11   Subsystem: COM/RPC   DLLs: combase
- What Windows does: DCOM: secure private references (cPrivateRefs), OBJREF_HANDLER/EXTENDED unmarshal, surrogate registration, IMessageFilter SERVERCALL_RETRYLATER, CoGetCallerTID, debug hook, handler connected state, CoDecodeProxy.
- Proposal (merged from the drafters): Extend unmarshal paths to OBJREF_HANDLER (load handler class), implement retry-later per docs, keep secure refs tied to caller identity, CoGetCallerTID from the call context, DllDebugObjectRPCHook as a stored hook.
- Dependencies: none
- Effort: L overall; S for TID/message filter   Risk to existing behaviour: low-medium (marshalling)
- DECISIONS.md: DECISIONS 9 (security model: stays escalated) for secure refs
- RECOMMENDATION: implement now for CoGetCallerTID, message-filter retry, CoRegisterSurrogate; rest later
- Examples: CoDecodeProxy, CoGetCallerTID, CoIsHandlerConnected, CoRegisterSurrogate, DllDebugObjectRPCHook, RemUnknown_RemAddRef ...

### N-MISC: Misc networking (pcap sources, SNMP manager, sensapi reachability, DisconnectEx reuse, connect.dll, urlmon zones, ieframe, WinRT networking)

- Items: 11   Subsystem: networking   DLLs: connect, ieframe, mgmtapi, sensapi, urlmon, wpcap, ws2_32
- What Windows does: Misc networking: WinPcap pcap_open sources, SNMP manager (mgmtapi), sensapi reachability, WSA DisconnectEx TF_REUSE_SOCKET, connect.dll IsInternetConnected, urlmon zone policy templates, ieframe popup/search provider, WinRT SetSocketMediaStreamingMode.
- Proposal (merged from the drafters): IsDestinationReachable/IsInternetConnected: check default route + DNS (documented semantics); DisconnectEx REUSE: dup new socket; SNMP manager: UDP SNMPv1/2 client (open RFC) or SNMP_MGMTAPI errors; zone templates tables are documented in registry.
- Dependencies: none
- Effort: M overall   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement now: sensapi, connect, DisconnectEx (small); others later
- Examples: IsInternetConnected, NewWindowManager_EvaluateNewWindow, ShellUIHelper2_AddSearchProvider, SnmpMgrOpen, SnmpMgrTrapListen, IsDestinationReachableA ...

### G-VIDDEC: D3D11 video decoder / video processor / DXVA2 (needs a hardware decode backend, e.g. VA-API)

- Items: 11   Subsystem: graphics/D3D   DLLs: d3d11, dxva2
- What Windows does: D3D11 video decode and DXVA2/DXVAHD: GPU-accelerated H.264/HEVC/VP9/AV1 decode, video processor blits (NV12->RGB), driver handle for DXVA2 interop. Browsers and media players ask for hardware decode.
- Proposal (merged from the drafters): Add a hardware decode backend: map ID3D11VideoDecoder to VA-API (libva) for supported profiles, shared with the GStreamer path; VideoProcessorBlt via a shader-based colour conversion; report no profiles (current) otherwise so apps fall back to software.
- Dependencies: libva, working GPU driver in the target VM; wined3d/DXVK interop.
- Effort: XL   Risk to existing behaviour: medium (apps that see decode profiles but get broken decode regress; gate on actual device support)
- DECISIONS.md: none
- RECOMMENDATION: implement later; keep 'no profiles' reported until a working backend exists
- Examples: d3d11_video_context_DecoderBeginFrame, d3d11_video_context_DecoderEndFrame, d3d11_video_context_DecoderExtension, d3d11_video_context_GetDecoderBuffer, d3d11_video_context_ReleaseDecoderBuffer, d3d11_video_context_SubmitDecoderBuffers ...

### P-STUBPROG: Command-line programs that are still stubs (netsh contexts, powershell fallback, wmplayer, subst, regini, wevtutil, mofcomp, extrac32 /A, regasm, regsvcs)

- Items: 11   Subsystem: installers   DLLs: extrac32, mofcomp, netsh, powershell, printui, regasm, regini, regsvcs, subst, wevtutil, wmplayer
- What Windows does: Command-line tools whose stubs remain: netsh contexts, powershell fallback, wmplayer launcher, subst, regini, wevtutil, mofcomp, extrac32 /A, regasm, regsvcs, printui.
- Proposal (merged from the drafters): subst/regini/extrac32 /A/netsh contexts are documented command-line behaviours (S each); wevtutil needs Evt* in wevtapi; mofcomp needs a MOF parser; regasm/regsvcs need the Wine Mono reflection path.
- Dependencies: wevtapi store, Wine Mono.
- Effort: L overall   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement now for subst, regini, extrac32 /A, printui verbs; others later
- Examples: wWinMain, wmain, wmain, wmain, PrintUIEntryW, wmain ...

### N-WFP: Windows Filtering Platform (fwpuclnt) and Traffic Control QoS

- Items: 11   Subsystem: networking   DLLs: fwpuclnt, traffic
- What Windows does: Windows Filtering Platform management (fwpuclnt Fwpm*) and Traffic Control QoS (Tc*).
- Proposal (merged from the drafters): Keep an in-process filter/sublayer database (engine handle, add/enum/delete) that does not filter packets (documented success semantics for management calls); real enforcement would map to nftables via sg-admind (admin).
- Dependencies: sg-admind/nft
- Effort: M (database) / XL (enforcement)   Risk to existing behaviour: medium if enforcement
- DECISIONS.md: DECISIONS 6 pattern
- RECOMMENDATION: implement later: database only (firewall tools expect the calls to succeed)
- Examples: FwpmEngineClose0, FwpmEngineOpen0, FwpmFilterAdd0, FwpmFilterCreateEnumHandle0, FwpmFilterDestroyEnumHandle0, FwpmFilterEnum0 ...

### N-RRAS: Routing and Remote Access administration (mprapi)

- Items: 11   Subsystem: networking   DLLs: mprapi
- What Windows does: Routing and Remote Access administration (mprapi): RRAS server settings, dial-in properties, tunnels.
- Proposal (merged from the drafters): No RRAS service exists; return the documented RPC-unavailable errors (ERROR_BAD_NETPATH/RPC_S_SERVER_UNAVAILABLE) consistently.
- Dependencies: none
- Effort: S   Risk to existing behaviour: none
- DECISIONS.md: DECISIONS 8 is RAS phonebook (rasapi), not mprapi
- RECOMMENDATION: won't do (server role); make errors consistent (S)
- Examples: MprAdminEstablishDomainRasServer, MprAdminGetPDCServer, MprAdminInterfaceGetCredentials, MprAdminInterfaceSetCredentials, MprAdminIsDomainRasServer, MprAdminUserGetInfo ...

### RPC-NDR: NDR [pipe] and async stub/client support, NdrConvert, pointer unmarshal

- Items: 11   Subsystem: COM/RPC   DLLs: rpcrt4
- What Windows does: NDR [pipe] support, NdrAsync* DCOM async stubs, NdrConvert (data-rep conversion), pointer attribute unmarshal, async abort/cancel with PDUs.
- Proposal (merged from the drafters): Implement pipe types in the interpreter (Oi2 HasPipes), async server calls with a fault PDU on abort, NdrConvert for little-endian only (documented no-op) and big-endian conversion.
- Dependencies: none
- Effort: XL   Risk to existing behaviour: medium (core RPC marshalling)
- DECISIONS.md: none
- RECOMMENDATION: implement later; pipes needed by Edge/WebView components (rpcrt4 NdrStubCall2 hit by Edge)
- Examples: I_RpcAsyncAbortCall, NdrAsyncServerCall, NdrAsyncStubCall, NdrConvert, NdrConvert2, NdrServerCallAll ...

### G-VKALLOC: Vulkan VkAllocationCallbacks pass-through

- Items: 11   Subsystem: graphics/D3D   DLLs: win32u, winevulkan
- What Windows does: Vulkan VkAllocationCallbacks pass-through (instance/device/surface/swapchain).
- Proposal (merged from the drafters): Wrap callbacks in thunks calling into the PE side (as done for wine's debug-utils callbacks).
- Dependencies: winevulkan thunk generator
- Effort: M   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement later (low priority; engines rarely require it)
- Examples: win32u_vkCreateWin32SurfaceKHR, win32u_vkDestroySurfaceKHR, win32u_vkDestroySwapchainKHR, wine_vkCreateCommandPool, wine_vkCreateDebugReportCallbackEXT, wine_vkCreateDebugUtilsMessengerEXT ...

### SYS-SETINFO: Ntdll Set* information classes that accept and ignore (object handle info, job object, thread priority, volume info, FSCTL lock volume, EA file, key write time, process callbacks)

- Items: 10   Subsystem: kernel/drivers   DLLs: ntdll, wow64
- What Windows does: NtSetInformationObject/JobObject/Thread/Key/VolumeInformationFile, NtFsControlFile lock volume, NtSetEaFile, CDROM IOCTLs: documented information classes that Wine accepts and ignores or rejects wrongly.
- Proposal (merged from the drafters): Each is an independent documented class: ThreadPriority maps to the Unix nice/RT value, KeyWriteTime stored in the server key, EA files kept in an xattr, FSCTL_LOCK_VOLUME tracks a per-volume lock flag, CDROM_GetDeviceNumber/MediaType return the documented structures.
- Dependencies: none large; some need wineserver requests.
- Effort: M (many small pieces)   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement now, split per function (most are mis-filed small items)
- Examples: CDROM_GetDeviceNumber, CDROM_GetMediaType, NtFsControlFile, NtSetEaFile, NtSetInformationJobObject, NtSetInformationKey ...

### G-GDIP: GDI+ EMF+ records, property items, CMYK output channel, winding outline

- Items: 9   Subsystem: graphics/D3D   DLLs: gdiplus
- What Windows does: GDI+ EMF+: object table kinds, DrawImagePointsRect records, playback of every record type, property items persisted on save, CMYK output channel and ICC profile, winding outline.
- Proposal (merged from the drafters): Implement remaining EMF+ records per [MS-EMFPLUS] (open spec), property item write-back in the encoders, output-channel via mscms.
- Dependencies: mscms
- Effort: L   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement later
- Examples: GdipPlayMetafileRecord, GdipRemovePropertyItem, GdipSetImageAttributesOutputChannel, GdipSetImageAttributesOutputChannelColorProfile, GdipSetPropertyItem, GdipWindingModeOutline ...

### P-MISCINST: imagehlp BindImageEx, sfc_os, itss, mstask, wimgapi

- Items: 9   Subsystem: installers   DLLs: imagehlp, itss, mstask, sfc_os, wimgapi
- What Windows does: imagehlp BindImageEx, sfc_os protection queries, itss CHM creation, mstask flags persistence, wimgapi mounted images/callbacks.
- Proposal (merged from the drafters): sfc_os: answer from the WRP file list or 'not protected' with the documented FALSE; mstask flags persist in the task .job file; wimgapi callbacks/mount list are documented.
- Dependencies: none
- Effort: M   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement now (small documented items)
- Examples: BindImageEx, ITStorageImpl_StgCreateDocfileOnILockBytes, MSTASK_ITask_GetTaskFlags, MSTASK_ITask_SetTaskFlags, SfcIsFileProtected, SfcIsKeyProtected ...

### PRN: Print spooler: localspl CUPS thread affinity/SetJob, winprint, winspool perf/SetPrinter, unidrv GPD

- Items: 9   Subsystem: printing   DLLs: localspl, unidrv, winprint, winspool.drv
- What Windows does: Print spooler: localspl SetJob level 3, CUPS thread affinity for localmon WritePort/EndDocPort, winprint ControlPrintProcessor, winspool SetPrinter remaining levels, PerfOpen counters, unidrv GPD data.
- Proposal (merged from the drafters): Documented per call: validate JobId for level 3; route all CUPS document writes through the thread that started the document; Print Queue perf counters (open); GPD data from PPD.
- Dependencies: CUPS
- Effort: M overall   Risk to existing behaviour: low
- DECISIONS.md: DECISIONS 3 covers only AddPrintProvidor/Upload
- RECOMMENDATION: implement now for SetJob/SetPrinter; the rest later
- Examples: fpSetJob, localmon_EndDocPort, localmon_WritePort, core_DrvGetGPDData, ControlPrintProcessor, PerfClose ...

### COM-WSC: Scriptlet components (scrobj WSC) - rewritten by unmerged fast-a patches

- Items: 9   Subsystem: COM/RPC   DLLs: scrobj
- What Windows does: Windows Script Components (.wsc): scriptlet parsing, public interfaces, connection points - rewritten in fast-a patches 3080/3096 not yet on main.
- Proposal (merged from the drafters): Merge the fast-a patches; these escalations disappear.
- Dependencies: fast-a branch
- Effort: S (merge)   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement now: integrate fast-a 3080/3096, re-triage
- Examples: ActiveScriptSite_GetDocVersionString, global_GetNameSpaceParent, instance_cp_EnumConnections, instance_cpc_EnumConnectionPoints, parse_scriptlet_file, parse_scriptlet_public ...

### SH-INPUT: user32 input gaps (keyboard layout load/unload, touch/pointer injection, DisplayConfig, IME state, mouse-in-pointer)

- Items: 9   Subsystem: shell/Explorer   DLLs: user32, win32u
- What Windows does: Keyboard layout load/unload with flags, synthetic pointer devices (touch/pen injection), DisplayConfigSetDeviceInfo, IME state (WINNLS), mouse-in-pointer.
- Proposal (merged from the drafters): LoadKeyboardLayout/Unload: honour KLF_* and return HKLs per the test; DisplayConfigSetDeviceInfo: accept the documented request types for the primary output; synthetic pointer device: create a pointer device object feeding InjectSyntheticPointerInput.
- Dependencies: win32u input
- Effort: M   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement now for keyboard layouts (apps like Firefox hit them)
- Examples: CreateSyntheticPointerDevice, DisplayConfigSetDeviceInfo, LoadKeyboardLayoutEx, LoadKeyboardLayoutW, RegisterTouchHitTestingWindow, UnloadKeyboardLayout ...

### G-DMUSIC: DirectMusic style/chord-map/script/compose engines

- Items: 8   Subsystem: media   DLLs: dmcompos, dmscript, dmstyle
- What Windows does: DirectMusic styles/chord maps/script automation/composer.
- Proposal (merged from the drafters): Chord-map and script engines are large; formats are only partially documented ('prrf' RIFF). Implement what the docs spell out (chord map enumeration) and leave the composer.
- Dependencies: dmusic loader
- Effort: XL   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: won't do (obsolete, undocumented formats)
- Examples: ComposeSegmentFrom*, ComposeTransition, AutoTransition, ComposeTemplateFromShape, AutoImp* / ScriptSourceCodeLoader classes, create_unimpl_instance, chord map methods (round 4), pattern playback for style/motif/audition tracks, style_EnumChordMap ...

### M-CAPTURE: Media Foundation video capture, FrameStep, swapchain handle, mmdevapi capture, winegstreamer formats

- Items: 8   Subsystem: media   DLLs: evr, mf, mfmediaengine, mfplay, mmdevapi, winegstreamer
- What Windows does: Media Foundation capture and engine items: video capture sources (V4L2), media engine/player FrameStep, windowless swapchain handle, mmdevapi capture init gap, H.264 profile, evr fullscreen.
- Proposal (merged from the drafters): V4L2 capture source in winegstreamer (device enumeration + sample delivery) is the key piece - webcams in Teams/Zoom/browsers; FrameStep via seeking one frame; mmdevapi stream_init gap from capture tests.
- Dependencies: GStreamer v4l2src
- Effort: L   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement now for V4L2 video capture (high value, shared with browsers)
- Examples: video_presenter_control_SetFullscreen, MFCreateDeviceSource/MFEnumDeviceSources (video capture), media_engine_FrameStep, media_engine_GetVideoSwapchainHandle, media_player_FrameStep, stream_init ...

### K-CONALIAS: Console alias table (Add/GetConsoleAlias*)

- Items: 8   Subsystem: kernel/drivers   DLLs: kernelbase
- What Windows does: Console aliases: AddConsoleAlias / GetConsoleAlias* keep per-exe alias tables applied to ReadConsole input (doskey).
- Proposal (merged from the drafters): Per-console alias table in the console host (conhost.exe), applied in cooked ReadConsole; length/enumeration calls read it.
- Dependencies: conhost changes.
- Effort: M   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement now (documented, self-contained)
- Examples: AddConsoleAliasA, AddConsoleAliasW, GetConsoleAliasA, GetConsoleAliasExesLengthA, GetConsoleAliasExesLengthW, GetConsoleAliasW ...

### K-OBSEC: ntoskrnl object manager and security (Ob* handle resolution, SePrivilegeCheck, IoCreateDeviceSecure, PsImpersonateClient)

- Items: 8   Subsystem: kernel/drivers   DLLs: ntoskrnl.exe
- What Windows does: Kernel object manager and security: ObReferenceObjectByHandle (UserMode access check), SePrivilegeCheck, IoCreateDeviceSecure SDs, PsImpersonateClient.
- Proposal (merged from the drafters): Run real access checks against the requestor token; honour the SD of secure devices; implement impersonation through server token requests.
- Dependencies: wineserver tokens.
- Effort: M-L   Risk to existing behaviour: medium
- DECISIONS.md: DECISIONS 9 (security model: stays escalated)
- RECOMMENDATION: ESCALATED (DECISIONS 9): implement-later with SEC-TOKEN
- Examples: IoCreateDeviceSecure, ObOpenObjectByName, ObOpenObjectByPointer, ObReferenceObjectByHandle, ObReferenceObjectByName, PsImpersonateClient ...

### K-SETUPAPI: setupapi remote machine names, driver info lists, INF logical configs; CM eject

- Items: 8   Subsystem: kernel/drivers   DLLs: setupapi
- What Windows does: setupapi accepts a remote machine name, builds class driver lists (SPDIT_CLASSDRIVER), parses LogConf directives, ejects devices.
- Proposal (merged from the drafters): Machine name: accept local computer name, else ERROR_INVALID_MACHINENAME as documented; class driver lists enumerate INF files from the driver store; CM_Request_Device_Eject -> udisks/sg-admind eject.
- Dependencies: driver store INF index.
- Effort: M   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement now for local-name handling and eject; INF driver lists later
- Examples: CM_Request_Device_EjectA, CM_Request_Device_EjectW, SetupDiBuildDriverInfoList, SetupDiCreateDeviceInfoListExW, SetupDiEnumDriverInfoW, SetupDiGetClassDevsExW ...

### COM-WMI: WMI scripting and providers (wbemdisp sink/CIM_OBJECT, wbemprox SpawnDerivedClass, SystemRestore)

- Items: 8   Subsystem: COM/RPC   DLLs: wbemdisp, wbemprox
- What Windows does: WMI scripting: SWbemSink async, CIM_OBJECT put/get, PROPERTYPUTREF, SpawnDerivedClass, System Restore classes.
- Proposal (merged from the drafters): Merge fast-a patch 3093 for the sink; SpawnDerivedClass: derived class definitions kept in a dynamic table; SystemRestore classes tie to the Btrfs snapshot design.
- Dependencies: fast-a 3093; Btrfs rollback agent work
- Effort: L   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement later
- Examples: SWbemObject_Put_CIM_OBJECT, object_Invoke, services_ExecNotificationQueryAsync, sink_cp_EnumConnections, sink_cpc_EnumConnectionPoints, Win32_SystemRestore_enumeration ...

### G-D3D11OTHER: D3D11 other: WARP software device, D3D11On12, min-LOD, ReadFromSubresource

- Items: 7   Subsystem: graphics/D3D   DLLs: d3d11, d3dcompiler_43
- What Windows does: D3D11 leftovers: WARP software device creation, D3D11On12, per-resource min-LOD clamp, ReadFromSubresource, d3dcompiler linker/module.
- Proposal (merged from the drafters): WARP: map D3D_DRIVER_TYPE_WARP to a lavapipe-backed Vulkan device; SetResourceMinLOD: store and clamp in sampling; ReadFromSubresource: documented simple copy via map; D3D11On12 needs vkd3d interop; d3dcompiler_43 linker is exported by _47.
- Dependencies: lavapipe, vkd3d-proton
- Effort: L   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement now: ReadFromSubresource, MinLOD (S); WARP/On12 later
- Examples: D3D11On12CreateDevice, d3d11_create_device, d3d11_device_ReadFromSubresource, d3d11_device_context_GetResourceMinLOD, d3d11_device_context_SetResourceMinLOD, D3DCreateLinker ...

### G-DINPUT: DirectInput custom force, HID translate, HRTF APO, UI animation

- Items: 7   Subsystem: graphics/D3D   DLLs: dinput, dxdiag, dxtrans, hid, hrtfapo, uianimation
- What Windows does: DirectInput custom force effects, HID translate-to-scan-codes, HRTF APO, UI animation RepeatBetweenKeyframes, DX transforms, dxdiag UI.
- Proposal (merged from the drafters): RepeatBetweenKeyframes and HidP_TranslateUsages are documented algorithms (S); dxdiag shows a tabbed info dialog (UI, M); HRTF needs the spatial audio filter data (open HRTF dataset) (L); DXTransform is an IE filter engine (won't do).
- Dependencies: none
- Effort: M overall   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement now for uianimation + hid translate; dxdiag UI later; others won't do
- Examples: hid_joystick_effect_Initialize, hid_joystick_effect_SetParameters, wWinMain, DllGetClassObject, HidP_TranslateUsagesToI8042ScanCodes, CreateHrtfApo ...

### N-DNS: DNS dynamic update (RFC 2136) and mDNS/DNS-SD browse (dnsapi)

- Items: 7   Subsystem: networking   DLLs: dnsapi
- What Windows does: DNS dynamic update (RFC 2136 UPDATE messages) and DNS-SD browse over mDNS.
- Proposal (merged from the drafters): Dynamic update: SOA discovery + UPDATE PDU (admin/Security policy); mDNS browse: join 224.0.0.251, parse PTR/SRV/TXT per RFC 6762/6763 (open).
- Dependencies: none
- Effort: L   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement later: DnsServiceBrowse first (app discovery), dynamic update only for domain use
- Examples: DnsModifyRecordsInSet_A, DnsModifyRecordsInSet_UTF8, DnsModifyRecordsInSet_W, DnsReplaceRecordSetA, DnsReplaceRecordSetUTF8, DnsReplaceRecordSetW ...

### SH-DIALOGS: shell32 dialogs/UI: PickIconDlg, SHFormatDrive, SHMultiFileProperties, NetworkAddressControl

- Items: 7   Subsystem: shell/Explorer   DLLs: gamingtcui, rasdlg, shell32, wiaservc
- What Windows does: Shell dialogs: Change Icon (PickIconDlg), Format Drive, multi-file Properties, network address control, gaming profile card, RAS entry property sheet, WIA add-device dialog.
- Proposal (merged from the drafters): PickIconDlg and SHMultiFileProperties are Explorer-parity dialogs we own (sg-shell can draw them); SHFormatDrive via privileged mkfs through sg-admind with confirmation; netaddress control is a documented window class.
- Dependencies: sg-admind format verb; Explorer dialog set (see Recycle Bin / Explorer round 2).
- Effort: L (group of 6 dialogs, each M)   Risk to existing behaviour: low
- DECISIONS.md: DECISIONS 4 only covers WIA callbacks (not add-device dialog)
- RECOMMENDATION: implement later: PickIconDlg + multi-file Properties first (Windows-style Explorer dialogs)
- Examples: ShowProfileCardUI, RasEntryDlgW, InitNetworkAddressControl, PickIconDlg, SHFormatDrive, SHMultiFileProperties ...

### W-OTHER: Other WinRT (networking connectivity history, web HttpContent headers, geolocation extras)

- Items: 7   Subsystem: WinRT   DLLs: geolocation, windows.networking, windows.networking.connectivity, windows.web
- What Windows does: Other WinRT: connection profile history, HttpContent headers typing, geolocation satellite/venue data, transport info.
- Proposal (merged from the drafters): Typed headers and transport info are documented classes; connection history needs a store; GeoClue lacks satellite/venue.
- Dependencies: none
- Effort: M   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement now for HttpContent.Headers/TransportInformation; rest won't do
- Examples: Geocoordinate.SatelliteData, Geocoordinate.VenueData, SetSocketMediaStreamingMode, IConnectionProfileFilter2/3, connection_profile_QueryInterface, content_get_Headers ...

### K-PORTIO: x86 port I/O access (READ/WRITE_PORT_*, Ke386 IO maps)

- Items: 7   Subsystem: kernel/drivers   DLLs: hal, ntoskrnl.exe
- What Windows does: READ/WRITE_PORT_* and Ke386 IO maps give kernel code raw port access; from user mode this needs iopl/ioperm (root).
- Proposal (merged from the drafters): Allow only when sg-admind grants an ioperm helper (administrator); otherwise return the documented failure. Never run unconditionally.
- Dependencies: root helper.
- Effort: M   Risk to existing behaviour: medium (raw hardware access)
- DECISIONS.md: SECD-like
- RECOMMENDATION: won't do by default; implement admin-gated helper later if a legacy hardware tool needs it
- Examples: READ_PORT_UCHAR, READ_PORT_ULONG, WRITE_PORT_UCHAR, WRITE_PORT_ULONG, Ke386IoSetAccessProcess, Ke386QueryIoAccessMap ...

### DB-ADO: ADO / OLE DB for ODBC (msado15 OpenSchema/batch, msdasql MSDTC)

- Items: 7   Subsystem: other   DLLs: msado15, msdasql
- What Windows does: ADO Connection.OpenSchema, Recordset UpdateBatch/CancelBatch, msdasql MSDTC transaction join, column rowsets.
- Proposal (merged from the drafters): OpenSchema maps DBSCHEMA_* to ODBC catalog functions; batch updates are documented; MSDTC is a distributed transaction coordinator (no).
- Dependencies: ODBC
- Effort: L   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement later: OpenSchema/batch; MSDTC items won't do
- Examples: connection_OpenSchema, recordset_CancelBatch, recordset_UpdateBatch, column_rs_GetColumnsRowset, transaction_GetTransactionInfo, transjoin_GetOptionsObject ...

### M-DSHOW: DirectShow / quartz / qcap / qedit / WM Format SDK editor+writer

- Items: 7   Subsystem: media   DLLs: qcap, qedit, quartz, wmvcore
- What Windows does: DirectShow: RenderStream, SetRecompFormatFromSource, quartz IPersistPropertyBag/3D buffers, WMV/ASF encoder+muxer, WM backup/editor.
- Proposal (merged from the drafters): Mostly feature gaps with documented behaviours; encoder needs WMV/WMA encoders + ASF muxer (GStreamer).
- Dependencies: GStreamer encoders
- Effort: XL   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement later
- Examples: fnCaptureGraphBuilder2_RenderStream, timelinegrp_SetRecompFormatFromSource, VideoWindow_put_FullScreenMode FIXME, filtergraph/filtermapper FIXMEs, AddSourceFilter collections, remaining todo_wine in the quartz suites, WMCreateBackupRestorer, WMCreateEditor ...

### SH-MISC: shell32 misc (desktop SetNameOf, printers SetNameOf, SHCreateLinks, group subset count, drag helper, ExtractAssociatedIcon)

- Items: 7   Subsystem: shell/Explorer   DLLs: query, shell32
- What Windows does: shell32 items: desktop and printers folder SetNameOf, SHCreateLinks, group subset count, drag description text, ExtractAssociatedIconExW, IFilter lookup.
- Proposal (merged from the drafters): All documented: rename printer via SetPrinter, SHCreateLinks naming per docs, LVM_SETGROUPSUBSETCOUNT drawing, drag description text in the drag image, LoadIFilter reads HKCR PersistentHandler.
- Dependencies: none
- Effort: M overall   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement now (several are mis-filed small items)
- Examples: LoadIFilter, DragSourceHelper2_SetFlags, ExtractAssociatedIconExW, FolderView2_SetGroupSubsetCount, ISF_Desktop_fnSetNameOf, IShellFolder_Printers_fnSetNameOf ...

### RPC-MGMT: RPC management/error APIs (RpcErrorLoadErrorInfo, EpEltInq, cancel timeout, stop listening, stack size)

- Items: 7   Subsystem: COM/RPC   DLLs: rpcrt4
- What Windows does: RPC management and error-info APIs: RpcErrorStartEnumeration/LoadErrorInfo, RpcMgmtEpEltInqBegin, SetCancelTimeout, StopServerListening, SetServerStackSize, InqIfIds for remote bindings.
- Proposal (merged from the drafters): All documented: error info enumeration (RPC_EXTENDED_ERROR_INFO store per thread), endpoint-mapper enumeration over rpcss, cancel timeout stored per binding, StopServerListening honoured via the server loop.
- Dependencies: rpcss
- Effort: M   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement now (documented, many mis-filed small)
- Examples: RpcErrorLoadErrorInfo, RpcErrorStartEnumeration, RpcMgmtEpEltInqBegin, RpcMgmtInqIfIds, RpcMgmtSetCancelTimeout, RpcMgmtSetServerStackSize ...

### SH-PROPSTORE: shell32 property stores/descriptions and IFileOperation property application

- Items: 7   Subsystem: shell/Explorer   DLLs: shell32
- What Windows does: IFileOperation SetProperties/ApplyProperties, ShellItem2/ShellItemArray property stores and description lists, BHID_DataObject.
- Proposal (merged from the drafters): Back property writes by the file property handlers (sg-shell property store) with the documented IPropertyStore semantics.
- Dependencies: propsys property store
- Effort: M   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement later
- Examples: IShellItemArray_fnBindToHandler, IShellItemArray_fnGetPropertyDescriptionList, IShellItemArray_fnGetPropertyStore, ShellItem2_GetPropertyDescriptionList, file_operation_ApplyPropertiesToItem, file_operation_ApplyPropertiesToItems ...

### P-SDB: Application compatibility database (apphelp Sdb*, sdbinst)

- Items: 6   Subsystem: installers   DLLs: apphelp, sdbinst
- What Windows does: Application compatibility database: Sdb* APIs and sdbinst to register .sdb files.
- Proposal (merged from the drafters): Implement an SDB reader (format documented by Geoff Chappell/reactos-style open specs) and registration in the AppPatch registry keys.
- Dependencies: none
- Effort: L   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement later (installer shims)
- Examples: SdbCloseDatabase, SdbCreateDatabase, SdbGetFirstChild, SdbInitDatabase, SdbOpenDatabase, wmain

### SEC-XMLSIG: cryptxml (XML digital signatures)

- Items: 6   Subsystem: security/crypto   DLLs: cryptxml
- What Windows does: cryptxml (CNG XML signature API) parses a Signature element, verifies references and signature values.
- Proposal (merged from the drafters): Implement the documented CryptXml* API on top of msxml/xmllite canonicalization + bcrypt; verification uses certificate store lookups.
- Dependencies: xmllite C14N, bcrypt, SEC-CRYPT32.
- Effort: L   Risk to existing behaviour: low (new surface only)
- DECISIONS.md: DECISIONS 9 (security model: stays escalated)
- RECOMMENDATION: implement later: rarely used by consumer software
- Examples: CryptXmlGetDocContext, CryptXmlGetReference, CryptXmlGetSignature, CryptXmlGetStatus, CryptXmlOpenToDecode, CryptXmlVerifySignature

### G-TILED: D3D11 tiled resources

- Items: 6   Subsystem: graphics/D3D   DLLs: d3d11
- What Windows does: Tiled (reserved) resources: UpdateTileMappings, CopyTiles etc.
- Proposal (merged from the drafters): Needs sparse-resource support from Vulkan (sparseBinding) in wined3d.
- Dependencies: wined3d + Vulkan sparse
- Effort: XL   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: won't do for now (few titles)
- Examples: d3d11_device_context_CopyTileMappings, d3d11_device_context_CopyTiles, d3d11_device_context_ResizeTilePool, d3d11_device_context_TiledResourceBarrier, d3d11_device_context_UpdateTileMappings, d3d11_device_context_UpdateTiles

### G-ICM: Colour management (mscms PS2 CRD/CSA, ICM registry keys, ColorMatchToTarget, SetupColorMatching)

- Items: 6   Subsystem: graphics/D3D   DLLs: gdi32, icmui, mscms
- What Windows does: Colour management: PostScript CRD/CSA from ICC, ICM registry key updates, ColorMatchToTarget, colour-matching setup dialog.
- Proposal (merged from the drafters): Generate PS2 CSA/CRD per ICC spec ('Appendix' tables) via lcms; ICM key updates done in patch 5223; dialog is UI work.
- Dependencies: lcms2
- Effort: M   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement later (print/prepress software)
- Examples: ColorMatchToTarget, UpdateICMRegKeyA, UpdateICMRegKeyW, SetupColorMatchingW, GetPS2ColorRenderingDictionary, GetPS2ColorSpaceArray

### K-MOUNT: Volume mount points (Find/Set/DeleteVolumeMountPoint)

- Items: 6   Subsystem: kernel/drivers   DLLs: kernel32
- What Windows does: Volume mount points (mounted folders): FindFirstVolumeMountPoint, Set/DeleteVolumeMountPoint (admin).
- Proposal (merged from the drafters): Map mount points to bind-mount style reparse entries kept in the prefix; Set/Delete admin-only via sg-admind; Find* enumerate them.
- Dependencies: sg-admind mount verb; volume GUID names (exist).
- Effort: M   Risk to existing behaviour: low
- DECISIONS.md: DECISIONS 6 pattern for admin gating
- RECOMMENDATION: implement later
- Examples: DeleteVolumeMountPointA, FindFirstVolumeMountPointA, FindFirstVolumeMountPointW, FindVolumeMountPointClose, SetVolumeMountPointA, SetVolumeMountPointW

### W-MEDIA: Windows.Media.* (SystemMediaTransportControls, audio state monitor, devices events)

- Items: 6   Subsystem: WinRT   DLLs: windows.media.devices, windows.media.mediacontrol
- What Windows does: Windows.Media.* : SystemMediaTransportControls display updater, AudioStateMonitor, default-audio-device change events.
- Proposal (merged from the drafters): Needs windows.media.audio IDL definitions (reimplement from docs); device change events via IMMNotificationClient.
- Dependencies: IDLs
- Effort: M   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement later
- Examples: DefaultAudioCapture/RenderDeviceChanged events, default device change events (agent note 3), AudioStateMonitor creators, DisplayUpdater VideoProperties/ImageProperties/CopyFromFileAsync, AudioStateMonitor exports (agent note 1), VideoProperties/ImageProperties/CopyFromFileAsync (agent note 2), display_updater_CopyFromFileAsync

### N-DOMAIN: DC locator / domain join / directory pickers (DsGetDcName, DsObjectPicker, adsldp, wldap32 mTLS)

- Items: 5   Subsystem: networking   DLLs: adsldp, netapi32, objsel, wldap32
- What Windows does: DC locator (DsGetDcName), domain join, object picker, directory searches, LDAP mTLS client cert, ADSI search preferences.
- Proposal (merged from the drafters): DsGetDcName: DNS SRV (_ldap._tcp.dc._msdcs.<domain>) lookup returning DOMAIN_CONTROLLER_INFO with documented flags (works with a real AD when joined); object picker queries local accounts via netapi; wldap32 client certificate via the TLS backend; no domain join logic yet.
- Dependencies: DNS SRV (dnsapi), DECISIONS 7 accounts, Kerberos (SEC-LOGON).
- Effort: XL (full domain support)   Risk to existing behaviour: low for locator
- DECISIONS.md: DECISIONS 7 only covers local accounts
- RECOMMENDATION: implement later: DsGetDcName + object picker (local scope) first
- Examples: search_SetSearchPreference, DsGetDcNameW, OBJSEL_IDsObjectPicker_Initialize, OBJSEL_IDsObjectPicker_InvokeDialog, WLDAP32_ldap_connect

### SEC-EFS: Encrypted File System raw backup API

- Items: 5   Subsystem: security/crypto   DLLs: advapi32
- What Windows does: EFS raw backup/restore API (OpenEncryptedFileRaw, Read/Write/Close) exchanges $EFS streams of encrypted NTFS files.
- Proposal (merged from the drafters): Files are never EFS-encrypted here, so Open returns ERROR_FILE_ENCRYPTED/ERROR_INVALID_PARAMETER as documented for non-encrypted files; implementing real EFS is out of scope.
- Dependencies: none.
- Effort: S   Risk to existing behaviour: low
- DECISIONS.md: DECISIONS 9 (security model: stays escalated)
- RECOMMENDATION: won't do beyond the documented error answers (A -> W conversion is S)
- Examples: CloseEncryptedFileRaw, OpenEncryptedFileRawA, OpenEncryptedFileRawW, ReadEncryptedFileRaw, WriteEncryptedFileRaw

### SEC-CRYPTOPOL: Crypto policy: FIPS mode, schannel attribute reporting, NCryptExportKey options

- Items: 5   Subsystem: security/crypto   DLLs: bcrypt, ncrypt, secur32
- What Windows does: BCryptGetFipsAlgorithmMode reads the FIPS policy; schannel reports SECPKG_ATTR_CIPHER_STRENGTHS from the real provider; NCryptExportKey honours NCRYPTBUFFER_PKCS_ALG_* encryption choices.
- Proposal (merged from the drafters): Read HKLM\...\Lsa\FipsAlgorithmPolicy for FIPS mode (default off) and enforce it; report the real strengths of the TLS backend; add PKCS#8 encrypted export options to NCryptExportKey.
- Dependencies: gnutls/crypto backend.
- Effort: M   Risk to existing behaviour: low-medium
- DECISIONS.md: DECISIONS 9 (security model: stays escalated)
- RECOMMENDATION: ESCALATED (DECISIONS 9): implement-later; FIPS read is S
- Examples: BCryptGetFipsAlgorithmMode, NCryptExportKey, schan_QueryCredentialsAttributes, schan_QueryCredentialsAttributesA, schan_QueryCredentialsAttributesW

### G-D3DX: d3dx9: fragment linker, tangent frame options, pixel formats

- Items: 5   Subsystem: graphics/D3D   DLLs: d3dx9_36
- What Windows does: d3dx9: fragment linker, tangent-frame options, pixel-format conversions.
- Proposal (merged from the drafters): Finish option handling per D3DX docs; BT.601 guess flagged in patch 3643.
- Dependencies: none
- Effort: M   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement later
- Examples: D3DXComputeTangentFrameEx, d3dx9_fragment_linker_LinkPixelShader, d3dx9_fragment_linker_LinkShader, d3dx9_fragment_linker_LinkVertexShader, d3dx_load_pixels_from_pixels

### K-IRQL: ntoskrnl/hal IRQL model (KfRaiseIrql, KfLowerIrql, KeGetCurrentIrql)

- Items: 5   Subsystem: kernel/drivers   DLLs: hal, ntoskrnl.exe
- What Windows does: IRQL is per-processor state; drivers raise/lower it around spin locks.
- Proposal (merged from the drafters): Track a per-thread IRQL value in ntoskrnl (KeGetCurrentIrql/KfRaise/KfLower with the documented bugcheck-style checks only logged).
- Dependencies: ntoskrnl thread state.
- Effort: S   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement now (tiny, documented)
- Examples: KeGetCurrentIrql, KfLowerIrql, KfRaiseIrql, KeLowerIrql, KfRaiseIrql

### K-NETDRV: Network kernel drivers: ndis.sys protocol binding, netio WSK, http.sys TLS, nsiproxy

- Items: 5   Subsystem: kernel/drivers   DLLs: http.sys, httpapi, ndis.sys, netio.sys
- What Windows does: ndis.sys protocol drivers, netio WSK extras, http.sys URL registration with TLS, nsiproxy.
- Proposal (merged from the drafters): http.sys TLS: terminate TLS in the http.sys implementation using the system TLS backend and the certificate binding store (also needed by httpapi); ndis/WSK: drivers for real network stacks are out of scope.
- Dependencies: TLS backend; SEC-CRYPTOPOL.
- Effort: L (http.sys TLS) / XL (ndis)   Risk to existing behaviour: medium
- DECISIONS.md: none
- RECOMMENDATION: implement later for http.sys TLS (HttpSetServiceConfiguration SSL); ndis protocol drivers won't do
- Examples: http_add_url, HttpSetServiceConfiguration, NdisRegisterProtocol, wsk_control_client, wsk_inspect_complete

### G-WIC: WIC: fast metadata encoder, CreateImageEncoder, TIFF planar, extension codecs

- Items: 5   Subsystem: graphics/D3D   DLLs: windowscodecs, windowscodecsext
- What Windows does: WIC: in-place fast metadata encoder, CreateImageEncoder, TIFF planar >8 bit, extension codecs (HEIF/WebP/RAW are Store packages).
- Proposal (merged from the drafters): Fast metadata encoder needs per-format padding writers; TIFF planar is a decoder fix (S-M); extension codecs return the documented CLASS_E_CLASSNOTAVAILABLE.
- Dependencies: libtiff
- Effort: M   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement later (TIFF planar now)
- Examples: ImagingFactory_CreateFastMetadataEncoderFromDecoder, ImagingFactory_CreateFastMetadataEncoderFromFrameDecode, ImagingFactory_CreateImageEncoder, tiff_get_decode_info, DllGetClassObject

### K-USBBUS: wineusb URBs and winebus SDL fallback

- Items: 5   Subsystem: kernel/drivers   DLLs: winebus.sys, wineusb.sys
- What Windows does: wineusb URB types (isochronous, status, feature, reset) and winebus SDL fallback when not built.
- Proposal (merged from the drafters): Add remaining URB functions via libusb; SDL fallback stays a build-time signal.
- Dependencies: libusb
- Effort: M   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement later (devices rarely need isochronous)
- Examples: sdl_bus_init, sdl_bus_stop, sdl_bus_wait, driver_internal_ioctl, usb_submit_urb

### G-WINED3D-VK: wined3d Vulkan renderer (not the shipped renderer)

- Items: 5   Subsystem: graphics/D3D   DLLs: wined3d
- What Windows does: wined3d Vulkan renderer paths (not the shipped renderer).
- Proposal (merged from the drafters): Complete only if the Vulkan renderer becomes default; DXVK is the shipped D3D9-11 path.
- Dependencies: none
- Effort: XL   Risk to existing behaviour: none
- DECISIONS.md: none
- RECOMMENDATION: won't do (DXVK ships)
- Examples: wined3d_context_vk_bind_stream_output_buffers, wined3d_render_target_view_vk_cs_init, wined3d_shader_descriptor_writes_vk_add_uav_counter_write, wined3d_shader_descriptor_writes_vk_add_uav_write, wined3d_texture_vk_load_location

### G-SO: Stream output: DrawAuto, SO offset append, multi-stream

- Items: 4   Subsystem: graphics/D3D   DLLs: d3d11, wined3d
- What Windows does: Stream output with DrawAuto and SO_BUFFER_OFFSET_APPEND; multi-stream GS output.
- Proposal (merged from the drafters): Track written bytes per SO buffer via query results (transform feedback) and implement DrawAuto reading it; multi-stream needs GL/VK multi-stream support.
- Dependencies: wined3d GL transform feedback / VK_EXT_transform_feedback
- Effort: L   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement later
- Examples: d3d10_device_DrawAuto, d3d11_device_context_DrawAuto, validate_stream_output_entries, state_so

### G-DCOMP: DirectComposition: surfaces from handle/hwnd, CreateDevice3, SetRenderingDevice, interop

- Items: 4   Subsystem: graphics/D3D   DLLs: dcomp
- What Windows does: DirectComposition surfaces from hwnd/handle for browser video overlays, Device3, SetRenderingDevice, interop.
- Proposal (merged from the drafters): Finish MISSING parts: live (not one-shot) hwnd content, swapchain_surface producers, handle surface sharing.
- Dependencies: dcomp swapchain_surface
- Effort: M   Risk to existing behaviour: low
- DECISIONS.md: DECISIONS 2 (implement) already covers CreateSurfaceFromHandle/Hwnd
- RECOMMENDATION: implement now (decision exists; finish the MISSING parts)
- Examples: DCompositionCreateDevice3, device_CreateSurfaceFromHandle, device_CreateSurfaceFromHwnd, device_interop_SetRenderingDevice

### G-DWRITE: DirectWrite: colour glyph formats, justification, HTTP font loader, WOFF2

- Items: 4   Subsystem: graphics/D3D   DLLs: dwrite
- What Windows does: DirectWrite: colour glyph formats (SVG, sbix/CBDT), justified alignment, HTTP font loader, WOFF2 unpack.
- Proposal (merged from the drafters): Justification is a documented layout step (S-M); WOFF2 needs libbrotli dependency (packaging decision); colour glyph formats via freetype with PNG; HTTP loader via wininet.
- Dependencies: libbrotli, freetype png
- Effort: M   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement now for justification; rest later
- Examples: dwritefactory5_CreateHttpFontFileLoader, dwritefactory5_UnpackFontFile, freetype_get_aa_glyph_bitmap, layout_apply_text_alignment

### G-USP10: Uniscribe OpenType features, tab stops, EUDC

- Items: 4   Subsystem: graphics/D3D   DLLs: gdi32
- What Windows does: Uniscribe OpenType features per range (ScriptShape/PlaceOpenType ranges), tab stops, EUDC.
- Proposal (merged from the drafters): Feed range properties into the existing GSUB/GPOS code; EnableEUDC toggles the EUDC font link.
- Dependencies: usp10 OpenType code
- Effort: M   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement later
- Examples: EnableEUDC, ScriptPlaceOpenType, ScriptShapeOpenType, XFORMOBJ_iGetXform

### COM-OLELINK: OLE linking (OleCreateLink*, linked objects in streams)

- Items: 4   Subsystem: COM/RPC   DLLs: ole32
- What Windows does: OLE linking (IOleLink) for OleCreateLink, OleCreateLinkToFile, linked objects in \1Ole streams - used by Office documents with linked objects.
- Proposal (merged from the drafters): Implement the OLE link object (moniker binding, IOleLink, update on open) in ole32.
- Dependencies: monikers (exist)
- Effort: L   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement later (Office/WordPad interop)
- Examples: OleCreateLink, OleCreateLinkFromData, OleCreateLinkToFile, load_ole_stream

### COM-STORAGE: Structured storage: SWMR direct writer lock, transacted storage flags

- Items: 4   Subsystem: COM/RPC   DLLs: ole32
- What Windows does: Structured storage: IDirectWriterLock (SWMR) for multi-process access, STGM_NOSCRATCH/NOSNAPSHOT.
- Proposal (merged from the drafters): Cross-process SWMR lock via named mutex + lock bytes protocol; honour NOSCRATCH/NOSNAPSHOT.
- Dependencies: none
- Effort: M   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement later
- Examples: Storage_ConstructTransacted, directwriterlock_HaveWriteAccess, directwriterlock_ReleaseWriteAccess, directwriterlock_WaitForWriteAccess

### RPC-SEC: RPC runtime security: authorization fn, auth info inquiry, client privileges

- Items: 4   Subsystem: COM/RPC   DLLs: rpcrt4
- What Windows does: RPC runtime security: RpcMgmtSetAuthorizationFn, RpcBindingInqAuthInfoEx with SecurityQOS, RpcBindingInqAuthClient privileges.
- Proposal (merged from the drafters): Store the authorization callback and call it on each incoming call; return the real auth info/privileges from the call's token.
- Dependencies: wineserver token / security context
- Effort: M   Risk to existing behaviour: medium
- DECISIONS.md: DECISIONS 9 (security model: stays escalated)
- RECOMMENDATION: ESCALATED (DECISIONS 9): implement-later with SEC-TOKEN
- Examples: RPCRT4_default_inquire_auth_client, RpcBindingInqAuthInfoExW, RpcMgmtSetAuthorizationFn, rpcrt4_ncalrpc_inquire_auth_client

### N-WEBSERVICES: WS-* stack (webservices: other transports/templates, WS-Security) and WS-Discovery (wsdapi)

- Items: 4   Subsystem: networking   DLLs: webservices, wsdapi
- What Windows does: WS-* stack: remaining service-proxy templates, WS-Security, timespan type text, WS-Discovery client (WSDCreateDiscoveryProvider).
- Proposal (merged from the drafters): Implement the 11 other templates incl. WS-Security (username token, X.509 signing via crypt32), WSD Probe/Resolve over UDP multicast (open WS-Discovery spec).
- Dependencies: crypt32; wininet
- Effort: XL   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement later: WS-Security as one decision with the templates
- Examples: WsCreateServiceProxyFromTemplate, read_type, write_type, WSDCreateDiscoveryProvider

### W-SPEECH: Windows.Media.Speech recognition/synthesis engines and SAPI

- Items: 4   Subsystem: WinRT   DLLs: windows.media.speech
- What Windows does: Windows.Media.SpeechRecognition / SpeechSynthesis engines: recognition, voices.
- Proposal (merged from the drafters): Back synthesis by a Linux TTS (espeak-ng/piper) and recognition by the Parakeet model already shipped for voice typing.
- Dependencies: Voice typing stack
- Effort: L   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement later
- Examples: SystemSpeechLanguage / TrySetSystemSpeechLanguageAsync, recognizer_factory_Create without a capture device, speech recognition engine, speech synthesis voices

### K-PERF: Performance counters and WMI providers in advapi32 (PerfAddCounters, WmiOpenBlock, SetServiceBits)

- Items: 3   Subsystem: other   DLLs: advapi32
- What Windows does: PerfAddCounters (perf consumer), WmiOpenBlock, SetServiceBits.
- Proposal (merged from the drafters): PerfAddCounters: validate sizes per the test; WmiOpenBlock: return ERROR_WMI_GUID_NOT_FOUND (no providers) instead of success; SetServiceBits stores bits after patch 5008 validation.
- Dependencies: none
- Effort: S   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement now (mis-filed small)
- Examples: PerfAddCounters, SetServiceBits, WmiOpenBlock

### SYS-AWE: Address Windowing Extensions (Allocate/Map/FreeUserPhysicalPages)

- Items: 3   Subsystem: kernel/drivers   DLLs: kernelbase
- What Windows does: AWE (AllocateUserPhysicalPages etc.) needs SeLockMemoryPrivilege and returns physical page frames to map into a window.
- Proposal (merged from the drafters): Emulate with anonymous shared memory: check SeLockMemoryPrivilege, allocate page frame numbers backed by memfd, MapUserPhysicalPages maps them; fix the MapUserPhysicalPages bug noted.
- Dependencies: ntdll virtual memory.
- Effort: M   Risk to existing behaviour: low
- DECISIONS.md: DECISIONS 9 (security model: stays escalated)
- RECOMMENDATION: implement later (only SQL Server-class software uses it)
- Examples: AllocateUserPhysicalPages, FreeUserPhysicalPages, MapUserPhysicalPages

### G-EOT: Embedded OpenType fonts (t2embed TTLoadEmbeddedFont / TTEmbedFont)

- Items: 3   Subsystem: graphics/D3D   DLLs: t2embed
- What Windows does: Embedded OpenType fonts (TTLoadEmbeddedFont, TTEmbedFont): MicroType Express decompression, XOR obfuscation.
- Proposal (merged from the drafters): MTX decoder is specified by the W3C EOT submission (open); implement load first, embed (subsetting+MTX encoder) later.
- Dependencies: freetype
- Effort: L   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement later (IE/Office legacy documents)
- Examples: TTDeleteEmbeddedFont, TTEmbedFont, TTLoadEmbeddedFont

### N-WEBSOCKET: WebSocket protocol API (websocket.dll)

- Items: 3   Subsystem: networking   DLLs: websocket
- What Windows does: websocket.dll WebSocket protocol state machine API (handle, handshake, action queue).
- Proposal (merged from the drafters): Implement RFC 6455 framing/handshake state machine (open spec) - used by HTTP.sys/IIS and some .NET HttpListener websocket code.
- Dependencies: none
- Effort: L   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement later (WinHTTP's own websocket exists)
- Examples: WebSocketAbortHandle, WebSocketCreateClientHandle, WebSocketDeleteHandle

### G-PSDRV: wineps PostScript driver: clip-exclusive, CFF outlines

- Items: 3   Subsystem: printing   DLLs: wineps.drv
- What Windows does: PostScript driver: CLIP_EXCLUSIVE clipping, CFF OpenType download, Type 42 conversion.
- Proposal (merged from the drafters): Implement clip exclusion with even-odd path; CFF via Type 1 conversion from freetype.
- Dependencies: none
- Effort: M   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement later
- Examples: ControlPrintProcessor, PSDRV_ExtEscape, get_glyf_pos

### G-X11: winex11 Configure/Gravity notify in virtual desktop, XIM preedit caret

- Items: 3   Subsystem: graphics/D3D   DLLs: winex11.drv
- What Windows does: winex11: synthetic ConfigureNotify mapping in virtual desktop, XIM preedit caret.
- Proposal (merged from the drafters): Map coordinates through desktop offset; XIM caret needs an input-method server to test.
- Dependencies: none
- Effort: S   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement now for ConfigureNotify mapping; XIM when testable
- Examples: X11DRV_ConfigureNotify, X11DRV_GravityNotify, xic_preedit_caret

### SEC-SCARD: Smart-card API (winscard) started-event and card database

- Items: 3   Subsystem: security/crypto   DLLs: winscard
- What Windows does: winscard SCardListCards / started-event objects talk to the smart-card resource manager.
- Proposal (merged from the drafters): Map to pcsc-lite when present: SCardAccessStartedEvent returns a manual-reset event signalled when pcscd runs; ListCards reads the card database in the registry.
- Dependencies: pcsc-lite (Recommends).
- Effort: M   Risk to existing behaviour: low
- DECISIONS.md: DECISIONS 9 (security model: stays escalated)
- RECOMMENDATION: implement later
- Examples: SCardAccessStartedEvent, SCardListCardsA, SCardReleaseStartedEvent

### P-CAB: cabinet XPRESS/LZMS compression, FDITruncateCabinet

- Items: 2   Subsystem: installers   DLLs: cabinet
- What Windows does: cabinet: XPRESS/XPRESS Huffman/LZMS Compress API, FDITruncateCabinet.
- Proposal (merged from the drafters): XPRESS is published ([MS-XCA] open spec); implement; LZMS later; FDITruncateCabinet is documented.
- Dependencies: none
- Effort: M   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement now for XPRESS and FDITruncateCabinet
- Examples: FDITruncateCabinet, create_handle

### CLOUD: Cloud Files API (cldapi) remaining pieces

- Items: 2   Subsystem: shell/Explorer   DLLs: cldapi
- What Windows does: Cloud Files API remaining pieces: oplock open, connect callbacks.
- Proposal (merged from the drafters): Per DECISIONS 1: user-mode API only; oplock can be advisory.
- Dependencies: none
- Effort: S   Risk to existing behaviour: low
- DECISIONS.md: DECISIONS 1
- RECOMMENDATION: implement now (decision exists)
- Examples: CfConnectSyncRoot, CfOpenFileWithOplock

### G-DDRAW: ddraw flip/texture surfaces and DuplicateSurface

- Items: 2   Subsystem: graphics/D3D   DLLs: ddraw
- What Windows does: ddraw: flippable texture surfaces, DuplicateSurface shared memory.
- Proposal (merged from the drafters): Allow DDSCAPS_FLIP|TEXTURE creation (documented); DuplicateSurface shares the sysmem pointer.
- Dependencies: none
- Effort: S-M   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement now (documented, small)
- Examples: ddraw7_DuplicateSurface, ddraw_surface_create

### SH-ADESK: Active Desktop items (AddDesktopItemWithUI, AddUrl)

- Items: 2   Subsystem: shell/Explorer   DLLs: shell32
- What Windows does: Active Desktop AddDesktopItemWithUI / AddUrl show the add-item dialog and persist the item.
- Proposal (merged from the drafters): Implement the documented confirmation dialog then call the existing list/persist code (gate fastb-adesk).
- Dependencies: none
- Effort: S   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement now (dialog only)
- Examples: active_desktop_AddDesktopItemWithUI, active_desktop_AddUrl

### SYS-INPUT: BlockInput / SetSystemCursor fake successes

- Items: 2   Subsystem: shell/Explorer   DLLs: user32
- What Windows does: BlockInput blocks keyboard/mouse input system-wide (needs the input desktop, privilege-gated); SetSystemCursor replaces a system cursor.
- Proposal (merged from the drafters): BlockInput: window-station wide flag consulted by the input path, cleared on process exit; SetSystemCursor: replace the cursor in the theme cursor table.
- Dependencies: win32u input path; compositor cursor theme.
- Effort: S-M   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: implement now (documented, small)
- Examples: BlockInput, SetSystemCursor

### SH-DEFAPP: Default-app registration (SetAppAsDefault)

- Items: 1   Subsystem: shell/Explorer   DLLs: shell32
- What Windows does: IApplicationAssociationRegistration::SetAppAsDefault since Windows 8 may not silently change defaults.
- Proposal (merged from the drafters): Per DECISIONS 5: duplicate Windows 10/11, keep our Default apps settings untouched.
- Dependencies: none
- Effort: S   Risk to existing behaviour: low
- DECISIONS.md: DECISIONS 5
- RECOMMENDATION: implement now (decision made); PARTIAL work already landed - finish remaining documented returns
- Examples: ApplicationAssociationRegistration_SetAppAsDefault

### SH-DPI: Per-process system DPI (NtUserGetSystemDpiForProcess)

- Items: 1   Subsystem: shell/Explorer   DLLs: win32u
- What Windows does: Per-process system DPI (GetSystemDpiForProcess) returns the DPI of the process's awareness context.
- Proposal (merged from the drafters): Add per-process DPI awareness to wineserver (process info) so GetSystemDpiForProcess, SetProcessDpiAwarenessContext and scaling agree; needs a server protocol change.
- Dependencies: wineserver protocol
- Effort: M   Risk to existing behaviour: medium (scaling regressions)
- DECISIONS.md: none
- RECOMMENDATION: implement later: bundle with the per-process DPI awareness work
- Examples: NtUserGetSystemDpiForProcess

### SEC-EDP: Windows.Security.EnterpriseData (ProtectionPolicyManager)

- Items: 1   Subsystem: WinRT   DLLs: windows.security.enterprisedata
- What Windows does: Enterprise data protection policy objects (ProtectionPolicyManager).
- Proposal (merged from the drafters): Constructible objects returning the documented 'not enrolled' state.
- Dependencies: none
- Effort: S   Risk to existing behaviour: low
- DECISIONS.md: none
- RECOMMENDATION: won't do: enterprise MDM feature
- Examples: factory_ActivateInstance

## 3. Every escalated item

Columns: group | dll!function | code location (first source) | sources as file.tsv:line[verdict]. Source file names: fast-a, fast-b, fast-a-cloud, recheck-fast-a are in /var/tmp/stubs-common/triage/; triage-5/6/6b/6c/7 are docs/stubs/ on branches stubs-5, stubs-6, stubs-6b, stubs-6c and claude/wine-sg-stub-drafter-7-cbbvkg (copies in /var/tmp/stubs-agent/esc/). Row marker '*' = another source disagrees (see the verdicts).

| group | item | code | sources |
|---|---|---|---|
| CLOUD | cldapi!CfConnectSyncRoot | dlls/cldapi/main.c:75 | fast-b.tsv:2593[escalate] |
| CLOUD | cldapi!CfOpenFileWithOplock | dlls/cldapi/main.c:183 | fast-b.tsv:2594[escalate] |
| COM-DCOM | combase!CoDecodeProxy | dlls/combase/rpc.c:2295 | triage-6.tsv:563[escalate] |
| COM-DCOM | combase!CoGetCallerTID | dlls/combase/combase.c:3622 | triage-6.tsv:21[escalate] |
| COM-DCOM | combase!CoIsHandlerConnected | dlls/combase/combase.c:3949 | triage-6.tsv:916[escalate] |
| COM-DCOM | combase!CoRegisterSurrogate | dlls/combase/combase.c:4016 | triage-6.tsv:919[escalate] |
| COM-DCOM | combase!DllDebugObjectRPCHook | dlls/combase/rpc.c:2285 | triage-6.tsv:114[escalate] |
| COM-DCOM | combase!RemUnknown_RemAddRef | dlls/combase/stubmanager.c:761 | triage-6.tsv:110[escalate] |
| COM-DCOM | combase!RemUnknown_RemRelease | dlls/combase/stubmanager.c:794 | triage-6.tsv:111[escalate] |
| COM-DCOM | combase!StdMarshalImpl_ReleaseMarshalData | dlls/combase/marshal.c:2336 | triage-6.tsv:565[escalate] |
| COM-DCOM | combase!StdMarshalImpl_UnmarshalInterface | dlls/combase/marshal.c:2307 | triage-6.tsv:564[escalate] |
| COM-DCOM | combase!get_unmarshaler_from_stream | dlls/combase/marshal.c:577 | triage-6.tsv:115[escalate] |
| COM-DCOM | combase!rpc_execute_call | dlls/combase/rpc.c:1948 | triage-6.tsv:113[escalate] |
| COM-MAIL | cdosys!DllGetClassObject | dlls/cdosys/main.c:36 | triage-7.tsv:270[escalate] |
| COM-MAIL | inetcomm!MimeMessage_CreateWebPage | dlls/inetcomm/mimeole.c:4037 | fast-a.tsv:665[escalate] |
| COM-MAIL | inetcomm!MimeMessage_GetRootMoniker | dlls/inetcomm/mimeole.c:4302 | fast-a.tsv:667[escalate] |
| COM-MAIL | inetcomm!MimeMessage_SplitMessage | dlls/inetcomm/mimeole.c:4293 | fast-a.tsv:666[escalate] |
| COM-MAIL | inetcomm!MimeSecurity_DecodeBody | dlls/inetcomm/mimeole.c:4543 | fast-a.tsv:632[escalate] |
| COM-MAIL | inetcomm!MimeSecurity_DecodeMessage | dlls/inetcomm/mimeole.c:4534 | fast-a.tsv:631[escalate] |
| COM-MAIL | inetcomm!MimeSecurity_EncodeBody | dlls/inetcomm/mimeole.c:4524 | fast-a.tsv:630[escalate] |
| COM-MAIL | inetcomm!MimeSecurity_EncodeMessage | dlls/inetcomm/mimeole.c:4515 | fast-a.tsv:629[escalate] |
| COM-MAIL | mapi32!MAPIAdminProfiles | dlls/mapi32/mapi32_main.c:204 | fast-a.tsv:634[escalate] |
| COM-MAIL | mapi32!MAPIOpenLocalFormContainer | dlls/mapi32/mapi32_main.c:179 | fast-a.tsv:633[escalate] |
| COM-MAIL* | winemapi!MAPIAddress | dlls/winemapi/main.c:35 | fast-a-cloud.tsv:50[escalate]; triage-7.tsv:291[windows-matches] |
| COM-MAIL* | winemapi!MAPIDetails | dlls/winemapi/main.c:50 | fast-a-cloud.tsv:51[escalate]; triage-7.tsv:292[windows-matches] |
| COM-OLELINK | ole32!OleCreateLink | dlls/ole32/ole2stubs.c:50 | triage-6.tsv:337[escalate] |
| COM-OLELINK | ole32!OleCreateLinkFromData | dlls/ole32/ole2.c:2919 | triage-6.tsv:357[escalate] |
| COM-OLELINK | ole32!OleCreateLinkToFile | dlls/ole32/ole2stubs.c:39 | triage-6.tsv:336[escalate] |
| COM-OLELINK | ole32!load_ole_stream | dlls/ole32/defaulthandler.c:1766 | triage-6.tsv:343[escalate] |
| COM-SCRIPT | dhtmled.ocx!DHTMLEdit_ExecCommand | dlls/dhtmled.ocx/edit.c:543 | fast-a.tsv:603[escalate] |
| COM-SCRIPT | dhtmled.ocx!DHTMLEdit_PrintDocument | dlls/dhtmled.ocx/edit.c:1019 | fast-a.tsv:606[escalate] |
| COM-SCRIPT | dhtmled.ocx!DHTMLEdit_QueryStatus | dlls/dhtmled.ocx/edit.c:551 | fast-a.tsv:604[escalate] |
| COM-SCRIPT | dhtmled.ocx!DHTMLEdit_SetContextMenu | dlls/dhtmled.ocx/edit.c:558 | fast-a.tsv:605[escalate] |
| COM-SCRIPT | dhtmled.ocx!OleInPlaceObjectWindowless_GetDropTarget | dlls/dhtmled.ocx/edit.c:1854 | fast-a.tsv:662[escalate] |
| COM-SCRIPT | dhtmled.ocx!ViewObjectEx_Draw | dlls/dhtmled.ocx/edit.c:1638 | fast-a.tsv:661[escalate] |
| COM-SCRIPT | jscript!Object_defineGetter | dlls/jscript/object.c:321 | fast-a.tsv:298[escalate] |
| COM-SCRIPT | jscript!Object_defineSetter | dlls/jscript/object.c:364 | fast-a.tsv:299[escalate] |
| COM-SCRIPT | msscript.ocx!PersistStreamInit_GetSizeMax | dlls/msscript.ocx/msscript.c | fast-b.tsv:2204[escalate] |
| COM-SCRIPT | msscript.ocx!PersistStreamInit_Load | dlls/msscript.ocx/msscript.c | fast-b.tsv:2202[escalate] |
| COM-SCRIPT | msscript.ocx!PersistStreamInit_Save | dlls/msscript.ocx/msscript.c | fast-b.tsv:2203[escalate] |
| COM-SCRIPT | msscript.ocx!ScriptControl_put_Timeout |  | fast-b.tsv:2201[escalate] |
| COM-SCRIPT | msscript.ocx!ViewObject_Draw | dlls/msscript.ocx/msscript.c | fast-b.tsv:2206[escalate] |
| COM-SCRIPT | vbscript!VBScriptDebug_EnumCodeContextsOfPosition | dlls/vbscript/vbscript.c:990 | fast-a.tsv:557[escalate] |
| COM-STORAGE | ole32!Storage_ConstructTransacted | dlls/ole32/storage32.c:6846 | triage-6.tsv:349[escalate] |
| COM-STORAGE | ole32!directwriterlock_HaveWriteAccess | dlls/ole32/storage32.c:475 | triage-6.tsv:347[escalate] |
| COM-STORAGE | ole32!directwriterlock_ReleaseWriteAccess | dlls/ole32/storage32.c:468 | triage-6.tsv:346[escalate] |
| COM-STORAGE | ole32!directwriterlock_WaitForWriteAccess | dlls/ole32/storage32.c:461 | triage-6.tsv:345[escalate] |
| COM-WMI | wbemdisp!SWbemObject_Put_CIM_OBJECT | dlls/wbemdisp/object.c | fast-b.tsv:2188[escalate] |
| COM-WMI | wbemdisp!object_Invoke | dlls/wbemdisp/locator.c:2731 | fast-b.tsv:2295[escalate] |
| COM-WMI | wbemdisp!services_ExecNotificationQueryAsync | dlls/wbemdisp/locator.c:4205 | fast-b.tsv:1030[escalate] |
| COM-WMI | wbemdisp!sink_cp_EnumConnections | dlls/wbemdisp/locator.c:425 | fast-b.tsv:2294[escalate] |
| COM-WMI | wbemdisp!sink_cpc_EnumConnectionPoints | dlls/wbemdisp/locator.c:313 | fast-b.tsv:2293[escalate] |
| COM-WMI | wbemprox!Win32_SystemRestore_enumeration | dlls/wbemprox/builtin.c | fast-b.tsv:2214[escalate] |
| COM-WMI | wbemprox!class_object_SpawnDerivedClass | dlls/wbemprox/class.c:996 | fast-b.tsv:840[escalate] |
| COM-WMI | wbemprox!sysrestore_restore | dlls/wbemprox/sysrestore.c:102 | fast-b.tsv:839[escalate] |
| COM-WSC | scrobj!ActiveScriptSite_GetDocVersionString | dlls/scrobj/scrobj.c:744 | fast-b.tsv:2347[escalate] |
| COM-WSC | scrobj!global_GetNameSpaceParent | dlls/scrobj/scrobj.c:576 | fast-b.tsv:2346[escalate] |
| COM-WSC | scrobj!instance_cp_EnumConnections | dlls/scrobj/scrobj.c:1714 | fast-b.tsv:2351[escalate] |
| COM-WSC | scrobj!instance_cpc_EnumConnectionPoints | dlls/scrobj/scrobj.c:1596 | fast-b.tsv:2350[escalate] |
| COM-WSC | scrobj!parse_scriptlet_file | dlls/scrobj/scrobj.c:2463 | fast-b.tsv:2355[escalate] |
| COM-WSC | scrobj!parse_scriptlet_public | dlls/scrobj/scrobj.c:2193 | fast-b.tsv:2354[escalate] |
| COM-WSC | scrobj!parse_scriptlet_registration | dlls/scrobj/scrobj.c:2061 | fast-b.tsv:2353[escalate] |
| COM-WSC | scrobj!parse_scriptlet_script | dlls/scrobj/scrobj.c:1980 | fast-b.tsv:2352[escalate] |
| COM-WSC | scrobj!scriptlet_GetNameSpaceParent | dlls/scrobj/scrobj.c:1492 | fast-b.tsv:2349[escalate] |
| CRT | concrt140/msvcr1x0!CreateResourceManager, _ConcRT_* hooks, location, set_task_execution_resources, agent, _Atexit; NFS_GetLineSize | docs/stubs/escalate-5.md:98 | triage-5.tsv:483[escalate] |
| CRT | msvcp!num_get::_Getffldx (round 4) | docs/stubs/escalate-5.md:110 | triage-5.tsv:493[escalate] |
| CRT | msvcp100+!messages/money facets for _Make*loc (round 4) | docs/stubs/escalate-5.md:109 | triage-5.tsv:492[escalate] |
| CRT | msvcp140!std::filesystem conformance failures | docs/stubs/escalate-5.md:93 | triage-5.tsv:478[escalate] |
| CRT | msvcp90!locale__Locimp__Makeushloc | dlls/msvcp90/locale.c:15207 | triage-5.tsv:260[escalate] |
| CRT | msvcp90!locale__Locimp__Makewloc | dlls/msvcp90/locale.c:15301 | triage-5.tsv:261[escalate] |
| CRT | msvcp90!locale__Locimp__Makexloc | dlls/msvcp90/locale.c:15407 | triage-5.tsv:262[escalate] |
| CRT | msvcp90!num_get_char__Getffldx | dlls/msvcp90/locale.c:9900 | triage-5.tsv:308[escalate] |
| CRT | msvcp90!num_get_wchar__Getffldx | dlls/msvcp90/locale.c:8698 | triage-5.tsv:307[escalate] |
| CRT | msvcrt!ExternalContextBase_GetScheduleGroupId | dlls/msvcrt/concurrency.c:1415 | triage-5.tsv:4[escalate] |
| CRT | msvcrt!ExternalContextBase_GetVirtualProcessorId | dlls/msvcrt/concurrency.c:1408 | triage-5.tsv:3[escalate] |
| CRT | msvcrt!ThreadScheduler_IsAvailableLocation | dlls/msvcrt/concurrency.c:2069 | triage-5.tsv:13[escalate] |
| CRT | msvcrt!ThreadScheduler_ScheduleTask_loc | dlls/msvcrt/concurrency.c:2028 | triage-5.tsv:12[escalate] |
| CRT | msvcrt!_StructuredTaskCollection_ctor | dlls/msvcrt/concurrency.c:2547 | triage-5.tsv:14[escalate] |
| CRT | msvcrt!__intrinsic_abnormal_termination | dlls/msvcrt/except.c:813 | triage-5.tsv:2[escalate] |
| CRT | msvcrt!_heapmin | dlls/msvcrt/heap.c:258 | triage-5.tsv:56[escalate] |
| CRT | msvcrt!_heapmin (round 4) | docs/stubs/escalate-5.md:111 | triage-5.tsv:494[escalate] |
| CRT | msvcrtd!_CrtMemCheckpoint block types, _CrtSetAllocHook, _CrtSetDbgBlockType, _CrtMemDumpAllObjectsSince | docs/stubs/escalate-5.md:97 | triage-5.tsv:482[escalate] |
| DB-ADO | msado15!connection_OpenSchema | dlls/msado15/connection.c:640 | fast-a.tsv:674[escalate] |
| DB-ADO | msado15!recordset_CancelBatch | dlls/msado15/recordset.c:3087 | fast-a.tsv:676[escalate] |
| DB-ADO | msado15!recordset_UpdateBatch | dlls/msado15/recordset.c:3074 | fast-a.tsv:675[escalate] |
| DB-ADO | msdasql!column_rs_GetColumnsRowset | dlls/msdasql/session.c:1533 | fast-b.tsv:1056[escalate] |
| DB-ADO | msdasql!transaction_GetTransactionInfo | dlls/msdasql/session.c:2457 | fast-b.tsv:933[escalate] |
| DB-ADO | msdasql!transjoin_GetOptionsObject | dlls/msdasql/session.c:2383 | fast-b.tsv:931[escalate] |
| DB-ADO | msdasql!transjoin_JoinTransaction | dlls/msdasql/session.c:2392 | fast-b.tsv:932[escalate] |
| DBG-DUMP | dbgeng!DebugConnect | dlls/dbgeng/dbgeng.c:14914 | fast-b.tsv:429[escalate] |
| DBG-DUMP | dbgeng!debugadvanced_Request | dlls/dbgeng/dbgeng.c:13990 | fast-b.tsv:417[escalate] |
| DBG-DUMP | dbgeng!debugclient_AddDumpInformationFile | dlls/dbgeng/dbgeng.c:1795 | fast-b.tsv:437[escalate] |
| DBG-DUMP | dbgeng!debugclient_AddDumpInformationFileWide | dlls/dbgeng/dbgeng.c:2004 | fast-b.tsv:438[escalate] |
| DBG-DUMP | dbgeng!debugclient_AttachProcess | dlls/dbgeng/dbgeng.c:1194 | fast-b.tsv:427[escalate] |
| DBG-DUMP | dbgeng!debugclient_CreateProcess | dlls/dbgeng/dbgeng.c:1270 | fast-b.tsv:428[escalate] |
| DBG-DUMP | dbgeng!debugclient_OpenDumpFile | dlls/dbgeng/dbgeng.c:1350 | fast-b.tsv:435[escalate] |
| DBG-DUMP | dbgeng!debugclient_OpenDumpFileWide | dlls/dbgeng/dbgeng.c:1980 | fast-b.tsv:436[escalate] |
| DBG-DUMP | dbgeng!debugcontrol_Assemble | dlls/dbgeng/dbgeng.c:11087 | fast-b.tsv:439[escalate] |
| DBG-DUMP | dbgeng!debugcontrol_AssembleWide | dlls/dbgeng/dbgeng.c:12950 | fast-b.tsv:440[escalate] |
| DBG-DUMP | dbgeng!debugcontrol_GetWindbgExtensionApis32 | dlls/dbgeng/dbgeng.c:12039 | fast-b.tsv:441[escalate] |
| DBG-DUMP | dbgeng!debugcontrol_GetWindbgExtensionApis64 | dlls/dbgeng/dbgeng.c:12047 | fast-b.tsv:442[escalate] |
| DBG-REMOTE | dbgeng!debugclient_AttachKernel | dlls/dbgeng/dbgeng.c:1060 | fast-b.tsv:418[escalate] |
| DBG-REMOTE | dbgeng!debugclient_AttachKernelWide | dlls/dbgeng/dbgeng.c:2038 | fast-b.tsv:419[escalate] |
| DBG-REMOTE | dbgeng!debugclient_ConnectProcessServer | dlls/dbgeng/dbgeng.c:1090 | fast-b.tsv:421[escalate] |
| DBG-REMOTE | dbgeng!debugclient_ConnectProcessServerWide | dlls/dbgeng/dbgeng.c:2063 | fast-b.tsv:426[escalate] |
| DBG-REMOTE | dbgeng!debugclient_ConnectSession | dlls/dbgeng/dbgeng.c:1367 | fast-b.tsv:430[escalate] |
| DBG-REMOTE | dbgeng!debugclient_DisconnectProcessServer | dlls/dbgeng/dbgeng.c:1098 | fast-b.tsv:422[escalate] |
| DBG-REMOTE | dbgeng!debugclient_EndProcessServer | dlls/dbgeng/dbgeng.c:1801 | fast-b.tsv:423[escalate] |
| DBG-REMOTE | dbgeng!debugclient_OutputServers | dlls/dbgeng/dbgeng.c:1381 | fast-b.tsv:432[escalate] |
| DBG-REMOTE | dbgeng!debugclient_OutputServersWide | dlls/dbgeng/dbgeng.c:2075 | fast-b.tsv:434[escalate] |
| DBG-REMOTE | dbgeng!debugclient_StartProcessServer | dlls/dbgeng/dbgeng.c:1082 | fast-b.tsv:420[escalate] |
| DBG-REMOTE | dbgeng!debugclient_StartProcessServerWide | dlls/dbgeng/dbgeng.c:2057 | fast-b.tsv:425[escalate] |
| DBG-REMOTE | dbgeng!debugclient_StartServer | dlls/dbgeng/dbgeng.c:1374 | fast-b.tsv:431[escalate] |
| DBG-REMOTE | dbgeng!debugclient_StartServerWide | dlls/dbgeng/dbgeng.c:2069 | fast-b.tsv:433[escalate] |
| DBG-REMOTE | dbgeng!debugclient_WaitForProcessServerEnd | dlls/dbgeng/dbgeng.c:1807 | fast-b.tsv:424[escalate] |
| DEVICES | several!"Not done" list (engines, dialogs, undocumented signatures) | docs/stubs/escalate-5.md:81 | triage-5.tsv:476[escalate] |
| DEVICES | sti!stillimagew_CreateDevice | dlls/sti/sti.c:93 | fast-b.tsv:193[escalate] |
| DEVICES | sti!stillimagew_EnableHwNotifications | dlls/sti/sti.c:184 | fast-b.tsv:195[escalate] |
| DEVICES | sti!stillimagew_LaunchApplicationForDevice | dlls/sti/sti.c:207 | fast-b.tsv:197[escalate] |
| DEVICES | sti!stillimagew_RefreshDeviceBus | dlls/sti/sti.c:200 | fast-b.tsv:196[escalate] |
| DEVICES | sti!stillimagew_SetDeviceValue | dlls/sti/sti.c:110 | fast-b.tsv:194[escalate] |
| DEVICES | sti!stillimagew_SetupDeviceParameters | dlls/sti/sti.c:216 | fast-b.tsv:198[escalate] |
| DEVICES | tdh!TdhEnumerateManifestProviderEvents | dlls/tdh/main.c | fast-b.tsv:2189[escalate] |
| DEVICES | tdh!TdhFormatProperty | dlls/tdh/main.c | fast-b.tsv:2191[escalate] |
| DEVICES | tdh!TdhGetEventMapInformation | dlls/tdh/main.c | fast-b.tsv:2190[escalate] |
| DEVICES | tdh!TdhGetProperty | dlls/tdh/main.c | fast-b.tsv:2192[escalate] |
| DEVICES | tdh!TdhGetPropertySize | dlls/tdh/main.c | fast-b.tsv:2193[escalate] |
| DEVICES | tdh!TdhLoadManifestFromBinary | dlls/tdh/tdh_main.c:39 | fast-b.tsv:1050[escalate] |
| G-D2D | d2d1!d2d_device_CreatePrintControl | dlls/d2d1/device.c:5092 | fast-b.tsv:265[escalate] |
| G-D2D | d2d1!d2d_device_context_BlendImage | dlls/d2d1/device.c:3604 | fast-b.tsv:1526[escalate] |
| G-D2D | d2d1!d2d_device_context_CreateImageSourceFromDxgi | dlls/d2d1/device.c:3418 | fast-b.tsv:2171[escalate] |
| G-D2D | d2d1!d2d_device_context_DrawColorBitmapGlyphRun | dlls/d2d1/device.c:3518 | fast-b.tsv:1523[escalate] |
| G-D2D | d2d1!d2d_device_context_DrawGdiMetafile | dlls/d2d1/device.c:3233 | fast-b.tsv:2168[escalate] |
| G-D2D | d2d1!d2d_device_context_DrawGradientMesh | dlls/d2d1/device.c:3442 | fast-b.tsv:2170[escalate] |
| G-D2D | d2d1!d2d_device_context_GetColorBitmapGlyphImage | dlls/d2d1/device.c:3535 | fast-b.tsv:1524[escalate] |
| G-D2D | d2d1!d2d_device_context_GetEffectInvalidRectangleCount | dlls/d2d1/device.c:3285 | fast-b.tsv:1520[escalate] |
| G-D2D | d2d1!d2d_device_context_GetEffectInvalidRectangles | dlls/d2d1/device.c:3293 | fast-b.tsv:1521[escalate] |
| G-D2D | d2d1!d2d_device_context_GetEffectRequiredInputRectangles | dlls/d2d1/device.c:3301 | fast-b.tsv:1522[escalate] |
| G-D2D | d2d1!d2d_device_context_GetSvgGlyphImage | dlls/d2d1/device.c:3548 | fast-b.tsv:1525[escalate] |
| G-D2D | d2d1!d2d_device_context_ID2D1DeviceContext2_DrawGdiMetafile | dlls/d2d1/device.c:3448 | fast-b.tsv:2169[escalate] |
| G-D2D | d2d1!d2d_device_context_InvalidateEffectInputRectangle | dlls/d2d1/device.c:3276 | fast-b.tsv:1519[escalate] |
| G-D2D | d2d1!d2d_draw_info_SetVertexProcessing | dlls/d2d1/effect.c:3194 | fast-b.tsv:2323[escalate] |
| G-D2D | d2d1!d2d_path_geometry1_ComputePointAndSegmentAtLength | dlls/d2d1/geometry.c:4064 | fast-b.tsv:1528[escalate] |
| G-D3D11CLASS | d3d11!d3d11_class_linkage_CreateClassInstance | dlls/d3d11/shader.c:1790 | fast-b.tsv:1379[escalate] |
| G-D3D11CLASS | d3d11!d3d11_class_linkage_GetClassInstance | dlls/d3d11/shader.c:1781 | fast-b.tsv:1378[escalate] |
| G-D3D11CLASS | d3d11!d3d11_device_CreateComputeShader | dlls/d3d11/device.c:4363 | fast-b.tsv:1377[escalate] |
| G-D3D11CLASS | d3d11!d3d11_device_CreateDomainShader | dlls/d3d11/device.c:4340 | fast-b.tsv:1376[escalate] |
| G-D3D11CLASS | d3d11!d3d11_device_CreateGeometryShader | dlls/d3d11/device.c:4240 | fast-b.tsv:1372[escalate] |
| G-D3D11CLASS | d3d11!d3d11_device_CreateGeometryShaderWithStreamOutput | dlls/d3d11/device.c:4264 | fast-b.tsv:1373[escalate] |
| G-D3D11CLASS | d3d11!d3d11_device_CreateHullShader | dlls/d3d11/device.c:4317 | fast-b.tsv:1375[escalate] |
| G-D3D11CLASS | d3d11!d3d11_device_CreatePixelShader | dlls/d3d11/device.c:4294 | fast-b.tsv:1374[escalate] |
| G-D3D11CLASS | d3d11!d3d11_device_CreateVertexShader | dlls/d3d11/device.c:4217 | fast-b.tsv:1371[escalate] |
| G-D3D11CLASS | d3d11!d3d11_device_context_CSSetShader | dlls/d3d11/device.c:1664 | fast-b.tsv:1370[escalate] |
| G-D3D11CLASS | d3d11!d3d11_device_context_DSSetShader | dlls/d3d11/device.c:1593 | fast-b.tsv:1369[escalate] |
| G-D3D11CLASS | d3d11!d3d11_device_context_GSSetShader | dlls/d3d11/device.c:945 | fast-b.tsv:1367[escalate] |
| G-D3D11CLASS | d3d11!d3d11_device_context_HSSetShader | dlls/d3d11/device.c:1549 | fast-b.tsv:1368[escalate] |
| G-D3D11CLASS | d3d11!d3d11_device_context_PSSetShader | dlls/d3d11/device.c:731 | fast-b.tsv:1365[escalate] |
| G-D3D11CLASS | d3d11!d3d11_device_context_VSSetShader | dlls/d3d11/device.c:756 | fast-b.tsv:1366[escalate] |
| G-D3D11OTHER | d3d11!D3D11On12CreateDevice | dlls/d3d11/d3d11_main.c:382 | fast-b.tsv:1475[escalate] |
| G-D3D11OTHER | d3d11!d3d11_create_device | dlls/d3d11/d3d11_main.c:146 | fast-b.tsv:1471[escalate] |
| G-D3D11OTHER | d3d11!d3d11_device_ReadFromSubresource | dlls/d3d11/device.c:5805 | fast-b.tsv:2310[escalate] |
| G-D3D11OTHER | d3d11!d3d11_device_context_GetResourceMinLOD | dlls/d3d11/device.c:1499 | fast-b.tsv:1383[escalate] |
| G-D3D11OTHER | d3d11!d3d11_device_context_SetResourceMinLOD | dlls/d3d11/device.c:1493 | fast-b.tsv:1382[escalate] |
| G-D3D11OTHER | d3dcompiler_43!D3DCreateLinker | dlls/d3dcompiler_43/compiler.c:790 | fast-b.tsv:2239[escalate] |
| G-D3D11OTHER | d3dcompiler_43!D3DLoadModule | dlls/d3dcompiler_43/compiler.c:796 | fast-b.tsv:2240[escalate] |
| G-D3DX* | d3dx9_36!D3DXComputeTangentFrameEx | dlls/d3dx9_36/mesh.c:7900 | fast-b.tsv:605[no-ground-truth]; recheck-fast-a.tsv:195[escalate] |
| G-D3DX | d3dx9_36!d3dx9_fragment_linker_LinkPixelShader | dlls/d3dx9_36/shader.c:2252 | fast-b.tsv:2179[escalate] |
| G-D3DX | d3dx9_36!d3dx9_fragment_linker_LinkShader | dlls/d3dx9_36/shader.c:2232 | fast-b.tsv:2177[escalate] |
| G-D3DX | d3dx9_36!d3dx9_fragment_linker_LinkVertexShader | dlls/d3dx9_36/shader.c:2242 | fast-b.tsv:2178[escalate] |
| G-D3DX | d3dx9_36!d3dx_load_pixels_from_pixels | dlls/d3dx9_36/surface.c:2748 | fast-b.tsv:2180[escalate] |
| G-DCOMP | dcomp!DCompositionCreateDevice3 | dlls/dcomp/device.c:961 | fast-b.tsv:2147[escalate] |
| G-DCOMP | dcomp!device_CreateSurfaceFromHandle | dlls/dcomp/device.c:805 | fast-b.tsv:2148[escalate] |
| G-DCOMP | dcomp!device_CreateSurfaceFromHwnd | dlls/dcomp/device.c:812 | fast-b.tsv:2149[escalate] |
| G-DCOMP | dcomp!device_interop_SetRenderingDevice | dlls/dcomp/composition.c:1505 | fast-b.tsv:2144[escalate] |
| G-DDRAW | ddraw!ddraw7_DuplicateSurface | dlls/ddraw/ddraw.c:3715 | fast-b.tsv:809[escalate] |
| G-DDRAW | ddraw!ddraw_surface_create | dlls/ddraw/surface.c:6537 | fast-b.tsv:2292[escalate] |
| G-DINPUT | dinput!hid_joystick_effect_Initialize | dlls/dinput/joystick_hid.c:2187 | triage-5.tsv:71[escalate] |
| G-DINPUT | dinput!hid_joystick_effect_SetParameters | dlls/dinput/joystick_hid.c:2555 | triage-5.tsv:72[escalate] |
| G-DINPUT | dxdiag!wWinMain | programs/dxdiag/main.c:215 | triage-7.tsv:241[escalate] |
| G-DINPUT | dxtrans!DllGetClassObject | dlls/dxtrans/main.c:31 | triage-7.tsv:361[escalate] |
| G-DINPUT | hid!HidP_TranslateUsagesToI8042ScanCodes | dlls/hid/hidp.c:685 | triage-7.tsv:164[escalate] |
| G-DINPUT | hrtfapo!CreateHrtfApo | dlls/hrtfapo/main.c:25 | triage-7.tsv:370[escalate] |
| G-DINPUT | uianimation!animation_storyboard_RepeatBetweenKeyframes | dlls/uianimation/main.c:996 | triage-7.tsv:201[escalate] |
| G-DMUSIC | dmcompos!ComposeSegmentFrom*, ComposeTransition, AutoTransition, ComposeTemplateFromShape | docs/stubs/escalate-5.md:65 | triage-5.tsv:462[escalate] |
| G-DMUSIC | dmscript!AutoImp* / ScriptSourceCodeLoader classes | docs/stubs/escalate-5.md:72 | triage-5.tsv:469[escalate] |
| G-DMUSIC | dmscript!create_unimpl_instance | dlls/dmscript/dmscript_main.c:46 | triage-5.tsv:267[escalate] |
| G-DMUSIC | dmstyle!chord map methods (round 4) | docs/stubs/escalate-5.md:116 | triage-5.tsv:499[escalate] |
| G-DMUSIC | dmstyle!pattern playback for style/motif/audition tracks | docs/stubs/escalate-5.md:64 | triage-5.tsv:461[escalate] |
| G-DMUSIC | dmstyle!style_EnumChordMap | dlls/dmstyle/style.c:387 | triage-5.tsv:388[escalate] |
| G-DMUSIC | dmstyle!style_GetChordMap | dlls/dmstyle/style.c:394 | triage-5.tsv:389[escalate] |
| G-DMUSIC | dmstyle!style_GetDefaultChordMap | dlls/dmstyle/style.c:379 | triage-5.tsv:387[escalate] |
| G-DPLAY | dplaysvr!wmain | programs/dplaysvr/main.c:23 | triage-5.tsv:99[escalate] |
| G-DPLAY | dpnhpast!DirectPlayNATHelpCreate | dlls/dpnhpast/main.c:34 | triage-7.tsv:359[escalate] |
| G-DPLAY | dpnhupnp!DirectPlayNATHelpCreate | dlls/dpnhupnp/main.c:29 | triage-7.tsv:360[escalate] |
| G-DPLAY | dpnsvr!wmain | programs/dpnsvr/main.c:23 | triage-7.tsv:260[escalate] |
| G-DPLAY | dpvoice!dpvclient_Connect | dlls/dpvoice/client.c:104 | triage-5.tsv:111[escalate] |
| G-DPLAY | dpvoice!dpvclient_Create3DSoundBuffer | dlls/dpvoice/client.c:171 | triage-5.tsv:119[escalate] |
| G-DPLAY | dpvoice!dpvclient_Delete3DSoundBuffer | dlls/dpvoice/client.c:180 | triage-5.tsv:120[escalate] |
| G-DPLAY | dpvoice!dpvclient_Disconnect | dlls/dpvoice/client.c:112 | triage-5.tsv:112[escalate] |
| G-DPLAY | dpvoice!dpvclient_GetCaps | dlls/dpvoice/client.c:140 | triage-5.tsv:116[escalate] |
| G-DPLAY | dpvoice!dpvclient_GetClientConfig | dlls/dpvoice/client.c:126 | triage-5.tsv:114[escalate] |
| G-DPLAY | dpvoice!dpvclient_GetCompressionTypes | dlls/dpvoice/client.c:147 | triage-5.tsv:76[escalate] |
| G-DPLAY | dpvoice!dpvclient_GetSessionDesc | dlls/dpvoice/client.c:119 | triage-5.tsv:113[escalate] |
| G-DPLAY | dpvoice!dpvclient_GetSoundDeviceConfig | dlls/dpvoice/client.c:194 | triage-5.tsv:122[escalate] |
| G-DPLAY | dpvoice!dpvclient_GetTransmitTargets | dlls/dpvoice/client.c:163 | triage-5.tsv:118[escalate] |
| G-DPLAY | dpvoice!dpvclient_Initialize | dlls/dpvoice/client.c:96 | triage-5.tsv:110[escalate] |
| G-DPLAY | dpvoice!dpvclient_SetClientConfig | dlls/dpvoice/client.c:133 | triage-5.tsv:115[escalate] |
| G-DPLAY | dpvoice!dpvclient_SetNotifyMask | dlls/dpvoice/client.c:187 | triage-5.tsv:121[escalate] |
| G-DPLAY | dpvoice!dpvclient_SetTransmitTargets | dlls/dpvoice/client.c:155 | triage-5.tsv:117[escalate] |
| G-DPLAY | dpvoice!dpvserver_GetCaps | dlls/dpvoice/server.c:158 | triage-5.tsv:129[escalate] |
| G-DPLAY | dpvoice!dpvserver_GetCompressionTypes | dlls/dpvoice/server.c:165 | triage-5.tsv:77[escalate] |
| G-DPLAY | dpvoice!dpvserver_GetSessionDesc | dlls/dpvoice/server.c:144 | triage-5.tsv:127[escalate] |
| G-DPLAY | dpvoice!dpvserver_GetTransmitTargets | dlls/dpvoice/server.c:181 | triage-5.tsv:131[escalate] |
| G-DPLAY | dpvoice!dpvserver_Initialize | dlls/dpvoice/server.c:122 | triage-5.tsv:124[escalate] |
| G-DPLAY | dpvoice!dpvserver_SetNotifyMask | dlls/dpvoice/server.c:189 | triage-5.tsv:132[escalate] |
| G-DPLAY | dpvoice!dpvserver_SetSessionDesc | dlls/dpvoice/server.c:151 | triage-5.tsv:128[escalate] |
| G-DPLAY | dpvoice!dpvserver_SetTransmitTargets | dlls/dpvoice/server.c:173 | triage-5.tsv:130[escalate] |
| G-DPLAY | dpvoice!dpvserver_StartSession | dlls/dpvoice/server.c:130 | triage-5.tsv:125[escalate] |
| G-DPLAY | dpvoice!dpvserver_StopSession | dlls/dpvoice/server.c:137 | triage-5.tsv:126[escalate] |
| G-DPLAY | dpvoice!dpvtest_CheckAudioSetup | dlls/dpvoice/client.c:283 | triage-5.tsv:123[escalate] |
| G-DPLAY | dpvsetup!wmain | programs/dpvsetup/main.c:23 | triage-7.tsv:261[escalate] |
| G-DWRITE | dwrite!dwritefactory5_CreateHttpFontFileLoader | dlls/dwrite/main.c:2047 | fast-b.tsv:1515[escalate] |
| G-DWRITE | dwrite!dwritefactory5_UnpackFontFile | dlls/dwrite/main.c:2063 | fast-b.tsv:1517[escalate] |
| G-DWRITE | dwrite!freetype_get_aa_glyph_bitmap | dlls/dwrite/freetype.c:646 | fast-b.tsv:2322[escalate] |
| G-DWRITE | dwrite!layout_apply_text_alignment | dlls/dwrite/layout.c:1835 | fast-b.tsv:2321[escalate] |
| G-EOT | t2embed!TTDeleteEmbeddedFont | dlls/t2embed/main.c:164 | fast-a-cloud.tsv:98[escalate]; triage-7.tsv:192[escalate] |
| G-EOT | t2embed!TTEmbedFont | dlls/t2embed/main.c:45 | fast-a-cloud.tsv:97[escalate]; triage-7.tsv:191[escalate] |
| G-EOT | t2embed!TTLoadEmbeddedFont | dlls/t2embed/main.c:31 | fast-a-cloud.tsv:96[escalate]; triage-7.tsv:190[escalate] |
| G-GDIP | gdiplus!GdipPlayMetafileRecord | dlls/gdiplus/metafile.c:2855 | fast-b.tsv:2315[escalate] |
| G-GDIP | gdiplus!GdipRemovePropertyItem | dlls/gdiplus/image.c:4591 | fast-b.tsv:1499[escalate] |
| G-GDIP | gdiplus!GdipSetImageAttributesOutputChannel | dlls/gdiplus/imageattributes.c:230 | fast-b.tsv:2311[escalate] |
| G-GDIP | gdiplus!GdipSetImageAttributesOutputChannelColorProfile | dlls/gdiplus/imageattributes.c:243 | fast-b.tsv:2312[escalate] |
| G-GDIP | gdiplus!GdipSetPropertyItem | dlls/gdiplus/image.c:4606 | fast-b.tsv:1500[escalate] |
| G-GDIP | gdiplus!GdipWindingModeOutline | dlls/gdiplus/graphicspath.c:2836 | fast-b.tsv:2317[escalate] |
| G-GDIP | gdiplus!METAFILE_DrawImagePointsRect | dlls/gdiplus/metafile.c:4932 | fast-b.tsv:2316[escalate] |
| G-GDIP | gdiplus!METAFILE_PlaybackObject | dlls/gdiplus/metafile.c:2627 | fast-b.tsv:2314[escalate] |
| G-GDIP | gdiplus!metafile_free_object_table_entry | dlls/gdiplus/metafile.c:649 | fast-b.tsv:2313[escalate] |
| G-GL32 | opengl32!wow64_ext_glColorPointerListIBM | dlls/opengl32/unix_thunks.c:38440 | fast-b.tsv:379[escalate] |
| G-GL32 | opengl32!wow64_ext_glColorPointervINTEL | dlls/opengl32/unix_thunks.c:38463 | fast-b.tsv:387[escalate] |
| G-GL32 | opengl32!wow64_ext_glCreateSyncFromCLeventARB | dlls/opengl32/unix_thunks.c:41623 | fast-b.tsv:2286[escalate] |
| G-GL32 | opengl32!wow64_ext_glEdgeFlagPointerListIBM | dlls/opengl32/unix_thunks.c:44322 | fast-b.tsv:380[escalate] |
| G-GL32 | opengl32!wow64_ext_glFogCoordPointerListIBM | dlls/opengl32/unix_thunks.c:45326 | fast-b.tsv:381[escalate] |
| G-GL32 | opengl32!wow64_ext_glGetVkProcAddrNV | dlls/opengl32/unix_thunks.c:55681 | fast-b.tsv:2287[escalate] |
| G-GL32 | opengl32!wow64_ext_glImportSyncEXT | dlls/opengl32/unix_thunks.c:57036 | fast-b.tsv:2288[escalate] |
| G-GL32 | opengl32!wow64_ext_glIndexPointerListIBM | dlls/opengl32/unix_thunks.c:57137 | fast-b.tsv:382[escalate] |
| G-GL32 | opengl32!wow64_ext_glMapObjectBufferATI | dlls/opengl32/unix_thunks.c:59231 | fast-b.tsv:395[escalate] |
| G-GL32 | opengl32!wow64_ext_glMapTexture2DINTEL | dlls/opengl32/unix_thunks.c:59290 | fast-b.tsv:396[escalate] |
| G-GL32 | opengl32!wow64_ext_glNormalPointerListIBM | dlls/opengl32/unix_thunks.c:66032 | fast-b.tsv:383[escalate] |
| G-GL32 | opengl32!wow64_ext_glNormalPointervINTEL | dlls/opengl32/unix_thunks.c:66053 | fast-b.tsv:388[escalate] |
| G-GL32 | opengl32!wow64_ext_glReplacementCodePointerSUN | dlls/opengl32/unix_thunks.c:72983 | fast-b.tsv:391[escalate] |
| G-GL32 | opengl32!wow64_ext_glSecondaryColorPointerListIBM | dlls/opengl32/unix_thunks.c:74893 | fast-b.tsv:384[escalate] |
| G-GL32 | opengl32!wow64_ext_glTexCoordPointerListIBM | dlls/opengl32/unix_thunks.c:77653 | fast-b.tsv:385[escalate] |
| G-GL32 | opengl32!wow64_ext_glTexCoordPointervINTEL | dlls/opengl32/unix_thunks.c:77676 | fast-b.tsv:389[escalate] |
| G-GL32 | opengl32!wow64_ext_glVertexPointerListIBM | dlls/opengl32/unix_thunks.c:89536 | fast-b.tsv:386[escalate] |
| G-GL32 | opengl32!wow64_ext_glVertexPointervINTEL | dlls/opengl32/unix_thunks.c:89559 | fast-b.tsv:390[escalate] |
| G-GL32 | win32u!dibdrv_wglCopyContext | dlls/win32u/dibdrv/dc.c:459 | triage-6.tsv:491[escalate] |
| G-ICM | gdi32!ColorMatchToTarget | dlls/gdi32/icm.c:219 | triage-6.tsv:685[escalate] |
| G-ICM | gdi32!UpdateICMRegKeyA | dlls/gdi32/icm.c:198 | triage-6c.tsv:10[escalate] |
| G-ICM | gdi32!UpdateICMRegKeyW | dlls/gdi32/icm.c:207 | triage-6c.tsv:11[escalate] |
| G-ICM | icmui!SetupColorMatchingW | dlls/icmui/icmui.c:30 | triage-7.tsv:371[escalate] |
| G-ICM | mscms!GetPS2ColorRenderingDictionary | dlls/mscms/stub.c:44 | fast-b.tsv:789[escalate] |
| G-ICM | mscms!GetPS2ColorSpaceArray | dlls/mscms/stub.c:52 | fast-b.tsv:2291[escalate] |
| G-PROTECT | d3d11!d3d11_video_context_ConfigureAuthenticatedChannel | dlls/d3d11/device.c:3752 | fast-b.tsv:2004[escalate] |
| G-PROTECT | d3d11!d3d11_video_context_DecryptionBlt |  | fast-b.tsv:1998[escalate] |
| G-PROTECT | d3d11!d3d11_video_context_EncryptionBlt | dlls/d3d11/device.c:3700 | fast-b.tsv:1997[escalate] |
| G-PROTECT | d3d11!d3d11_video_context_FinishSessionKeyRefresh | dlls/d3d11/device.c:3724 | fast-b.tsv:2000[escalate] |
| G-PROTECT | d3d11!d3d11_video_context_GetEncryptionBltKey | dlls/d3d11/device.c:3730 | fast-b.tsv:2001[escalate] |
| G-PROTECT | d3d11!d3d11_video_context_NegotiateAuthenticatedChannelKeyExchange | dlls/d3d11/device.c:3737 | fast-b.tsv:2002[escalate] |
| G-PROTECT | d3d11!d3d11_video_context_NegotiateCryptoSessionKeyExchange | dlls/d3d11/device.c:3693 | fast-b.tsv:1996[escalate] |
| G-PROTECT | d3d11!d3d11_video_context_QueryAuthenticatedChannel | dlls/d3d11/device.c:3744 | fast-b.tsv:2003[escalate] |
| G-PROTECT | d3d11!d3d11_video_context_StartSessionKeyRefresh | dlls/d3d11/device.c:3717 | fast-b.tsv:1999[escalate] |
| G-PROTECT | d3d11!d3d11_video_device_CreateAuthenticatedChannel | dlls/d3d11/device.c:8233 | fast-b.tsv:1993[escalate] |
| G-PROTECT | d3d11!d3d11_video_device_CreateCryptoSession | dlls/d3d11/device.c:8240 | fast-b.tsv:1994[escalate] |
| G-PROTECT | d3d11!d3d11_video_device_GetCryptoSessionPrivateDataSize | dlls/d3d11/device.c:8358 | fast-b.tsv:1995[escalate] |
| G-PSDRV | wineps.drv!ControlPrintProcessor | dlls/wineps.drv/printproc.c:3314 | fast-a-cloud.tsv:57[escalate]; triage-7.tsv:395[escalate] |
| G-PSDRV* | wineps.drv!PSDRV_ExtEscape | dlls/wineps.drv/escape.c:90 | fast-a-cloud.tsv:52[windows-matches]; triage-7.tsv:396[escalate] |
| G-PSDRV | wineps.drv!get_glyf_pos | dlls/wineps.drv/type1.c:216 | fast-a-cloud.tsv:54[escalate]; triage-7.tsv:397[escalate] |
| G-SHARED | d3d11!d3d10_device_OpenSharedResource | dlls/d3d11/device.c:7933 | fast-b.tsv:2309[escalate] |
| G-SHARED | d3d11!d3d11_device_OpenSharedFence | dlls/d3d11/device.c:5829 | fast-b.tsv:2175[escalate] |
| G-SHARED | d3d11!d3d11_device_OpenSharedResource1 | dlls/d3d11/device.c:5497 | fast-b.tsv:2307[escalate] |
| G-SHARED | d3d11!d3d11_device_OpenSharedResourceByName | dlls/d3d11/device.c:5510 | fast-b.tsv:2308[escalate] |
| G-SHARED | d3d11!d3d11_device_context_Wait | dlls/d3d11/device.c:3139 | fast-b.tsv:2176[escalate] |
| G-SHARED | d3d9!d3d9_device_CreateCubeTexture | dlls/d3d9/device.c:1496 | fast-b.tsv:1531[escalate] |
| G-SHARED | d3d9!d3d9_device_CreateDepthStencilSurface | dlls/d3d9/device.c:1750 | fast-b.tsv:1535[escalate] |
| G-SHARED | d3d9!d3d9_device_CreateDepthStencilSurfaceEx | dlls/d3d9/device.c:4459 | fast-b.tsv:1538[escalate] |
| G-SHARED | d3d9!d3d9_device_CreateIndexBuffer | dlls/d3d9/device.c:1596 | fast-b.tsv:1533[escalate] |
| G-SHARED | d3d9!d3d9_device_CreateOffscreenPlainSurface | dlls/d3d9/device.c:2068 | fast-b.tsv:1536[escalate] |
| G-SHARED | d3d9!d3d9_device_CreateRenderTarget | dlls/d3d9/device.c:1718 | fast-b.tsv:1534[escalate] |
| G-SHARED | d3d9!d3d9_device_CreateRenderTargetEx | dlls/d3d9/device.c:4409 | fast-b.tsv:1537[escalate] |
| G-SHARED | d3d9!d3d9_device_CreateTexture | dlls/d3d9/device.c:1376 | fast-b.tsv:1529[escalate] |
| G-SHARED | d3d9!d3d9_device_CreateVertexBuffer | dlls/d3d9/device.c:1552 | fast-b.tsv:1532[escalate] |
| G-SHARED | d3d9!d3d9_device_CreateVolumeTexture | dlls/d3d9/device.c:1449 | fast-b.tsv:1530[escalate] |
| G-SHARED | dxgi!dxgi_composition_swapchain_create | dlls/dxgi/composition.c:980 | fast-b.tsv:1890[escalate]; triage-7.tsv:130[escalate] |
| G-SHARED | win32u!NtGdiDdDDICreateAllocation | dlls/win32u/d3dkmt.c:624 | triage-6.tsv:453[escalate] |
| G-SHARED | win32u!NtGdiDdDDICreateAllocation2 | dlls/win32u/d3dkmt.c:615 | triage-6.tsv:452[escalate] |
| G-SHARED | win32u!NtGdiDdDDICreateKeyedMutex | dlls/win32u/d3dkmt.c:706 | triage-6.tsv:462[escalate] |
| G-SHARED | win32u!NtGdiDdDDICreateKeyedMutex2 | dlls/win32u/d3dkmt.c:697 | triage-6.tsv:461[escalate] |
| G-SHARED | win32u!NtGdiDdDDICreateSynchronizationObject | dlls/win32u/d3dkmt.c:761 | triage-6.tsv:468[escalate] |
| G-SHARED | win32u!NtGdiDdDDICreateSynchronizationObject2 | dlls/win32u/d3dkmt.c:752 | triage-6.tsv:467[escalate] |
| G-SHARED | win32u!NtGdiDdDDIDestroyAllocation | dlls/win32u/d3dkmt.c:642 | triage-6.tsv:455[escalate] |
| G-SHARED | win32u!NtGdiDdDDIDestroyAllocation2 | dlls/win32u/d3dkmt.c:633 | triage-6.tsv:454[escalate] |
| G-SHARED | win32u!NtGdiDdDDIDestroyKeyedMutex | dlls/win32u/d3dkmt.c:715 | triage-6.tsv:463[escalate] |
| G-SHARED | win32u!NtGdiDdDDIDestroySynchronizationObject | dlls/win32u/d3dkmt.c:806 | triage-6.tsv:473[escalate] |
| G-SHARED | win32u!NtGdiDdDDIOpenKeyedMutex | dlls/win32u/d3dkmt.c:733 | triage-6.tsv:465[escalate] |
| G-SHARED | win32u!NtGdiDdDDIOpenKeyedMutex2 | dlls/win32u/d3dkmt.c:724 | triage-6.tsv:464[escalate] |
| G-SHARED | win32u!NtGdiDdDDIOpenKeyedMutexFromNtHandle | dlls/win32u/d3dkmt.c:742 | triage-6.tsv:466[escalate] |
| G-SHARED | win32u!NtGdiDdDDIOpenResource | dlls/win32u/d3dkmt.c:651 | triage-6.tsv:456[escalate] |
| G-SHARED | win32u!NtGdiDdDDIOpenResource2 | dlls/win32u/d3dkmt.c:660 | triage-6.tsv:457[escalate] |
| G-SHARED | win32u!NtGdiDdDDIOpenResourceFromNtHandle | dlls/win32u/d3dkmt.c:669 | triage-6.tsv:458[escalate] |
| G-SHARED | win32u!NtGdiDdDDIOpenSyncObjectFromNtHandle | dlls/win32u/d3dkmt.c:779 | triage-6.tsv:470[escalate] |
| G-SHARED | win32u!NtGdiDdDDIOpenSyncObjectFromNtHandle2 | dlls/win32u/d3dkmt.c:770 | triage-6.tsv:469[escalate] |
| G-SHARED | win32u!NtGdiDdDDIOpenSyncObjectNtHandleFromName | dlls/win32u/d3dkmt.c:788 | triage-6.tsv:471[escalate] |
| G-SHARED | win32u!NtGdiDdDDIOpenSynchronizationObject | dlls/win32u/d3dkmt.c:797 | triage-6.tsv:472[escalate] |
| G-SHARED | win32u!NtGdiDdDDIQueryAdapterInfo | dlls/win32u/d3dkmt.c:334 | triage-6c.tsv:8[escalate] |
| G-SHARED | win32u!NtGdiDdDDIQueryResourceInfo | dlls/win32u/d3dkmt.c:678 | triage-6.tsv:459[escalate] |
| G-SHARED | win32u!NtGdiDdDDIQueryResourceInfoFromNtHandle | dlls/win32u/d3dkmt.c:687 | triage-6.tsv:460[escalate] |
| G-SHARED | win32u!NtGdiDdDDIQueryStatistics | dlls/win32u/d3dkmt.c:345 | triage-6.tsv:449[escalate] |
| G-SHARED | win32u!NtGdiDdDDISetQueuedLimit | dlls/win32u/d3dkmt.c:412 | triage-6.tsv:450[escalate] |
| G-SHARED | win32u!NtGdiDdDDIShareObjects | dlls/win32u/d3dkmt.c:605 | triage-6.tsv:451[escalate] |
| G-SO | d3d11!d3d10_device_DrawAuto | dlls/d3d11/device.c:6472 | fast-b.tsv:1381[escalate] |
| G-SO | d3d11!d3d11_device_context_DrawAuto | dlls/d3d11/device.c:1232 | fast-b.tsv:1380[escalate] |
| G-SO* | d3d11!validate_stream_output_entries | dlls/d3d11/shader.c:933 | fast-b.tsv:1472[no-ground-truth]; recheck-fast-a.tsv:194[escalate] |
| G-SO | wined3d!state_so | dlls/wined3d/ffp_gl.c:1394 | fast-b.tsv:326[escalate] |
| G-TILED | d3d11!d3d11_device_context_CopyTileMappings | dlls/d3d11/device.c:3048 | fast-b.tsv:1385[escalate] |
| G-TILED | d3d11!d3d11_device_context_CopyTiles | dlls/d3d11/device.c:3056 | fast-b.tsv:1386[escalate] |
| G-TILED | d3d11!d3d11_device_context_ResizeTilePool | dlls/d3d11/device.c:3070 | fast-b.tsv:1388[escalate] |
| G-TILED | d3d11!d3d11_device_context_TiledResourceBarrier | dlls/d3d11/device.c:3077 | fast-b.tsv:1389[escalate] |
| G-TILED | d3d11!d3d11_device_context_UpdateTileMappings | dlls/d3d11/device.c:3039 | fast-b.tsv:1384[escalate] |
| G-TILED | d3d11!d3d11_device_context_UpdateTiles | dlls/d3d11/device.c:3063 | fast-b.tsv:1387[escalate] |
| G-USP10 | gdi32!EnableEUDC | dlls/gdi32/text.c:2124 | triage-6.tsv:148[escalate] |
| G-USP10 | gdi32!ScriptPlaceOpenType | dlls/gdi32/uniscribe/usp10.c:3358 | triage-6.tsv:151[escalate] |
| G-USP10 | gdi32!ScriptShapeOpenType | dlls/gdi32/uniscribe/usp10.c:3095 | triage-6.tsv:150[escalate] |
| G-USP10 | gdi32!XFORMOBJ_iGetXform | dlls/gdi32/umpd.c:971 | triage-6c.tsv:12[escalate] |
| G-VIDDEC | d3d11!d3d11_video_context_DecoderBeginFrame | dlls/d3d11/device.c:3378 | fast-b.tsv:1988[escalate] |
| G-VIDDEC | d3d11!d3d11_video_context_DecoderEndFrame | dlls/d3d11/device.c:3385 | fast-b.tsv:1989[escalate] |
| G-VIDDEC | d3d11!d3d11_video_context_DecoderExtension | dlls/d3d11/device.c:3399 | fast-b.tsv:1991[escalate] |
| G-VIDDEC | d3d11!d3d11_video_context_GetDecoderBuffer | dlls/d3d11/device.c:3364 | fast-b.tsv:1986[escalate] |
| G-VIDDEC | d3d11!d3d11_video_context_ReleaseDecoderBuffer | dlls/d3d11/device.c:3371 | fast-b.tsv:1987[escalate] |
| G-VIDDEC | d3d11!d3d11_video_context_SubmitDecoderBuffers | dlls/d3d11/device.c:3392 | fast-b.tsv:1990[escalate] |
| G-VIDDEC | d3d11!d3d11_video_context_VideoProcessorBlt | dlls/d3d11/device.c:3684 | fast-b.tsv:1992[escalate] |
| G-VIDDEC | d3d11!d3d11_video_decoder_GetDriverHandle | dlls/d3d11/decoder.c:123 | fast-b.tsv:2006[escalate] |
| G-VIDDEC | d3d11!d3d11_video_device_CreateVideoDecoder | dlls/d3d11/device.c:8209 | fast-b.tsv:2005[escalate] |
| G-VIDDEC | dxva2!DXVAHD_CreateDevice | dlls/dxva2/main.c:1557 | triage-5.tsv:79[escalate] |
| G-VIDDEC | dxva2!device_manager_processor_service_RegisterVideoProcessorSoftwareDevice | dlls/dxva2/main.c:437 | triage-5.tsv:133[escalate] |
| G-VKALLOC | win32u!win32u_vkCreateWin32SurfaceKHR | dlls/win32u/vulkan.c:117 | triage-6.tsv:480[escalate] |
| G-VKALLOC | win32u!win32u_vkDestroySurfaceKHR | dlls/win32u/vulkan.c:170 | triage-6.tsv:481[escalate] |
| G-VKALLOC | win32u!win32u_vkDestroySwapchainKHR | dlls/win32u/vulkan.c:382 | triage-6.tsv:482[escalate] |
| G-VKALLOC | winevulkan!wine_vkCreateCommandPool | dlls/winevulkan/vulkan.c:1251 | fast-a-cloud.tsv:15[escalate]; triage-7.tsv:342[escalate] |
| G-VKALLOC | winevulkan!wine_vkCreateDebugReportCallbackEXT | dlls/winevulkan/vulkan.c:2009 | fast-a-cloud.tsv:18[escalate]; triage-7.tsv:345[escalate] |
| G-VKALLOC | winevulkan!wine_vkCreateDebugUtilsMessengerEXT | dlls/winevulkan/vulkan.c:1953 | fast-a-cloud.tsv:17[escalate]; triage-7.tsv:344[escalate] |
| G-VKALLOC | winevulkan!wine_vkCreateDeferredOperationKHR | dlls/winevulkan/vulkan.c:2065 | fast-a-cloud.tsv:19[escalate]; triage-7.tsv:346[escalate] |
| G-VKALLOC | winevulkan!wine_vkCreateDevice | dlls/winevulkan/vulkan.c:857 | fast-a-cloud.tsv:12[escalate]; triage-7.tsv:339[escalate] |
| G-VKALLOC | winevulkan!wine_vkCreateInstance | dlls/winevulkan/vulkan.c:933 | fast-a-cloud.tsv:13[escalate]; triage-7.tsv:340[escalate] |
| G-VKALLOC | winevulkan!wine_vkDestroyCommandPool | dlls/winevulkan/vulkan.c:1282 | fast-a-cloud.tsv:16[escalate]; triage-7.tsv:343[escalate] |
| G-VKALLOC | winevulkan!wine_vkDestroyDevice | dlls/winevulkan/vulkan.c:1020 | fast-a-cloud.tsv:14[escalate]; triage-7.tsv:341[escalate] |
| G-WIC | windowscodecs!ImagingFactory_CreateFastMetadataEncoderFromDecoder | dlls/windowscodecs/imgfactory.c:1033 | fast-b.tsv:2318[escalate] |
| G-WIC | windowscodecs!ImagingFactory_CreateFastMetadataEncoderFromFrameDecode | dlls/windowscodecs/imgfactory.c:1041 | fast-b.tsv:2319[escalate] |
| G-WIC | windowscodecs!ImagingFactory_CreateImageEncoder | dlls/windowscodecs/imgfactory.c:1235 | fast-b.tsv:1507[escalate] |
| G-WIC | windowscodecs!tiff_get_decode_info | dlls/windowscodecs/libtiff.c:174 | fast-b.tsv:2320[escalate] |
| G-WIC | windowscodecsext!DllGetClassObject | dlls/windowscodecsext/main.c:36 | triage-7.tsv:362[escalate] |
| G-WINED3D-GL | wined3d!context_generate_rt_mask_from_resource | dlls/wined3d/context_gl.c:458 | fast-b.tsv:311[escalate] |
| G-WINED3D-GL | wined3d!cpu_blitter_clear_rtv | dlls/wined3d/surface.c:1173 | fast-b.tsv:318[escalate] |
| G-WINED3D-GL | wined3d!draw_primitive | dlls/wined3d/context_gl.c:4640 | fast-b.tsv:313[escalate] |
| G-WINED3D-GL | wined3d!shader_generate_code | dlls/wined3d/shader.c:1873 | fast-b.tsv:2285[escalate] |
| G-WINED3D-GL | wined3d!wined3d_context_gl_set_fbo_key_for_render_target | dlls/wined3d/context_gl.c:487 | fast-b.tsv:312[escalate] |
| G-WINED3D-GL | wined3d!wined3d_device_apply_stateblock | dlls/wined3d/stateblock.c:3012 | fast-b.tsv:2284[escalate] |
| G-WINED3D-GL | wined3d!wined3d_device_context_clear_rendertarget_view | dlls/wined3d/device.c:4464 | fast-b.tsv:314[escalate] |
| G-WINED3D-GL | wined3d!wined3d_render_target_view_gl_cs_init | dlls/wined3d/view.c:556 | fast-b.tsv:317[escalate] |
| G-WINED3D-GL | wined3d!wined3d_rendertarget_view_prepare_location | dlls/wined3d/view.c:469 | fast-b.tsv:315[escalate] |
| G-WINED3D-GL | wined3d!wined3d_rendertarget_view_validate_location | dlls/wined3d/view.c:495 | fast-b.tsv:316[escalate] |
| G-WINED3D-GL | wined3d!wined3d_texture_load_drawable | dlls/wined3d/texture.c:2865 | fast-b.tsv:2282[escalate] |
| G-WINED3D-GL | wined3d!wined3d_texture_load_renderbuffer | dlls/wined3d/texture.c:2899 | fast-b.tsv:2283[escalate] |
| G-WINED3D-VK | wined3d!wined3d_context_vk_bind_stream_output_buffers | dlls/wined3d/context_vk.c:2934 | fast-b.tsv:321[escalate] |
| G-WINED3D-VK | wined3d!wined3d_render_target_view_vk_cs_init | dlls/wined3d/view.c:858 | fast-b.tsv:319[escalate] |
| G-WINED3D-VK | wined3d!wined3d_shader_descriptor_writes_vk_add_uav_counter_write | dlls/wined3d/context_vk.c:3294 | fast-b.tsv:323[escalate] |
| G-WINED3D-VK | wined3d!wined3d_shader_descriptor_writes_vk_add_uav_write | dlls/wined3d/context_vk.c:3248 | fast-b.tsv:322[escalate] |
| G-WINED3D-VK | wined3d!wined3d_texture_vk_load_location | dlls/wined3d/texture.c:5371 | fast-b.tsv:320[escalate] |
| G-WINRTCOMP | dcomp!compositor_CreateColorKeyFrameAnimation | dlls/dcomp/composition.c | fast-b.tsv:2151[escalate] |
| G-WINRTCOMP | dcomp!compositor_CreateCubicBezierEasingFunction | dlls/dcomp/composition.c | fast-b.tsv:2152[escalate] |
| G-WINRTCOMP | dcomp!compositor_CreateEffectFactory | dlls/dcomp/composition.c | fast-b.tsv:2153[escalate] |
| G-WINRTCOMP | dcomp!compositor_CreateEffectFactoryWithProperties | dlls/dcomp/composition.c | fast-b.tsv:2154[escalate] |
| G-WINRTCOMP | dcomp!compositor_CreateExpressionAnimation | dlls/dcomp/composition.c | fast-b.tsv:2155[escalate] |
| G-WINRTCOMP | dcomp!compositor_CreateExpressionAnimationWithExpression | dlls/dcomp/composition.c | fast-b.tsv:2156[escalate] |
| G-WINRTCOMP | dcomp!compositor_CreateLinearEasingFunction | dlls/dcomp/composition.c | fast-b.tsv:2157[escalate] |
| G-WINRTCOMP | dcomp!compositor_CreatePropertySet | dlls/dcomp/composition.c | fast-b.tsv:2158[escalate] |
| G-WINRTCOMP | dcomp!compositor_CreateQuaternionKeyFrameAnimation | dlls/dcomp/composition.c | fast-b.tsv:2159[escalate] |
| G-WINRTCOMP | dcomp!compositor_CreateScalarKeyFrameAnimation | dlls/dcomp/composition.c | fast-b.tsv:2160[escalate] |
| G-WINRTCOMP | dcomp!compositor_CreateScopedBatch | dlls/dcomp/composition.c | fast-b.tsv:2161[escalate] |
| G-WINRTCOMP | dcomp!compositor_CreateTargetForCurrentView | dlls/dcomp/composition.c | fast-b.tsv:2162[escalate] |
| G-WINRTCOMP | dcomp!compositor_CreateVector2KeyFrameAnimation | dlls/dcomp/composition.c | fast-b.tsv:2163[escalate] |
| G-WINRTCOMP | dcomp!compositor_CreateVector3KeyFrameAnimation | dlls/dcomp/composition.c | fast-b.tsv:2164[escalate] |
| G-WINRTCOMP | dcomp!compositor_CreateVector4KeyFrameAnimation | dlls/dcomp/composition.c | fast-b.tsv:2165[escalate] |
| G-WINRTCOMP | dcomp!compositor_GetCommitBatch | dlls/dcomp/composition.c | fast-b.tsv:2166[escalate] |
| G-WINRTCOMP | dcomp!compositor_interop_CreateCompositionSurfaceForHandle | dlls/dcomp/composition.c:2357 | fast-b.tsv:2150[escalate] |
| G-WINRTCOMP | dcomp!graphics_device_add_RenderingDeviceReplaced | dlls/dcomp/composition.c | fast-b.tsv:2145[escalate] |
| G-WINRTCOMP | dcomp!graphics_device_remove_RenderingDeviceReplaced | dlls/dcomp/composition.c | fast-b.tsv:2146[escalate] |
| G-X11 | winex11.drv!X11DRV_ConfigureNotify | dlls/winex11.drv/event.c:1128 | fast-a-cloud.tsv:29[escalate]; triage-7.tsv:349[escalate] |
| G-X11 | winex11.drv!X11DRV_GravityNotify | dlls/winex11.drv/event.c:1163 | fast-a-cloud.tsv:30[escalate]; triage-7.tsv:350[escalate] |
| G-X11* | winex11.drv!xic_preedit_caret | dlls/winex11.drv/xim.c:210 | fast-a-cloud.tsv:70[escalate]; triage-7.tsv:348[no-ground-truth] |
| K-CALLBACKS | ntoskrnl.exe!CmRegisterCallback | dlls/ntoskrnl.exe/ntoskrnl.c:3733 | triage-6.tsv:788[escalate] |
| K-CALLBACKS | ntoskrnl.exe!CmUnRegisterCallback | dlls/ntoskrnl.exe/ntoskrnl.c:3742 | triage-6.tsv:789[escalate] |
| K-CALLBACKS | ntoskrnl.exe!FsRtlRegisterFileSystemFilterCallbacks | dlls/ntoskrnl.exe/ntoskrnl.c:4258 | triage-6.tsv:794[escalate] |
| K-CALLBACKS | ntoskrnl.exe!FsRtlRegisterUncProvider | dlls/ntoskrnl.exe/ntoskrnl.c:2448 | triage-6.tsv:756[escalate] |
| K-CALLBACKS | ntoskrnl.exe!IoAttachDevice | dlls/ntoskrnl.exe/ntoskrnl.c:3751 | triage-6.tsv:790[escalate] |
| K-CALLBACKS | ntoskrnl.exe!IoGetDeviceObjectPointer | dlls/ntoskrnl.exe/ntoskrnl.c:1820 | triage-6.tsv:753[escalate] |
| K-CALLBACKS | ntoskrnl.exe!IoRegisterFileSystem | dlls/ntoskrnl.exe/ntoskrnl.c:2806 | triage-6.tsv:758[escalate] |
| K-CALLBACKS | ntoskrnl.exe!IoUnregisterFileSystem | dlls/ntoskrnl.exe/ntoskrnl.c:2833 | triage-6.tsv:759[escalate] |
| K-CALLBACKS | ntoskrnl.exe!IoWMIOpenBlock | dlls/ntoskrnl.exe/ntoskrnl.c:3538 | triage-6.tsv:782[escalate] |
| K-CALLBACKS | ntoskrnl.exe!IoWMIRegistrationControl | dlls/ntoskrnl.exe/ntoskrnl.c:3529 | triage-6.tsv:781[escalate] |
| K-CALLBACKS | ntoskrnl.exe!ObRegisterCallbacks | dlls/ntoskrnl.exe/ntoskrnl.c:3142 | triage-6.tsv:813[escalate] |
| K-CALLBACKS | ntoskrnl.exe!ObUnRegisterCallbacks | dlls/ntoskrnl.exe/ntoskrnl.c:3155 | triage-6.tsv:771[escalate] |
| K-CALLBACKS | ntoskrnl.exe!PsRemoveCreateThreadNotifyRoutine | dlls/ntoskrnl.exe/ntoskrnl.c:3309 | triage-6.tsv:777[escalate] |
| K-CALLBACKS | ntoskrnl.exe!PsSetCreateProcessNotifyRoutine | dlls/ntoskrnl.exe/ntoskrnl.c:3279 | triage-6.tsv:774[escalate] |
| K-CALLBACKS | ntoskrnl.exe!PsSetCreateProcessNotifyRoutineEx | dlls/ntoskrnl.exe/ntoskrnl.c:3289 | triage-6.tsv:775[escalate] |
| K-CALLBACKS | ntoskrnl.exe!PsSetCreateThreadNotifyRoutine | dlls/ntoskrnl.exe/ntoskrnl.c:3299 | triage-6.tsv:776[escalate] |
| K-CONALIAS | kernelbase!AddConsoleAliasA | dlls/kernelbase/console.c:363 | triage-6.tsv:215[escalate] |
| K-CONALIAS | kernelbase!AddConsoleAliasW | dlls/kernelbase/console.c:374 | triage-6.tsv:216[escalate] |
| K-CONALIAS | kernelbase!GetConsoleAliasA | dlls/kernelbase/console.c:837 | triage-6.tsv:219[escalate] |
| K-CONALIAS | kernelbase!GetConsoleAliasExesLengthA | dlls/kernelbase/console.c:859 | triage-6.tsv:221[escalate] |
| K-CONALIAS | kernelbase!GetConsoleAliasExesLengthW | dlls/kernelbase/console.c:870 | triage-6.tsv:222[escalate] |
| K-CONALIAS | kernelbase!GetConsoleAliasW | dlls/kernelbase/console.c:848 | triage-6.tsv:220[escalate] |
| K-CONALIAS | kernelbase!GetConsoleAliasesLengthA | dlls/kernelbase/console.c:881 | triage-6.tsv:223[escalate] |
| K-CONALIAS | kernelbase!GetConsoleAliasesLengthW | dlls/kernelbase/console.c:892 | triage-6.tsv:224[escalate] |
| K-IRQL | hal!KeGetCurrentIrql | dlls/hal/hal.c:139 | triage-7.tsv:64[escalate] |
| K-IRQL | hal!KfLowerIrql | dlls/hal/hal.c:126 | triage-7.tsv:62[escalate] |
| K-IRQL | hal!KfRaiseIrql | dlls/hal/hal.c:132 | triage-7.tsv:63[escalate] |
| K-IRQL | ntoskrnl.exe!KeLowerIrql | dlls/ntoskrnl.exe/ntoskrnl.c:4654 | triage-6.tsv:806[escalate] |
| K-IRQL | ntoskrnl.exe!KfRaiseIrql | dlls/ntoskrnl.exe/ntoskrnl.c:4649 | triage-6.tsv:805[escalate] |
| K-LPC | ntdll!NtAcceptConnectPort | dlls/ntdll/unix/sync.c:2442 | triage-6.tsv:305[escalate] |
| K-LPC | ntdll!NtCompleteConnectPort | dlls/ntdll/unix/sync.c:2453 | triage-6.tsv:306[escalate] |
| K-LPC | ntdll!NtConnectPort | dlls/ntdll/unix/sync.c:2405 | triage-6.tsv:302[escalate] |
| K-LPC | ntdll!NtCreatePort | dlls/ntdll/unix/sync.c:2394 | triage-6.tsv:301[escalate] |
| K-LPC | ntdll!NtListenPort | dlls/ntdll/unix/sync.c:2432 | triage-6.tsv:304[escalate] |
| K-LPC | ntdll!NtRegisterThreadTerminatePort | dlls/ntdll/unix/sync.c:2463 | triage-6.tsv:307[escalate] |
| K-LPC | ntdll!NtReplyWaitReceivePort | dlls/ntdll/unix/sync.c:2488 | triage-6.tsv:309[escalate] |
| K-LPC | ntdll!NtRequestWaitReplyPort | dlls/ntdll/unix/sync.c:2473 | triage-6.tsv:308[escalate] |
| K-LPC | ntdll!NtSecureConnectPort | dlls/ntdll/unix/sync.c:2419 | triage-6.tsv:303[escalate] |
| K-LPC | wow64!wow64_NtAcceptConnectPort | dlls/wow64/sync.c:135 | triage-6.tsv:710[escalate] |
| K-LPC | wow64!wow64_NtConnectPort | dlls/wow64/sync.c:198 | triage-6.tsv:711[escalate] |
| K-LPC | wow64!wow64_NtListenPort | dlls/wow64/sync.c:527 | triage-6.tsv:712[escalate] |
| K-LPC | wow64!wow64_NtReplyWaitReceivePort | dlls/wow64/sync.c:1266 | triage-6.tsv:714[escalate] |
| K-LPC | wow64!wow64_NtRequestWaitReplyPort | dlls/wow64/sync.c:1281 | triage-6.tsv:715[escalate] |
| K-LPC | wow64!wow64_NtSecureConnectPort | dlls/wow64/sync.c:1307 | triage-6.tsv:716[escalate] |
| K-MISC | advapi32!GetThreadWaitChain | dlls/advapi32/advapi.c:646 | triage-6.tsv:71[escalate] |
| K-MISC | advapi32!OpenThreadWaitChainSession | dlls/advapi32/advapi.c:639 | triage-6.tsv:70[escalate] |
| K-MISC | kernelbase!EnumResourceLanguagesExA | dlls/kernelbase/loader.c:662 | triage-6.tsv:229[escalate] |
| K-MISC | kernelbase!EnumResourceLanguagesExW | dlls/kernelbase/loader.c:722 | triage-6.tsv:230[escalate] |
| K-MISC | kernelbase!EnumResourceNamesExA | dlls/kernelbase/loader.c:782 | triage-6.tsv:231[escalate] |
| K-MISC | kernelbase!EnumResourceNamesExW | dlls/kernelbase/loader.c:862 | triage-6.tsv:232[escalate] |
| K-MISC | kernelbase!EnumResourceTypesExA | dlls/kernelbase/loader.c:949 | triage-6.tsv:233[escalate] |
| K-MISC | kernelbase!GetWsChangesEx | dlls/kernelbase/debug.c:2011 | triage-6.tsv:572[escalate] |
| K-MISC | kernelbase!QueryIoRingCapabilities | dlls/kernelbase/file.c:5401 | triage-6.tsv:206[escalate] |
| K-MISC | ntdll!NtNotifyChangeMultipleKeys | dlls/ntdll/unix/registry.c:762 | triage-6.tsv:320[escalate] |
| K-MISC | ntdll!create_logical_proc_info | dlls/ntdll/unix/system.c:1007 | triage-6.tsv:581[escalate] |
| K-MISC | ntdll!get_cached_dir_data | dlls/ntdll/unix/file.c:2948 | triage-6.tsv:277[escalate] |
| K-MISC | ntdll!get_working_set_ex | dlls/ntdll/unix/virtual.c:5464 | triage-6.tsv:874[escalate] |
| K-MISC | ntdll!symbolic link relative target and directory link removal | docs/stubs/escalate-5.md:99 | triage-5.tsv:484[escalate] |
| K-MISC* | virtdisk!DetachVirtualDisk | dlls/virtdisk/virtdisk_main.c:72 | fast-a-cloud.tsv:104[escalate]; triage-7.tsv:179[windows-matches] |
| K-MISC | virtdisk!OpenVirtualDisk | dlls/virtdisk/virtdisk_main.c:58 | fast-a-cloud.tsv:103[escalate]; triage-7.tsv:178[escalate] |
| K-MISC | win32u!pack_message | dlls/win32u/message.c:945 | triage-6.tsv:16[escalate] |
| K-MISC | win32u!unpack_message | dlls/win32u/message.c:522 | triage-6.tsv:446[escalate] |
| K-MM | ntoskrnl.exe!KeQueryPriorityThread | dlls/ntoskrnl.exe/ntoskrnl.c:2698 | triage-6.tsv:757[escalate] |
| K-MM | ntoskrnl.exe!KeStackAttachProcess | dlls/ntoskrnl.exe/ntoskrnl.c:4619 | triage-6.tsv:801[escalate] |
| K-MM | ntoskrnl.exe!KeUnstackDetachProcess | dlls/ntoskrnl.exe/ntoskrnl.c:4624 | triage-6.tsv:802[escalate] |
| K-MM | ntoskrnl.exe!MmAllocateContiguousMemory | dlls/ntoskrnl.exe/ntoskrnl.c:2850 | triage-6.tsv:760[escalate] |
| K-MM | ntoskrnl.exe!MmAllocateContiguousMemorySpecifyCache | dlls/ntoskrnl.exe/ntoskrnl.c:2859 | triage-6.tsv:761[escalate] |
| K-MM | ntoskrnl.exe!MmAllocatePagesForMdl | dlls/ntoskrnl.exe/ntoskrnl.c:2872 | triage-6.tsv:762[escalate] |
| K-MM | ntoskrnl.exe!MmCopyVirtualMemory | dlls/ntoskrnl.exe/ntoskrnl.c:4322 | triage-6.tsv:799[escalate] |
| K-MM | ntoskrnl.exe!MmGetPhysicalAddress | dlls/ntoskrnl.exe/ntoskrnl.c:2931 | triage-6.tsv:764[escalate] |
| K-MM | ntoskrnl.exe!MmMapIoSpace | dlls/ntoskrnl.exe/ntoskrnl.c:2942 | triage-6.tsv:765[escalate] |
| K-MM | ntoskrnl.exe!MmUnmapIoSpace | dlls/ntoskrnl.exe/ntoskrnl.c:3024 | triage-6.tsv:768[escalate] |
| K-MM | ntoskrnl.exe!ProbeForRead | dlls/ntoskrnl.exe/ntoskrnl.c:3717 | triage-6.tsv:786[escalate] |
| K-MM | ntoskrnl.exe!ProbeForWrite | dlls/ntoskrnl.exe/ntoskrnl.c:3725 | triage-6.tsv:787[escalate] |
| K-MOUNT | kernel32!DeleteVolumeMountPointA | dlls/kernel32/volume.c:536 | triage-6.tsv:171[escalate] |
| K-MOUNT | kernel32!FindFirstVolumeMountPointA | dlls/kernel32/volume.c:507 | triage-6.tsv:168[escalate] |
| K-MOUNT | kernel32!FindFirstVolumeMountPointW | dlls/kernel32/volume.c:517 | triage-6.tsv:169[escalate] |
| K-MOUNT | kernel32!FindVolumeMountPointClose | dlls/kernel32/volume.c:527 | triage-6.tsv:170[escalate] |
| K-MOUNT | kernel32!SetVolumeMountPointA | dlls/kernel32/volume.c:545 | triage-6.tsv:172[escalate] |
| K-MOUNT | kernel32!SetVolumeMountPointW | dlls/kernel32/volume.c:555 | triage-6.tsv:173[escalate] |
| K-NETDRV | http.sys!http_add_url | dlls/http.sys/http.c:933 | fast-b.tsv:989[escalate] |
| K-NETDRV | httpapi!HttpSetServiceConfiguration | dlls/httpapi/httpapi_main.c:458 | fast-b.tsv:988[escalate] |
| K-NETDRV | ndis.sys!NdisRegisterProtocol | dlls/ndis.sys/main.c:211 | triage-7.tsv:55[escalate] |
| K-NETDRV | netio.sys!wsk_control_client | dlls/netio.sys/netio.c:819 | triage-6.tsv:809[escalate] |
| K-NETDRV | netio.sys!wsk_inspect_complete | dlls/netio.sys/netio.c:468 | triage-6.tsv:745[escalate] |
| K-OBSEC | ntoskrnl.exe!IoCreateDeviceSecure | dlls/ntoskrnl.exe/ntoskrnl.c:1613 | triage-6.tsv:752[escalate] |
| K-OBSEC | ntoskrnl.exe!ObOpenObjectByName | dlls/ntoskrnl.exe/ntoskrnl.c:3078 | triage-6.tsv:770[escalate] |
| K-OBSEC | ntoskrnl.exe!ObOpenObjectByPointer | dlls/ntoskrnl.exe/ntoskrnl.c:374 | triage-6.tsv:751[escalate] |
| K-OBSEC | ntoskrnl.exe!ObReferenceObjectByHandle | dlls/ntoskrnl.exe/ntoskrnl.c:351 | triage-6.tsv:750[escalate] |
| K-OBSEC | ntoskrnl.exe!ObReferenceObjectByName | dlls/ntoskrnl.exe/ntoskrnl.c:3033 | triage-6.tsv:769[escalate] |
| K-OBSEC | ntoskrnl.exe!PsImpersonateClient | dlls/ntoskrnl.exe/ntoskrnl.c:3258 | triage-6.tsv:773[escalate] |
| K-OBSEC | ntoskrnl.exe!SePrivilegeCheck | dlls/ntoskrnl.exe/ntoskrnl.c:4277 | triage-6.tsv:796[escalate] |
| K-OBSEC | ntoskrnl.exe!SeSinglePrivilegeCheck | dlls/ntoskrnl.exe/ntoskrnl.c:4267 | triage-6.tsv:795[escalate] |
| K-PERF | advapi32!PerfAddCounters | dlls/advapi32/advapi.c:672 | triage-6.tsv:73[escalate] |
| K-PERF | advapi32!SetServiceBits | dlls/advapi32/service.c:512 | triage-6.tsv:640[escalate] |
| K-PERF | advapi32!WmiOpenBlock | dlls/advapi32/wmi.c:109 | triage-6.tsv:82[escalate] |
| K-PORTIO | hal!READ_PORT_UCHAR | dlls/hal/hal.c:145 | triage-7.tsv:65[escalate] |
| K-PORTIO | hal!READ_PORT_ULONG | dlls/hal/hal.c:151 | triage-7.tsv:66[escalate] |
| K-PORTIO | hal!WRITE_PORT_UCHAR | dlls/hal/hal.c:157 | triage-7.tsv:67[escalate] |
| K-PORTIO | hal!WRITE_PORT_ULONG | dlls/hal/hal.c:162 | triage-7.tsv:68[escalate] |
| K-PORTIO | ntoskrnl.exe!Ke386IoSetAccessProcess | dlls/ntoskrnl.exe/ntoskrnl.c:3579 | triage-6.tsv:783[escalate] |
| K-PORTIO | ntoskrnl.exe!Ke386QueryIoAccessMap | dlls/ntoskrnl.exe/ntoskrnl.c:3588 | triage-6.tsv:784[escalate] |
| K-PORTIO | ntoskrnl.exe!Ke386SetIoAccessMap | dlls/ntoskrnl.exe/ntoskrnl.c:3597 | triage-6.tsv:785[escalate] |
| K-SETUPAPI | setupapi!CM_Request_Device_EjectA | dlls/setupapi/stubs.c:863 | fast-a.tsv:126[escalate] |
| K-SETUPAPI | setupapi!CM_Request_Device_EjectW | dlls/setupapi/stubs.c:872 | fast-a.tsv:127[escalate] |
| K-SETUPAPI | setupapi!SetupDiBuildDriverInfoList | dlls/setupapi/devinst.c:6186 | fast-a.tsv:494[escalate] |
| K-SETUPAPI | setupapi!SetupDiCreateDeviceInfoListExW | dlls/setupapi/devinst.c:1668 | fast-a.tsv:128[escalate] |
| K-SETUPAPI | setupapi!SetupDiEnumDriverInfoW | dlls/setupapi/devinst.c:6286 | fast-a.tsv:495[escalate] |
| K-SETUPAPI | setupapi!SetupDiGetClassDevsExW | dlls/setupapi/devinst.c:2847 | fast-a.tsv:129[escalate] |
| K-SETUPAPI | setupapi!SetupDiOpenClassRegKeyExW | dlls/setupapi/devinst.c:3760 | fast-a.tsv:130[escalate] |
| K-SETUPAPI | setupapi!logconf_callback | dlls/setupapi/install.c:845 | fast-a.tsv:582[escalate] |
| K-TXF | kernel32!CopyFileTransactedA | dlls/kernel32/path.c:269 | triage-6.tsv:188[escalate] |
| K-TXF | kernel32!CopyFileTransactedW | dlls/kernel32/path.c:279 | triage-6.tsv:189[escalate] |
| K-TXF | kernel32!CreateDirectoryTransactedA | dlls/kernel32/path.c:192 | triage-6.tsv:180[escalate] |
| K-TXF | kernel32!CreateDirectoryTransactedW | dlls/kernel32/path.c:201 | triage-6.tsv:181[escalate] |
| K-TXF | kernel32!CreateFileTransactedA | dlls/kernel32/path.c:165 | triage-6.tsv:178[escalate] |
| K-TXF | kernel32!CreateFileTransactedW | dlls/kernel32/path.c:179 | triage-6.tsv:179[escalate] |
| K-TXF | kernel32!CreateHardLinkTransactedA | dlls/kernel32/path.c:602 | triage-6.tsv:204[escalate] |
| K-TXF | kernel32!CreateHardLinkTransactedW | dlls/kernel32/path.c:612 | triage-6.tsv:205[escalate] |
| K-TXF | kernel32!CreateSymbolicLinkTransactedA | dlls/kernel32/path.c:289 | triage-6.tsv:190[escalate] |
| K-TXF | kernel32!CreateSymbolicLinkTransactedW | dlls/kernel32/path.c:298 | triage-6.tsv:191[escalate] |
| K-TXF | kernel32!DeleteFileTransactedA | dlls/kernel32/path.c:210 | triage-6.tsv:182[escalate] |
| K-TXF | kernel32!DeleteFileTransactedW | dlls/kernel32/path.c:219 | triage-6.tsv:183[escalate] |
| K-TXF | kernel32!FindFirstFileNameTransactedW | dlls/kernel32/path.c:307 | triage-6.tsv:192[escalate] |
| K-TXF | kernel32!FindFirstFileTransactedA | dlls/kernel32/path.c:228 | triage-6.tsv:184[escalate] |
| K-TXF | kernel32!FindFirstFileTransactedW | dlls/kernel32/path.c:239 | triage-6.tsv:185[escalate] |
| K-TXF | kernel32!FindFirstStreamTransactedW | dlls/kernel32/path.c:316 | triage-6.tsv:193[escalate] |
| K-TXF | kernel32!GetCompressedFileSizeTransactedA | dlls/kernel32/path.c:325 | triage-6.tsv:194[escalate] |
| K-TXF | kernel32!GetCompressedFileSizeTransactedW | dlls/kernel32/path.c:334 | triage-6.tsv:195[escalate] |
| K-TXF | kernel32!GetFileAttributesTransactedA | dlls/kernel32/path.c:251 | triage-6.tsv:186[escalate] |
| K-TXF | kernel32!GetFileAttributesTransactedW | dlls/kernel32/path.c:260 | triage-6.tsv:187[escalate] |
| K-TXF | kernel32!GetLongPathNameTransactedA | dlls/kernel32/path.c:361 | triage-6.tsv:198[escalate] |
| K-TXF | kernel32!GetLongPathNameTransactedW | dlls/kernel32/path.c:370 | triage-6.tsv:199[escalate] |
| K-TXF | kernel32!MoveFileTransactedA | dlls/kernel32/path.c:145 | triage-6.tsv:176[escalate] |
| K-TXF | kernel32!MoveFileTransactedW | dlls/kernel32/path.c:155 | triage-6.tsv:177[escalate] |
| K-TXF | kernel32!RemoveDirectoryTransactedA | dlls/kernel32/path.c:397 | triage-6.tsv:202[escalate] |
| K-TXF | kernel32!RemoveDirectoryTransactedW | dlls/kernel32/path.c:406 | triage-6.tsv:203[escalate] |
| K-TXF | kernel32!SetFileAttributesTransactedA | dlls/kernel32/path.c:379 | triage-6.tsv:200[escalate] |
| K-TXF | kernel32!SetFileAttributesTransactedW | dlls/kernel32/path.c:388 | triage-6.tsv:201[escalate] |
| K-TXF | ntdll!NtCommitTransaction | dlls/ntdll/unix/sync.c:2937 | triage-6.tsv:311[escalate] |
| K-TXF | ntdll!NtCreateKeyTransacted | dlls/ntdll/unix/registry.c:146 | triage-6.tsv:588[escalate] |
| K-TXF | ntdll!NtCreateTransaction | dlls/ntdll/unix/sync.c:2923 | triage-6.tsv:310[escalate] |
| K-TXF | ntdll!NtOpenKeyTransactedEx | dlls/ntdll/unix/registry.c:201 | triage-6.tsv:318[escalate] |
| K-TXF | ntdll!NtRollbackTransaction | dlls/ntdll/unix/sync.c:2947 | triage-6.tsv:312[escalate] |
| K-USBBUS | winebus.sys!sdl_bus_init | dlls/winebus.sys/bus_sdl.c:1233 | triage-7.tsv:43[escalate] |
| K-USBBUS | winebus.sys!sdl_bus_stop | dlls/winebus.sys/bus_sdl.c:1245 | triage-7.tsv:45[escalate] |
| K-USBBUS | winebus.sys!sdl_bus_wait | dlls/winebus.sys/bus_sdl.c:1239 | triage-7.tsv:44[escalate] |
| K-USBBUS | wineusb.sys!driver_internal_ioctl | dlls/wineusb.sys/wineusb.c:652 | triage-7.tsv:61[escalate] |
| K-USBBUS | wineusb.sys!usb_submit_urb | dlls/wineusb.sys/wineusb.c:527 | triage-7.tsv:60[escalate] |
| M-CAPTURE | evr!video_presenter_control_SetFullscreen | dlls/evr/presenter.c:1789 | triage-5.tsv:143[escalate] |
| M-CAPTURE | mf!MFCreateDeviceSource/MFEnumDeviceSources (video capture) | docs/stubs/escalate-5.md:7 | triage-5.tsv:448[escalate] |
| M-CAPTURE | mfmediaengine!media_engine_FrameStep | dlls/mfmediaengine/main.c:3064 | triage-5.tsv:145[escalate] |
| M-CAPTURE | mfmediaengine!media_engine_GetVideoSwapchainHandle | dlls/mfmediaengine/main.c:3554 | triage-5.tsv:146[escalate] |
| M-CAPTURE | mfplay!media_player_FrameStep | dlls/mfplay/player.c:1059 | triage-5.tsv:147[escalate] |
| M-CAPTURE | mmdevapi!stream_init | dlls/mmdevapi/client.c:371 | triage-5.tsv:55[escalate] |
| M-CAPTURE | winegstreamer!H.264 encoder ignores MF_MT_MPEG2_PROFILE (round 4) | docs/stubs/escalate-5.md:115 | triage-5.tsv:498[escalate] |
| M-CAPTURE | winegstreamer!amt_from_wg_format | dlls/winegstreamer/quartz_parser.c:730 | triage-5.tsv:94[escalate] |
| M-DSHOW | qcap!fnCaptureGraphBuilder2_RenderStream | dlls/qcap/capturegraph.c:671 | triage-5.tsv:151[escalate] |
| M-DSHOW | qedit!timelinegrp_SetRecompFormatFromSource | dlls/qedit/timeline.c:1738 | triage-5.tsv:426[escalate] |
| M-DSHOW | quartz!VideoWindow_put_FullScreenMode FIXME, filtergraph/filtermapper FIXMEs, AddSourceFilter collections | docs/stubs/escalate-5.md:68 | triage-5.tsv:465[escalate] |
| M-DSHOW | quartz!remaining todo_wine in the quartz suites | docs/stubs/escalate-5.md:71 | triage-5.tsv:468[escalate] |
| M-DSHOW | wmvcore!WMCreateBackupRestorer | dlls/wmvcore/wmvcore_main.c:163 | triage-5.tsv:98[escalate] |
| M-DSHOW | wmvcore!WMCreateEditor | dlls/wmvcore/wmvcore_main.c:154 | triage-5.tsv:97[escalate] |
| M-DSHOW | wmvcore!WMWriter_BeginWriting | dlls/wmvcore/writer.c:695 | triage-5.tsv:259[escalate] |
| M-LEGACY | avifil32!AVIGetFromClipboard | dlls/avifil32/api.c:2197 | triage-5.tsv:264[escalate] |
| M-LEGACY | avifil32!AVIMakeStreamFromClipboard | dlls/avifil32/api.c:2209 | triage-5.tsv:265[escalate] |
| M-LEGACY | avifil32!AVIPutFileOnClipboard | dlls/avifil32/api.c:2223 | triage-5.tsv:266[escalate] |
| M-LEGACY | mciavi32!MCIAVI_mciConfigure | dlls/mciavi32/mciavi.c:1108 | triage-5.tsv:272[escalate] |
| M-LEGACY | msacm32!acmFilterChooseA | dlls/msacm32/filter.c:42 | triage-5.tsv:302[escalate] |
| M-LEGACY | msacm32!acmFilterChooseW | dlls/msacm32/filter.c:52 | triage-5.tsv:303[escalate] |
| M-LEGACY | msttsengine!ttsengine_GetOutputFormat | dlls/msttsengine/tts.c:117 | triage-7.tsv:436[escalate] |
| M-LEGACY | msttsengine!ttsengine_Speak | dlls/msttsengine/tts.c:108 | triage-7.tsv:435[escalate] |
| M-LEGACY | msvfw32!GetFileNamePreview | dlls/msvfw32/msvideo_main.c:1626 | triage-5.tsv:313[escalate] |
| M-LEGACY | msvfw32!MCIWndProc | dlls/msvfw32/mciwnd.c:454 | triage-5.tsv:312[escalate] |
| M-LEGACY | winmm!midiConnect/midiDisconnect, MMIO_GETTEMP, global IO procs | docs/stubs/escalate-5.md:6 | triage-5.tsv:447[escalate] |
| M-MMCSS | rtworkq!RtwqBeginRegisterWorkQueueWithMMCSS | dlls/rtworkq/queue.c:1614 | triage-5.tsv:167[escalate] |
| M-MMCSS | rtworkq!RtwqBeginUnregisterWorkQueueWithMMCSS | dlls/rtworkq/queue.c:1629 | triage-5.tsv:169[escalate] |
| M-MMCSS | rtworkq!RtwqCancelDeadline | dlls/rtworkq/queue.c:1546 | triage-5.tsv:159[escalate] |
| M-MMCSS | rtworkq!RtwqEndRegisterWorkQueueWithMMCSS | dlls/rtworkq/queue.c:1622 | triage-5.tsv:168[escalate] |
| M-MMCSS | rtworkq!RtwqEndUnregisterWorkQueueWithMMCSS | dlls/rtworkq/queue.c:1636 | triage-5.tsv:170[escalate] |
| M-MMCSS | rtworkq!RtwqGetWorkQueueMMCSSClass | dlls/rtworkq/queue.c:1579 | triage-5.tsv:162[escalate] |
| M-MMCSS | rtworkq!RtwqGetWorkQueueMMCSSPriority | dlls/rtworkq/queue.c:1593 | triage-5.tsv:164[escalate] |
| M-MMCSS | rtworkq!RtwqGetWorkQueueMMCSSTaskId | dlls/rtworkq/queue.c:1586 | triage-5.tsv:163[escalate] |
| M-MMCSS | rtworkq!RtwqJoinWorkQueue | dlls/rtworkq/queue.c:1565 | triage-5.tsv:160[escalate] |
| M-MMCSS | rtworkq!RtwqRegisterPlatformWithMMCSS | dlls/rtworkq/queue.c:1600 | triage-5.tsv:165[escalate] |
| M-MMCSS | rtworkq!RtwqSetDeadline | dlls/rtworkq/queue.c:1532 | triage-5.tsv:157[escalate] |
| M-MMCSS | rtworkq!RtwqSetDeadline2 | dlls/rtworkq/queue.c:1539 | triage-5.tsv:158[escalate] |
| M-MMCSS | rtworkq!RtwqUnjoinWorkQueue | dlls/rtworkq/queue.c:1572 | triage-5.tsv:161[escalate] |
| M-MMCSS | rtworkq!RtwqUnregisterPlatformFromMMCSS | dlls/rtworkq/queue.c:1607 | triage-5.tsv:166[escalate] |
| MSH-CSSOM | mshtml!DocNodeOleInPlaceObjectWindowless_GetDropTarget | dlls/mshtml/oleobj.c:1570 | fast-a.tsv:418[escalate] |
| MSH-CSSOM | mshtml!DocObjOleInPlaceObjectWindowless_GetDropTarget | dlls/mshtml/oleobj.c:1706 | fast-a.tsv:419[escalate] |
| MSH-CSSOM | mshtml!HTMLObjectElement2_put_classid | dlls/mshtml/htmlobject.c:525 | fast-a.tsv:56[escalate] |
| MSH-CSSOM | mshtml!HTMLPerformanceNavigation_toJSON | dlls/mshtml/omnavigator.c:1669 | fast-a.tsv:310[escalate] |
| MSH-CSSOM | mshtml!HTMLPerformanceTiming_toJSON | dlls/mshtml/omnavigator.c:1529 | fast-a.tsv:309[escalate] |
| MSH-CSSOM | mshtml!HTMLPerformance_toJSON | dlls/mshtml/omnavigator.c:1828 | fast-a.tsv:311[escalate] |
| MSH-CSSOM | mshtml!HTMLStyleSheet4_get_ownerRule | dlls/mshtml/htmlstylesheet.c:1358 | fast-a.tsv:429[escalate] |
| MSH-CSSOM | mshtml!HTMLStyleSheetRule_get_style | dlls/mshtml/htmlstylesheet.c:221 | fast-a.tsv:427[escalate] |
| MSH-CSSOM | mshtml!HTMLStyleSheetRule_put_selectorText | dlls/mshtml/htmlstylesheet.c:185 | fast-a.tsv:426[escalate] |
| MSH-CSSOM | mshtml!HTMLStyleSheet_get_imports | dlls/mshtml/htmlstylesheet.c:1044 | fast-a.tsv:428[escalate] |
| MSH-CSSOM | mshtml!HTMLWindow2_get_frames | dlls/mshtml/htmlwindow.c:449 | fast-a.tsv:59[escalate] |
| MSH-CSSOM | mshtml!HTMLWindow5_put_XMLHttpRequest | dlls/mshtml/htmlwindow.c:2046 | fast-a.tsv:334[escalate] |
| MSH-CSSOM | mshtml!HTMLWindow6_get_XDomainRequest | dlls/mshtml/htmlwindow.c:2128 | fast-a.tsv:336[escalate] |
| MSH-CSSOM | mshtml!HTMLWindow6_put_XDomainRequest | dlls/mshtml/htmlwindow.c:2121 | fast-a.tsv:335[escalate] |
| MSH-CSSOM | mshtml!HTMLXMLHttpRequest_get_responseBody | dlls/mshtml/xmlhttprequest.c:541 | fast-a.tsv:420[escalate] |
| MSH-CSSOM | mshtml!HTMLXMLHttpRequest_private_get_response | dlls/mshtml/xmlhttprequest.c:1043 | fast-a.tsv:422[escalate] |
| MSH-CSSOM | mshtml!HTMLXMLHttpRequest_private_get_upload | dlls/mshtml/xmlhttprequest.c:1125 | fast-a.tsv:423[escalate] |
| MSH-CSSOM | mshtml!OmHistory_go | dlls/mshtml/omnavigator.c:554 | fast-a.tsv:305[escalate] |
| MSH-CSSOM | mshtml!SVGSVGElement_getEnclosureList | dlls/mshtml/svg.c:467 | fast-a.tsv:393[escalate] |
| MSH-CSSOM | mshtml!SVGSVGElement_getIntersectionList | dlls/mshtml/svg.c:459 | fast-a.tsv:392[escalate] |
| MSH-CSSOM | mshtml!SVGSVGElement_get_currentView | dlls/mshtml/svg.c:361 | fast-a.tsv:390[escalate] |
| MSH-CSSOM | mshtml!handle_extern_mime_navigation | dlls/mshtml/navigate.c:1637 | fast-a.tsv:58[escalate] |
| MSH-CSSOM | mshtml!on_change_dlcontrol | dlls/mshtml/oleobj.c:1143 | fast-a.tsv:575[escalate] |
| MSH-LEGACY | mshtml!DisplayServices_CreateDisplayPointer | dlls/mshtml/htmldoc.c:5989 | fast-a.tsv:355[escalate] |
| MSH-LEGACY | mshtml!DisplayServices_GetCaret | dlls/mshtml/htmldoc.c:6012 | fast-a.tsv:356[escalate] |
| MSH-LEGACY | mshtml!DisplayServices_GetComputedStyle | dlls/mshtml/htmldoc.c:6019 | fast-a.tsv:357[escalate] |
| MSH-LEGACY | mshtml!HTMLDocument4_createRenderStyle | dlls/mshtml/htmldoc.c:3366 | fast-a.tsv:353[escalate] |
| MSH-LEGACY | mshtml!HTMLDocument6_get_compatible | dlls/mshtml/htmldoc.c:3712 | fast-a.tsv:354[escalate] |
| MSH-LEGACY | mshtml!HTMLElement2_addBehavior | dlls/mshtml/htmlelem.c:4130 | fast-a.tsv:411[escalate] |
| MSH-LEGACY | mshtml!HTMLElement2_addFilter | dlls/mshtml/htmlelem.c:3500 | fast-a.tsv:408[escalate] |
| MSH-LEGACY | mshtml!HTMLElement2_createControlRange | dlls/mshtml/htmlelem.c:3733 | fast-a.tsv:410[escalate] |
| MSH-LEGACY | mshtml!HTMLElement2_getExpression | dlls/mshtml/htmlelem.c:3311 | fast-a.tsv:406[escalate] |
| MSH-LEGACY | mshtml!HTMLElement2_get_behaviorUrns | dlls/mshtml/htmlelem.c:4168 | fast-a.tsv:413[escalate] |
| MSH-LEGACY | mshtml!HTMLElement2_removeBehavior | dlls/mshtml/htmlelem.c:4138 | fast-a.tsv:412[escalate] |
| MSH-LEGACY | mshtml!HTMLElement2_removeExpression | dlls/mshtml/htmlelem.c:3319 | fast-a.tsv:407[escalate] |
| MSH-LEGACY | mshtml!HTMLElement2_removeFilter | dlls/mshtml/htmlelem.c:3507 | fast-a.tsv:409[escalate] |
| MSH-LEGACY | mshtml!HTMLElement2_setExpression | dlls/mshtml/htmlelem.c:3302 | fast-a.tsv:405[escalate] |
| MSH-LEGACY | mshtml!HTMLGenericElement_get_recordset | dlls/mshtml/htmlgeneric.c:50 | fast-a.tsv:350[escalate] |
| MSH-LEGACY | mshtml!HTMLGenericElement_namedRecordset | dlls/mshtml/htmlgeneric.c:57 | fast-a.tsv:351[escalate] |
| MSH-LEGACY | mshtml!HTMLNamespaceCollection_add | dlls/mshtml/omnavigator.c:2018 | fast-a.tsv:313[escalate] |
| MSH-LEGACY | mshtml!HTMLNamespaceCollection_get_length | dlls/mshtml/omnavigator.c:2003 | fast-a.tsv:569[escalate] |
| MSH-LEGACY | mshtml!HTMLNamespaceCollection_item | dlls/mshtml/omnavigator.c:2011 | fast-a.tsv:312[escalate] |
| MSH-LEGACY | mshtml!HTMLObjectElement2_namedRecordset | dlls/mshtml/htmlobject.c:517 | fast-a.tsv:304[escalate] |
| MSH-LEGACY | mshtml!HTMLObjectElement_get_recordset | dlls/mshtml/htmlobject.c:88 | fast-a.tsv:303[escalate] |
| MSH-LEGACY | mshtml!HTMLObjectElement_put_recordset | dlls/mshtml/htmlobject.c:81 | fast-a.tsv:302[escalate] |
| MSH-LEGACY | mshtml!HTMLStyle2_getExpression | dlls/mshtml/htmlstyle.c:4359 | fast-a.tsv:367[escalate] |
| MSH-LEGACY | mshtml!HTMLStyle2_removeExpression | dlls/mshtml/htmlstyle.c:4366 | fast-a.tsv:368[escalate] |
| MSH-LEGACY | mshtml!HTMLStyle2_setExpression | dlls/mshtml/htmlstyle.c:4352 | fast-a.tsv:366[escalate] |
| MSH-LEGACY | mshtml!HTMLTextContainer_createControlRange | dlls/mshtml/htmlbody.c:735 | fast-a.tsv:315[escalate] |
| MSH-LEGACY | mshtml!HTMLTxtRange_moveToPoint | dlls/mshtml/range.c:1850 | fast-a.tsv:361[escalate] |
| MSH-LEGACY | mshtml!HTMLWindow4_createPopup | dlls/mshtml/htmlwindow.c:1983 | fast-a.tsv:333[escalate] |
| MSH-LEGACY | mshtml!MarkupPointer2_MoveUnit | dlls/mshtml/range.c:4083 | fast-a.tsv:363[escalate] |
| MSH-LEGACY | mshtml!editsvcs_AddDesigner | dlls/mshtml/service.c:259 | fast-a.tsv:340[escalate] |
| MSH-LEGACY | mshtml!editsvcs_GetSelectionServices | dlls/mshtml/service.c:275 | fast-a.tsv:342[escalate] |
| MSH-LEGACY | mshtml!editsvcs_MoveToSelectionAnchor | dlls/mshtml/service.c:283 | fast-a.tsv:343[escalate] |
| MSH-LEGACY | mshtml!editsvcs_MoveToSelectionEnd | dlls/mshtml/service.c:291 | fast-a.tsv:344[escalate] |
| MSH-LEGACY | mshtml!editsvcs_RemoveDesigner | dlls/mshtml/service.c:267 | fast-a.tsv:341[escalate] |
| MSH-LEGACY | mshtml!editsvcs_SelectRange | dlls/mshtml/service.c:299 | fast-a.tsv:345[escalate] |
| N-DNS | dnsapi!DnsModifyRecordsInSet_A | dlls/dnsapi/main.c:56 | fast-b.tsv:781[escalate] |
| N-DNS | dnsapi!DnsModifyRecordsInSet_UTF8 | dlls/dnsapi/main.c:67 | fast-b.tsv:782[escalate] |
| N-DNS | dnsapi!DnsModifyRecordsInSet_W | dlls/dnsapi/main.c:78 | fast-b.tsv:783[escalate] |
| N-DNS | dnsapi!DnsReplaceRecordSetA | dlls/dnsapi/main.c:89 | fast-b.tsv:784[escalate] |
| N-DNS | dnsapi!DnsReplaceRecordSetUTF8 | dlls/dnsapi/main.c:100 | fast-b.tsv:2289[escalate] |
| N-DNS | dnsapi!DnsReplaceRecordSetW | dlls/dnsapi/main.c:111 | fast-b.tsv:2290[escalate] |
| N-DNS | dnsapi!DnsServiceBrowse | dlls/dnsapi/main.c:122 | fast-b.tsv:787[escalate] |
| N-DOMAIN | adsldp!search_SetSearchPreference | dlls/adsldp/adsldp.c:1136 | fast-a.tsv:592[escalate] |
| N-DOMAIN | netapi32!DsGetDcNameW | dlls/netapi32/netapi32.c:2755 | fast-a.tsv:121[escalate] |
| N-DOMAIN | objsel!OBJSEL_IDsObjectPicker_Initialize | dlls/objsel/objsel.c:113 | triage-7.tsv:188[escalate] |
| N-DOMAIN | objsel!OBJSEL_IDsObjectPicker_InvokeDialog | dlls/objsel/objsel.c:125 | triage-7.tsv:189[escalate] |
| N-DOMAIN | wldap32!WLDAP32_ldap_connect | dlls/wldap32/init.c:251 | triage-7.tsv:203[escalate] |
| N-MISC | connect!IsInternetConnected | dlls/connect/main.c:28 | triage-7.tsv:363[escalate] |
| N-MISC | ieframe!NewWindowManager_EvaluateNewWindow | dlls/ieframe/shellbrowser.c:1026 | fast-a.tsv:564[escalate] |
| N-MISC | ieframe!ShellUIHelper2_AddSearchProvider | dlls/ieframe/shelluihelper.c:289 | fast-a.tsv:288[escalate] |
| N-MISC | mgmtapi!SnmpMgrOpen | dlls/mgmtapi/mgmtapi.c:27 | triage-7.tsv:374[escalate] |
| N-MISC | mgmtapi!SnmpMgrTrapListen | dlls/mgmtapi/mgmtapi.c:33 | triage-7.tsv:375[escalate] |
| N-MISC | sensapi!IsDestinationReachableA | dlls/sensapi/sensapi.c:37 | triage-7.tsv:175[escalate] |
| N-MISC | sensapi!IsDestinationReachableW | dlls/sensapi/sensapi.c:42 | triage-7.tsv:176[escalate] |
| N-MISC | urlmon!ZoneMgrImpl_CopyTemplatePoliciesToZone | dlls/urlmon/sec_mgr.c:2310 | fast-a.tsv:168[escalate] |
| N-MISC* | wpcap!pcap_findalldevs_ex | dlls/wpcap/wpcap.c:685 | fast-a-cloud.tsv:91[escalate]; triage-7.tsv:438[no-ground-truth] |
| N-MISC* | wpcap!pcap_open | dlls/wpcap/wpcap.c:1085 | fast-a-cloud.tsv:90[escalate]; triage-7.tsv:439[no-ground-truth] |
| N-MISC | ws2_32!WS2_DisconnectEx | dlls/ws2_32/socket.c:1722 | fast-b.tsv:2200[escalate]; triage-7.tsv:110[escalate] |
| N-RRAS | mprapi!MprAdminEstablishDomainRasServer | dlls/mprapi/mprapi.c:576 | fast-b.tsv:2335[escalate] |
| N-RRAS | mprapi!MprAdminGetPDCServer | dlls/mprapi/mprapi.c:582 | fast-b.tsv:2336[escalate] |
| N-RRAS | mprapi!MprAdminInterfaceGetCredentials | dlls/mprapi/mprapi.c:630 | fast-b.tsv:2337[escalate] |
| N-RRAS | mprapi!MprAdminInterfaceSetCredentials | dlls/mprapi/mprapi.c:660 | fast-b.tsv:2338[escalate] |
| N-RRAS | mprapi!MprAdminIsDomainRasServer | dlls/mprapi/mprapi.c:714 | fast-b.tsv:2339[escalate] |
| N-RRAS | mprapi!MprAdminUserGetInfo | dlls/mprapi/mprapi.c:834 | fast-b.tsv:2340[escalate] |
| N-RRAS | mprapi!MprAdminUserSetInfo | dlls/mprapi/mprapi.c:840 | fast-b.tsv:2341[escalate] |
| N-RRAS | mprapi!MprConfigServerInstall | dlls/mprapi/mprapi.c:924 | fast-b.tsv:2342[escalate] |
| N-RRAS | mprapi!MprSetupIpInIpInterfaceFriendlyNameCreate | dlls/mprapi/mprapi.c:960 | fast-b.tsv:2343[escalate] |
| N-RRAS | mprapi!MprSetupIpInIpInterfaceFriendlyNameDelete | dlls/mprapi/mprapi.c:966 | fast-b.tsv:2344[escalate] |
| N-RRAS | mprapi!MprSetupIpInIpInterfaceFriendlyNameEnum | dlls/mprapi/mprapi.c:972 | fast-b.tsv:2345[escalate] |
| N-WEBSERVICES | webservices!WsCreateServiceProxyFromTemplate | dlls/webservices/proxy.c:151 | fast-a.tsv:655[escalate] |
| N-WEBSERVICES | webservices!read_type | dlls/webservices/reader.c:6920 | fast-a.tsv:681[escalate] |
| N-WEBSERVICES | webservices!write_type | dlls/webservices/writer.c:3909 | fast-a.tsv:679[escalate] |
| N-WEBSERVICES | wsdapi!WSDCreateDiscoveryProvider | dlls/wsdapi/discovery.c:473 | fast-b.tsv:719[escalate] |
| N-WEBSOCKET | websocket!WebSocketAbortHandle | dlls/websocket/websocket.c:33 | triage-7.tsv:181[escalate] |
| N-WEBSOCKET | websocket!WebSocketCreateClientHandle | dlls/websocket/websocket.c:25 | triage-7.tsv:180[escalate] |
| N-WEBSOCKET | websocket!WebSocketDeleteHandle | dlls/websocket/websocket.c:38 | triage-7.tsv:182[escalate] |
| N-WFP | fwpuclnt!FwpmEngineClose0 | dlls/fwpuclnt/main.c | triage-7.tsv:402[escalate] |
| N-WFP | fwpuclnt!FwpmEngineOpen0 | dlls/fwpuclnt/main.c | triage-7.tsv:401[escalate] |
| N-WFP | fwpuclnt!FwpmFilterAdd0 | dlls/fwpuclnt/main.c | triage-7.tsv:406[escalate] |
| N-WFP | fwpuclnt!FwpmFilterCreateEnumHandle0 | dlls/fwpuclnt/main.c | triage-7.tsv:407[escalate] |
| N-WFP | fwpuclnt!FwpmFilterDestroyEnumHandle0 | dlls/fwpuclnt/main.c | triage-7.tsv:409[escalate] |
| N-WFP | fwpuclnt!FwpmFilterEnum0 | dlls/fwpuclnt/main.c | triage-7.tsv:408[escalate] |
| N-WFP | fwpuclnt!FwpmFreeMemory0 | dlls/fwpuclnt/main.c | triage-7.tsv:403[escalate] |
| N-WFP | fwpuclnt!FwpmSubLayerAdd0 | dlls/fwpuclnt/main.c | triage-7.tsv:404[escalate] |
| N-WFP | fwpuclnt!FwpmSubLayerGetByKey0 | dlls/fwpuclnt/main.c | triage-7.tsv:405[escalate] |
| N-WFP | traffic!TcDeregisterClient | dlls/traffic/traffic_main.c:43 | triage-7.tsv:377[escalate] |
| N-WFP | traffic!TcRegisterClient | dlls/traffic/traffic_main.c:32 | triage-7.tsv:376[escalate] |
| NET-DBG | mscoree!CLRCreateInstance | dlls/mscoree/mscoree_main.c:715 | fast-b.tsv:705[escalate] |
| NET-DBG | mscoree!CorDebug_DebugActiveProcess | dlls/mscoree/cordebug.c:710 | fast-b.tsv:2086[escalate] |
| NET-DBG | mscoree!cordebugprocess_ClearCurrentException | dlls/mscoree/cordebug.c:312 | fast-b.tsv:2079[escalate] |
| NET-DBG | mscoree!cordebugprocess_EnableLogMessages | dlls/mscoree/cordebug.c:320 | fast-b.tsv:2080[escalate] |
| NET-DBG | mscoree!cordebugprocess_EnumerateAppDomains | dlls/mscoree/cordebug.c:336 | fast-b.tsv:2082[escalate] |
| NET-DBG | mscoree!cordebugprocess_EnumerateObjects | dlls/mscoree/cordebug.c:254 | fast-b.tsv:2077[escalate] |
| NET-DBG | mscoree!cordebugprocess_EnumerateThreads | dlls/mscoree/cordebug.c:163 | fast-b.tsv:2074[escalate] |
| NET-DBG | mscoree!cordebugprocess_GetHelperThreadID | dlls/mscoree/cordebug.c:360 | fast-b.tsv:2085[escalate] |
| NET-DBG | mscoree!cordebugprocess_GetObject | dlls/mscoree/cordebug.c:344 | fast-b.tsv:2083[escalate] |
| NET-DBG | mscoree!cordebugprocess_GetThread | dlls/mscoree/cordebug.c:246 | fast-b.tsv:2076[escalate] |
| NET-DBG | mscoree!cordebugprocess_IsTransitionStub | dlls/mscoree/cordebug.c:262 | fast-b.tsv:2078[escalate] |
| NET-DBG | mscoree!cordebugprocess_ModifyLogSwitch | dlls/mscoree/cordebug.c:328 | fast-b.tsv:2081[escalate] |
| NET-DBG | mscoree!cordebugprocess_SetAllThreadsDebugState | dlls/mscoree/cordebug.c:171 | fast-b.tsv:2075[escalate] |
| NET-DBG | mscoree!cordebugprocess_Stop | dlls/mscoree/cordebug.c:130 | fast-b.tsv:2073[escalate] |
| NET-DBG | mscoree!cordebugprocess_ThreadForFiberCookie | dlls/mscoree/cordebug.c:352 | fast-b.tsv:2084[escalate] |
| NET-HOST | mscoree!CLRRuntimeHost_ExecuteApplication | dlls/mscoree/corruntimehost.c:1017 | fast-b.tsv:675[escalate] |
| NET-HOST | mscoree!CLRRuntimeHost_GetCLRControl | dlls/mscoree/corruntimehost.c:948 | fast-b.tsv:673[escalate] |
| NET-HOST | mscoree!CLRRuntimeHost_SetHostControl | dlls/mscoree/corruntimehost.c:941 | fast-b.tsv:672[escalate] |
| NET-HOST | mscoree!CLRRuntimeHost_UnloadAppDomain | dlls/mscoree/corruntimehost.c:955 | fast-b.tsv:674[escalate] |
| NET-HOST | mscoree!CoEEShutDownCOM | dlls/mscoree/mscoree_main.c:1000 | fast-b.tsv:680[escalate] |
| NET-HOST | mscoree!RuntimeHost_GetInterface | dlls/mscoree/corruntimehost.c:1758 | fast-b.tsv:704[escalate] |
| NET-HOST | mscoree!StrongNameSignatureVerification | dlls/mscoree/mscoree_main.c:657 | fast-b.tsv:681[escalate] |
| NET-HOST | mscoree!StrongNameSignatureVerificationEx | dlls/mscoree/mscoree_main.c:663 | fast-b.tsv:682[escalate] |
| NET-HOST | mscoree!StrongNameTokenFromAssembly | dlls/mscoree/mscoree_main.c:670 | fast-b.tsv:683[escalate] |
| NET-HOST | mscoree!corruntimehost_UnloadDomain | dlls/mscoree/corruntimehost.c:831 | fast-b.tsv:671[escalate] |
| NET-HOST | msvcm80!CrtImplementationDetails_DoDllLanguageSupportValidation | dlls/msvcm80/msvcm_main.c:29 | triage-5.tsv:85[escalate] |
| NET-HOST | msvcm80!CrtImplementationDetails_RegisterModuleUninitializer | dlls/msvcm80/msvcm_main.c:35 | triage-5.tsv:86[escalate] |
| NET-HOST | msvcm80!CrtImplementationDetails_ThrowModuleLoadException | dlls/msvcm80/msvcm_main.c:48 | triage-5.tsv:88[escalate] |
| NET-HOST | msvcm80!CrtImplementationDetails_ThrowModuleLoadException_inner | dlls/msvcm80/msvcm_main.c:54 | triage-5.tsv:89[escalate] |
| NET-HOST | msvcm80!CrtImplementationDetails_ThrowNestedModuleLoadException | dlls/msvcm80/msvcm_main.c:60 | triage-5.tsv:90[escalate] |
| NET-HOST | msvcm80!__setusermatherr_m | dlls/msvcm80/msvcm_main.c:42 | triage-5.tsv:87[escalate] |
| NET-META | mscoree!GetAssemblyMDImport | dlls/mscoree/mscoree_main.c:516 | fast-b.tsv:684[escalate] |
| NET-META | mscoree!MetaDataDispenser_DefineScope | dlls/mscoree/metadata.c:101 | fast-b.tsv:662[escalate] |
| NET-META | mscoree!MetaDataDispenser_FindAssembly | dlls/mscoree/metadata.c:153 | fast-b.tsv:669[escalate] |
| NET-META | mscoree!MetaDataDispenser_FindAssemblyModule | dlls/mscoree/metadata.c:163 | fast-b.tsv:670[escalate] |
| NET-META | mscoree!MetaDataDispenser_GetCORSystemDirectory | dlls/mscoree/metadata.c:146 | fast-b.tsv:668[escalate] |
| NET-META | mscoree!MetaDataDispenser_GetOption | dlls/mscoree/metadata.c:132 | fast-b.tsv:666[escalate] |
| NET-META | mscoree!MetaDataDispenser_OpenScope | dlls/mscoree/metadata.c:109 | fast-b.tsv:663[escalate] |
| NET-META | mscoree!MetaDataDispenser_OpenScopeOnITypeInfo | dlls/mscoree/metadata.c:139 | fast-b.tsv:667[escalate] |
| NET-META | mscoree!MetaDataDispenser_OpenScopeOnMemory | dlls/mscoree/metadata.c:117 | fast-b.tsv:664[escalate] |
| NET-META | mscoree!MetaDataDispenser_SetOption | dlls/mscoree/metadata.c:125 | fast-b.tsv:665[escalate] |
| NET-META | rometadata!MetaDataDispenser_DefineScope | dlls/rometadata/main.c:98 | fast-b.tsv:1033[escalate] |
| NET-META | rometadata!metadata_open_memory | dlls/rometadata/metadata.c:2250 | fast-b.tsv:2296[escalate] |
| P-APPX | appxpackaging!appx_factory_CreateBlockMapReader | dlls/appxpackaging/main.c:82 | triage-7.tsv:27[escalate] |
| P-APPX | appxpackaging!appx_factory_CreatePackageWriter | dlls/appxpackaging/main.c:48 | triage-7.tsv:26[escalate] |
| P-APPX | appxpackaging!appx_factory_CreateValidatedBlockMapReader | dlls/appxpackaging/main.c:88 | triage-7.tsv:28[escalate] |
| P-APPX | appxpackaging!appx_factory_QueryInterface | dlls/appxpackaging/main.c:32 | triage-7.tsv:25[escalate] |
| P-APPX | appxpackaging!bundle_GetBlockMap | dlls/appxpackaging/package.c:1456 | triage-7.tsv:33[escalate] |
| P-APPX | appxpackaging!bundle_factory_CreateBundleWriter | dlls/appxpackaging/main.c:127 | triage-7.tsv:30[escalate] |
| P-APPX | appxpackaging!bundle_factory_QueryInterface | dlls/appxpackaging/main.c:111 | triage-7.tsv:29[escalate] |
| P-APPX | appxpackaging!manifest_GetDeviceCapabilities | dlls/appxpackaging/manifest.c:877 | triage-7.tsv:34[escalate] |
| P-APPX | appxpackaging!package_GetBlockMap | dlls/appxpackaging/package.c:624 | triage-7.tsv:32[escalate] |
| P-APPX | appxpackaging!package_QueryInterface | dlls/appxpackaging/package.c:585 | triage-7.tsv:31[escalate] |
| P-APPX | opcservices!opc_factory_CreateDigitalSignatureManager | dlls/opcservices/factory.c:435 | fast-b.tsv:2356[escalate] |
| P-CAB | cabinet!FDITruncateCabinet | dlls/cabinet/fdi.c:2850 | triage-7.tsv:206[escalate] |
| P-CAB | cabinet!create_handle | dlls/cabinet/compressapi.c:90 | triage-7.tsv:207[escalate] |
| P-MISCINST | imagehlp!BindImageEx | dlls/imagehlp/modify.c:48 | fast-b.tsv:2181[escalate] |
| P-MISCINST | itss!ITStorageImpl_StgCreateDocfileOnILockBytes | dlls/itss/itss.c:241 | fast-b.tsv:1048[escalate] |
| P-MISCINST | mstask!MSTASK_ITask_GetTaskFlags | dlls/mstask/task.c:1224 | fast-b.tsv:2183[escalate] |
| P-MISCINST | mstask!MSTASK_ITask_SetTaskFlags | dlls/mstask/task.c:1216 | fast-b.tsv:2182[escalate] |
| P-MISCINST* | sfc_os!SfcIsFileProtected | dlls/sfc_os/sfc_os.c:64 | fast-a-cloud.tsv:85[escalate]; triage-7.tsv:183[windows-matches] |
| P-MISCINST* | sfc_os!SfcIsKeyProtected | dlls/sfc_os/sfc_os.c:98 | fast-a-cloud.tsv:86[escalate]; triage-7.tsv:184[windows-matches] |
| P-MISCINST | wimgapi!WIMCreateFile | dlls/wimgapi/main.c:44 | fast-a-cloud.tsv:107[escalate]; triage-7.tsv:444[escalate] |
| P-MISCINST* | wimgapi!WIMGetMountedImages | dlls/wimgapi/main.c:37 | fast-a-cloud.tsv:106[no-ground-truth]; triage-7.tsv:443[escalate] |
| P-MISCINST* | wimgapi!WIMRegisterMessageCallback | dlls/wimgapi/main.c:31 | fast-a-cloud.tsv:105[no-ground-truth]; triage-7.tsv:442[escalate] |
| P-SDB | apphelp!SdbCloseDatabase | dlls/apphelp/apphelp.c:95 | fast-b.tsv:1917[escalate]; triage-7.tsv:22[escalate] |
| P-SDB | apphelp!SdbCreateDatabase | dlls/apphelp/apphelp.c:54 | fast-b.tsv:1913[escalate]; triage-7.tsv:19[escalate] |
| P-SDB | apphelp!SdbGetFirstChild | dlls/apphelp/apphelp.c:89 | fast-b.tsv:1916[escalate]; triage-7.tsv:23[escalate] |
| P-SDB | apphelp!SdbInitDatabase | dlls/apphelp/apphelp.c:75 | fast-b.tsv:1914[escalate]; triage-7.tsv:21[escalate] |
| P-SDB | apphelp!SdbOpenDatabase | dlls/apphelp/apphelp.c:82 | fast-b.tsv:1915[escalate]; triage-7.tsv:20[escalate] |
| P-SDB | sdbinst!wmain | programs/sdbinst/main.c:23 | triage-7.tsv:258[escalate] |
| P-STUBPROG* | extrac32!wWinMain | ? | recheck-fast-a.tsv:198[escalate]; triage-7.tsv:242[no-ground-truth] |
| P-STUBPROG | mofcomp!wmain | programs/mofcomp/mofcomp.c:28 | triage-7.tsv:251[escalate] |
| P-STUBPROG | netsh!wmain | programs/netsh/netsh.c:849 | triage-7.tsv:248[escalate] |
| P-STUBPROG | powershell!wmain | programs/powershell/main.c:116 | triage-7.tsv:268[escalate] |
| P-STUBPROG | printui!PrintUIEntryW | dlls/printui/printui.c:247 | triage-7.tsv:387[escalate] |
| P-STUBPROG | regasm!wmain | programs/regasm/main.c:23 | triage-7.tsv:253[escalate] |
| P-STUBPROG | regini!wmain | programs/regini/main.c:27 | triage-7.tsv:252[escalate] |
| P-STUBPROG | regsvcs!wmain | programs/regsvcs/main.c:23 | triage-7.tsv:254[escalate] |
| P-STUBPROG | subst!wmain | programs/subst/main.c:23 | triage-7.tsv:212[escalate] |
| P-STUBPROG | wevtutil!wmain | programs/wevtutil/main.c:23 | triage-7.tsv:259[escalate] |
| P-STUBPROG | wmplayer!wmain | programs/wmplayer/main.c:78 | triage-5.tsv:100[escalate] |
| PRN* | localspl!fpSetJob | dlls/localspl/provider.c:4003 | fast-b.tsv:922[no-ground-truth]; recheck-fast-a.tsv:203[escalate] |
| PRN | localspl!localmon_EndDocPort | dlls/localspl/localmon.c:585 | fast-b.tsv:929[escalate] |
| PRN | localspl!localmon_WritePort | dlls/localspl/localmon.c:549 | fast-b.tsv:928[escalate] |
| PRN | unidrv!core_DrvGetGPDData | dlls/unidrv/oem.c:117 | triage-7.tsv:388[escalate] |
| PRN | winprint!ControlPrintProcessor | dlls/winprint/printproc.c:213 | fast-a-cloud.tsv:58[escalate]; triage-7.tsv:394[escalate] |
| PRN | winspool.drv!PerfClose | dlls/winspool.drv/info.c:9199 | fast-b.tsv:1154[escalate] |
| PRN | winspool.drv!PerfCollect | dlls/winspool.drv/info.c:9208 | fast-b.tsv:1155[escalate] |
| PRN | winspool.drv!PerfOpen | dlls/winspool.drv/info.c:9190 | fast-b.tsv:1153[escalate] |
| PRN | winspool.drv!SetPrinterW | dlls/winspool.drv/info.c:3694 | fast-b.tsv:2167[escalate] |
| RDS | wtsapi32!WTSConnectSessionA | dlls/wtsapi32/wtsapi32.c:320 | triage-6.tsv:536[escalate] |
| RDS | wtsapi32!WTSConnectSessionW | dlls/wtsapi32/wtsapi32.c:329 | triage-6.tsv:537[escalate] |
| RDS | wtsapi32!WTSEnumerateProcessesExW | dlls/wtsapi32/wtsapi32.c:535 | triage-6.tsv:639[escalate] |
| RDS | wtsapi32!WTSStartRemoteControlSessionA | dlls/wtsapi32/wtsapi32.c:1904 | triage-6.tsv:538[escalate] |
| RDS | wtsapi32!WTSStartRemoteControlSessionW | dlls/wtsapi32/wtsapi32.c:1913 | triage-6.tsv:539[escalate] |
| RDS | wtsapi32!WTSStopRemoteControlSession | dlls/wtsapi32/wtsapi32.c:1922 | triage-6.tsv:540[escalate] |
| RDS | wtsapi32!WTSVirtualChannelClose | dlls/wtsapi32/wtsapi32.c:2009 | triage-6.tsv:541[escalate] |
| RDS | wtsapi32!WTSVirtualChannelOpen | dlls/wtsapi32/wtsapi32.c:2018 | triage-6.tsv:542[escalate] |
| RDS | wtsapi32!WTSVirtualChannelOpenEx | dlls/wtsapi32/wtsapi32.c:2027 | triage-6.tsv:543[escalate] |
| RDS | wtsapi32!WTSVirtualChannelPurgeInput | dlls/wtsapi32/wtsapi32.c:2036 | triage-6.tsv:544[escalate] |
| RDS | wtsapi32!WTSVirtualChannelPurgeOutput | dlls/wtsapi32/wtsapi32.c:2045 | triage-6.tsv:545[escalate] |
| RDS | wtsapi32!WTSVirtualChannelQuery | dlls/wtsapi32/wtsapi32.c:2055 | triage-6.tsv:546[escalate] |
| RDS | wtsapi32!WTSVirtualChannelRead | dlls/wtsapi32/wtsapi32.c:2064 | triage-6.tsv:547[escalate] |
| RDS | wtsapi32!WTSVirtualChannelWrite | dlls/wtsapi32/wtsapi32.c:2073 | triage-6.tsv:548[escalate] |
| RDS | wtsapi32!WTSWaitSystemEvent | dlls/wtsapi32/wtsapi32.c:2082 | triage-6.tsv:549[escalate] |
| RE-OLE | riched20!IOleInPlaceSite_fnCanInPlaceActivate | dlls/riched20/richole.c:1140 | fast-b.tsv:1283[escalate] |
| RE-OLE | riched20!IOleInPlaceSite_fnContextSensitiveHelp | dlls/riched20/richole.c:1133 | fast-b.tsv:1282[escalate] |
| RE-OLE | riched20!IOleInPlaceSite_fnDeactivateAndUndo | dlls/riched20/richole.c:1198 | fast-b.tsv:1291[escalate] |
| RE-OLE | riched20!IOleInPlaceSite_fnDiscardUndoState | dlls/riched20/richole.c:1191 | fast-b.tsv:1290[escalate] |
| RE-OLE | riched20!IOleInPlaceSite_fnGetWindow | dlls/riched20/richole.c:1118 | fast-b.tsv:1293[escalate] |
| RE-OLE | riched20!IOleInPlaceSite_fnGetWindowContext | dlls/riched20/richole.c:1161 | fast-b.tsv:1286[escalate] |
| RE-OLE | riched20!IOleInPlaceSite_fnOnInPlaceActivate | dlls/riched20/richole.c:1147 | fast-b.tsv:1284[escalate] |
| RE-OLE | riched20!IOleInPlaceSite_fnOnInPlaceDeactivate | dlls/riched20/richole.c:1184 | fast-b.tsv:1289[escalate] |
| RE-OLE | riched20!IOleInPlaceSite_fnOnPosRectChange | dlls/riched20/richole.c:1205 | fast-b.tsv:1292[escalate] |
| RE-OLE | riched20!IOleInPlaceSite_fnOnUIActivate | dlls/riched20/richole.c:1154 | fast-b.tsv:1285[escalate] |
| RE-OLE | riched20!IOleInPlaceSite_fnOnUIDeactivate | dlls/riched20/richole.c:1177 | fast-b.tsv:1288[escalate] |
| RE-OLE | riched20!IOleInPlaceSite_fnScroll | dlls/riched20/richole.c:1170 | fast-b.tsv:1287[escalate] |
| RE-OLE | riched20!IRichEditOle_fnActivateAs | dlls/riched20/richole.c:948 | fast-b.tsv:1263[escalate] |
| RE-OLE | riched20!IRichEditOle_fnContextSensitiveHelp | dlls/riched20/richole.c:956 | fast-b.tsv:1264[escalate] |
| RE-OLE | riched20!IRichEditOle_fnConvertObject | dlls/riched20/richole.c:964 | fast-b.tsv:1265[escalate] |
| RE-OLE | riched20!IRichEditOle_fnGetLinkCount | dlls/riched20/richole.c:1287 | fast-b.tsv:1269[escalate] |
| RE-OLE | riched20!IRichEditOle_fnHandsOffStorage | dlls/riched20/richole.c:1352 | fast-b.tsv:1270[escalate] |
| RE-OLE | riched20!IRichEditOle_fnImportDataObject | dlls/riched20/richole.c:1360 | fast-b.tsv:1271[escalate] |
| RE-OLE | riched20!IRichEditOle_fnInPlaceDeactivate | dlls/riched20/richole.c:1369 | fast-b.tsv:1272[escalate] |
| RE-OLE | riched20!IRichEditOle_fnSaveCompleted | dlls/riched20/richole.c:1399 | fast-b.tsv:1273[escalate] |
| RE-OLE | riched20!IRichEditOle_fnSetDvaspect | dlls/riched20/richole.c:1407 | fast-b.tsv:1274[escalate] |
| RE-OLE | riched20!IRichEditOle_fnSetHostNames | dlls/riched20/richole.c:1415 | fast-b.tsv:1275[escalate] |
| RE-OLE | riched20!IRichEditOle_fnSetLinkAvailable | dlls/riched20/richole.c:1423 | fast-b.tsv:1276[escalate] |
| RE-OLE | riched20!fnTextSrv_TxGetDropTarget | dlls/riched20/txtsrv.c:562 | fast-b.tsv:2196[escalate] |
| RE-TOM | riched20!IOleClientSite_fnGetContainer | dlls/riched20/richole.c:1046 | fast-b.tsv:1268[escalate] |
| RE-TOM | riched20!IOleClientSite_fnGetMoniker | dlls/riched20/richole.c:1035 | fast-b.tsv:1267[escalate] |
| RE-TOM | riched20!IOleClientSite_fnSaveObject | dlls/riched20/richole.c:1025 | fast-b.tsv:1266[escalate] |
| RE-TOM | riched20!ITextDocument2Old_fnAttachMsgFilter | dlls/riched20/richole.c:5699 | fast-b.tsv:2036[escalate] |
| RE-TOM | riched20!ITextDocument2Old_fnGetPreferredFont | dlls/riched20/richole.c:5762 | fast-b.tsv:2037[escalate] |
| RE-TOM | riched20!ITextDocument2_fnGetActiveStory | dlls/riched20/richole.c:6320 | fast-b.tsv:2050[escalate] |
| RE-TOM | riched20!ITextDocument2_fnGetCallManager | dlls/riched20/richole.c:6198 | fast-b.tsv:2045[escalate] |
| RE-TOM | riched20!ITextDocument2_fnGetDisplays | dlls/riched20/richole.c:6088 | fast-b.tsv:2038[escalate] |
| RE-TOM | riched20!ITextDocument2_fnGetDocumentFont | dlls/riched20/richole.c:6094 | fast-b.tsv:2039[escalate] |
| RE-TOM | riched20!ITextDocument2_fnGetDocumentPara | dlls/riched20/richole.c:6106 | fast-b.tsv:2041[escalate] |
| RE-TOM | riched20!ITextDocument2_fnGetMainStory | dlls/riched20/richole.c:6332 | fast-b.tsv:2052[escalate] |
| RE-TOM | riched20!ITextDocument2_fnGetNewStory | dlls/riched20/richole.c:6338 | fast-b.tsv:2053[escalate] |
| RE-TOM | riched20!ITextDocument2_fnGetProperty | dlls/riched20/richole.c:6224 | fast-b.tsv:2034[escalate] |
| RE-TOM | riched20!ITextDocument2_fnGetSelection2 | dlls/riched20/richole.c:6156 | fast-b.tsv:2043[escalate] |
| RE-TOM | riched20!ITextDocument2_fnGetStory | dlls/riched20/richole.c:6344 | fast-b.tsv:2054[escalate] |
| RE-TOM | riched20!ITextDocument2_fnGetStoryRanges2 | dlls/riched20/richole.c:6162 | fast-b.tsv:2044[escalate] |
| RE-TOM | riched20!ITextDocument2_fnGetStrings | dlls/riched20/richole.c:6230 | fast-b.tsv:2047[escalate] |
| RE-TOM | riched20!ITextDocument2_fnRange2 | dlls/riched20/richole.c:6242 | fast-b.tsv:2048[escalate] |
| RE-TOM | riched20!ITextDocument2_fnRangeFromPoint2 | dlls/riched20/richole.c:6248 | fast-b.tsv:2049[escalate] |
| RE-TOM | riched20!ITextDocument2_fnReleaseCallManager | dlls/riched20/richole.c:6254 | fast-b.tsv:2046[escalate] |
| RE-TOM | riched20!ITextDocument2_fnSetActiveStory | dlls/riched20/richole.c:6326 | fast-b.tsv:2051[escalate] |
| RE-TOM | riched20!ITextDocument2_fnSetDocumentFont | dlls/riched20/richole.c:6100 | fast-b.tsv:2040[escalate] |
| RE-TOM | riched20!ITextDocument2_fnSetDocumentPara | dlls/riched20/richole.c:6112 | fast-b.tsv:2042[escalate] |
| RE-TOM | riched20!ITextDocument2_fnSetProperty | dlls/riched20/richole.c:6270 | fast-b.tsv:2035[escalate] |
| RE-TOM | riched20!ITextRange_fnGetEmbeddedObject | dlls/riched20/richole.c:3162 | fast-b.tsv:1277[escalate] |
| RE-TOM | riched20!ITextRange_fnGetFormattedText | dlls/riched20/richole.c:1705 | fast-b.tsv:2299[escalate] |
| RE-TOM | riched20!ITextRange_fnSetFormattedText | dlls/riched20/richole.c:1717 | fast-b.tsv:2301[escalate] |
| RE-TOM | riched20!ITextRange_fnSetPoint | dlls/riched20/richole.c:3117 | fast-b.tsv:1298[escalate] |
| RE-TOM | riched20!ITextSelection_fnGetEmbeddedObject | dlls/riched20/richole.c:7515 | fast-b.tsv:1278[escalate] |
| RE-TOM | riched20!ITextSelection_fnGetFormattedText | dlls/riched20/richole.c:6641 | fast-b.tsv:2300[escalate] |
| RE-TOM | riched20!ITextSelection_fnMoveDown | dlls/riched20/richole.c:7627 | fast-b.tsv:2306[escalate] |
| RE-TOM | riched20!ITextSelection_fnMoveLeft | dlls/riched20/richole.c:7570 | fast-b.tsv:2303[escalate] |
| RE-TOM | riched20!ITextSelection_fnMoveRight | dlls/riched20/richole.c:7589 | fast-b.tsv:2304[escalate] |
| RE-TOM | riched20!ITextSelection_fnMoveUp | dlls/riched20/richole.c:7608 | fast-b.tsv:2305[escalate] |
| RE-TOM | riched20!ITextSelection_fnSetFormattedText | dlls/riched20/richole.c:6653 | fast-b.tsv:2302[escalate] |
| RE-TOM | riched20!ITextSelection_fnSetPoint | dlls/riched20/richole.c:7462 | fast-b.tsv:1299[escalate] |
| RE-TOM | riched20!textrange_delete | dlls/riched20/richole.c:2597 | fast-b.tsv:1351[escalate] |
| RPC-MGMT | rpcrt4!RpcErrorLoadErrorInfo | dlls/rpcrt4/rpcrt4_main.c:921 | triage-6.tsv:398[escalate] |
| RPC-MGMT | rpcrt4!RpcErrorStartEnumeration | dlls/rpcrt4/rpcrt4_main.c:894 | triage-6.tsv:395[escalate] |
| RPC-MGMT | rpcrt4!RpcMgmtEpEltInqBegin | dlls/rpcrt4/rpc_server.c:1693 | triage-6.tsv:393[escalate] |
| RPC-MGMT | rpcrt4!RpcMgmtInqIfIds | dlls/rpcrt4/rpc_server.c:1650 | triage-6.tsv:893[escalate] |
| RPC-MGMT | rpcrt4!RpcMgmtSetCancelTimeout | dlls/rpcrt4/rpcrt4_main.c:939 | triage-6.tsv:400[escalate] |
| RPC-MGMT | rpcrt4!RpcMgmtSetServerStackSize | dlls/rpcrt4/rpc_server.c:1734 | triage-6.tsv:907[escalate] |
| RPC-MGMT | rpcrt4!RpcMgmtStopServerListening | dlls/rpcrt4/rpc_server.c:1596 | triage-6.tsv:388[escalate] |
| RPC-NDR | rpcrt4!I_RpcAsyncAbortCall | dlls/rpcrt4/rpc_message.c:1981 | triage-6.tsv:912[escalate] |
| RPC-NDR | rpcrt4!NdrAsyncServerCall | dlls/rpcrt4/ndr_stubless.c:1905 | triage-6.tsv:405[escalate] |
| RPC-NDR | rpcrt4!NdrAsyncStubCall | dlls/rpcrt4/ndr_stubless.c:1897 | triage-6.tsv:404[escalate] |
| RPC-NDR | rpcrt4!NdrConvert | dlls/rpcrt4/ndr_marshall.c:4670 | triage-6.tsv:384[escalate] |
| RPC-NDR | rpcrt4!NdrConvert2 | dlls/rpcrt4/ndr_marshall.c:4680 | triage-6.tsv:385[escalate] |
| RPC-NDR | rpcrt4!NdrServerCallAll | dlls/rpcrt4/ndr_stubless.c:1512 | triage-6.tsv:402[escalate] |
| RPC-NDR | rpcrt4!NdrStubCall2 | dlls/rpcrt4/ndr_stubless.c:1228 | triage-6.tsv:34[escalate] |
| RPC-NDR | rpcrt4!PointerUnmarshall | dlls/rpcrt4/ndr_marshall.c:882 | triage-6.tsv:383[escalate] |
| RPC-NDR | rpcrt4!RpcAsyncAbortCall | dlls/rpcrt4/rpc_async.c:156 | triage-6.tsv:911[escalate] |
| RPC-NDR | rpcrt4!RpcAsyncCancelCall | dlls/rpcrt4/rpc_async.c:176 | triage-6.tsv:913[escalate] |
| RPC-NDR | rpcrt4!ndr_client_call | dlls/rpcrt4/ndr_stubless.c:705 | triage-6.tsv:401[escalate] |
| RPC-SEC | rpcrt4!RPCRT4_default_inquire_auth_client | dlls/rpcrt4/rpc_message.c:1183 | triage-6.tsv:387[escalate] |
| RPC-SEC | rpcrt4!RpcBindingInqAuthInfoExW | dlls/rpcrt4/rpc_binding.c:1501 | triage-6.tsv:374[escalate] |
| RPC-SEC | rpcrt4!RpcMgmtSetAuthorizationFn | dlls/rpcrt4/rpc_server.c:1725 | triage-6.tsv:394[escalate] |
| RPC-SEC | rpcrt4!rpcrt4_ncalrpc_inquire_auth_client | dlls/rpcrt4/rpc_transport.c:863 | triage-6.tsv:372[escalate] |
| SEC-ACL | advapi32!BuildSecurityDescriptorA | dlls/advapi32/security.c:329 | triage-6.tsv:91[escalate] |
| SEC-ACL | advapi32!GetEffectiveRightsFromAclA | dlls/advapi32/security.c:270 | triage-6.tsv:90[escalate] |
| SEC-ACL | advapi32!GetEffectiveRightsFromAclW | dlls/advapi32/security.c:279 | triage-6.tsv:20[escalate] |
| SEC-ACL | advapi32!GetNamedSecurityInfoExA | dlls/advapi32/security.c:3451 | triage-6.tsv:103[escalate] |
| SEC-ACL | advapi32!GetNamedSecurityInfoExW | dlls/advapi32/security.c:3439 | triage-6.tsv:102[escalate] |
| SEC-ACL | advapi32!GetSecurityInfo | dlls/advapi32/security.c:1788 | triage-6.tsv:12[escalate] |
| SEC-ACL | advapi32!GetSecurityInfoExA | dlls/advapi32/security.c:1926 | triage-6.tsv:99[escalate] |
| SEC-ACL | advapi32!GetSecurityInfoExW | dlls/advapi32/security.c:1940 | triage-6.tsv:100[escalate] |
| SEC-ACL | advapi32!LookupSecurityDescriptorPartsA | dlls/advapi32/security.c:4433 | triage-6.tsv:105[escalate] |
| SEC-ACL | advapi32!LookupSecurityDescriptorPartsW | dlls/advapi32/security.c:4445 | triage-6.tsv:106[escalate] |
| SEC-ACL | advapi32!SetSecurityInfo | dlls/advapi32/security.c:3694 | triage-6.tsv:5[escalate] |
| SEC-ACL | advapi32!trustee_to_sid | dlls/advapi32/security.c:2353 | triage-6.tsv:101[escalate] |
| SEC-ACL | kernelbase!AccessCheckByType | dlls/kernelbase/security.c:1402 | triage-6.tsv:238[escalate] |
| SEC-ACL | kernelbase!GetWindowsAccountDomainSid | dlls/kernelbase/security.c:369 | triage-6.tsv:234[escalate] |
| SEC-ACL | kernelbase!SetAclInformation | dlls/kernelbase/security.c:1632 | triage-6.tsv:244[escalate] |
| SEC-ACL | kernelbase!SetPrivateObjectSecurity | dlls/kernelbase/security.c:1306 | triage-6.tsv:235[escalate] |
| SEC-ACL | kernelbase!SetPrivateObjectSecurityEx | dlls/kernelbase/security.c:1317 | triage-6.tsv:236[escalate] |
| SEC-ACL | ntdll!RtlAddAccessAllowedObjectAce | dlls/ntdll/sec.c:1371 | triage-6.tsv:256[escalate] |
| SEC-ACL | ntdll!RtlAddAccessDeniedObjectAce | dlls/ntdll/sec.c:1416 | triage-6.tsv:257[escalate] |
| SEC-ACL | ntdll!RtlAddAuditAccessObjectAce | dlls/ntdll/sec.c:1472 | triage-6.tsv:258[escalate] |
| SEC-ACL | ntdll!RtlConvertToAutoInheritSecurityObject | dlls/ntdll/sec.c:1889 | triage-6.tsv:259[escalate] |
| SEC-ACL | ntdll!RtlDefaultNpAcl | dlls/ntdll/sec.c:1905 | triage-6.tsv:260[escalate] |
| SEC-ACL | ntdll!RtlDeleteSecurityObject | dlls/ntdll/sec.c:1212 | triage-6.tsv:255[escalate] |
| SEC-ACL | ntdll!RtlNewSecurityObjectEx | dlls/ntdll/sec.c:1153 | triage-6.tsv:253[escalate] |
| SEC-ACL | ntdll!RtlNewSecurityObjectWithMultipleInheritance | dlls/ntdll/sec.c:1201 | triage-6.tsv:254[escalate] |
| SEC-ACL | sechost!parse_acl | dlls/sechost/security.c:957 | fast-b.tsv:2184[escalate] |
| SEC-AUDIT | advapi32!AccessCheckAndAuditAlarmA | dlls/advapi32/security.c:1716 | triage-6.tsv:94[escalate] |
| SEC-AUDIT | advapi32!AuditQuerySystemPolicy | dlls/advapi32/lsa.c:1187 | triage-6.tsv:550[escalate] |
| SEC-AUDIT | advapi32!GetAuditedPermissionsFromAclA | dlls/advapi32/security.c:2997 | triage-6.tsv:551[escalate] |
| SEC-AUDIT | advapi32!GetAuditedPermissionsFromAclW | dlls/advapi32/security.c:3008 | triage-6.tsv:552[escalate] |
| SEC-AUDIT | advapi32!ObjectCloseAuditAlarmA | dlls/advapi32/security.c:1728 | triage-6.tsv:95[escalate] |
| SEC-AUDIT | advapi32!ObjectOpenAuditAlarmA | dlls/advapi32/security.c:1735 | triage-6.tsv:96[escalate] |
| SEC-AUDIT | advapi32!ObjectPrivilegeAuditAlarmA | dlls/advapi32/security.c:1748 | triage-6.tsv:97[escalate] |
| SEC-AUDIT | advapi32!PrivilegedServiceAuditAlarmA | dlls/advapi32/security.c:1757 | triage-6.tsv:98[escalate] |
| SEC-AUDIT | kernelbase!AccessCheckAndAuditAlarmW | dlls/kernelbase/security.c:1388 | triage-6.tsv:237[escalate] |
| SEC-AUDIT | kernelbase!ObjectCloseAuditAlarmW | dlls/kernelbase/security.c:1580 | triage-6.tsv:239[escalate] |
| SEC-AUDIT | kernelbase!ObjectDeleteAuditAlarmW | dlls/kernelbase/security.c:1589 | triage-6.tsv:240[escalate] |
| SEC-AUDIT | kernelbase!ObjectOpenAuditAlarmW | dlls/kernelbase/security.c:1598 | triage-6.tsv:241[escalate] |
| SEC-AUDIT | kernelbase!ObjectPrivilegeAuditAlarmW | dlls/kernelbase/security.c:1612 | triage-6.tsv:242[escalate] |
| SEC-AUDIT | kernelbase!PrivilegedServiceAuditAlarmW | dlls/kernelbase/security.c:1622 | triage-6.tsv:243[escalate] |
| SEC-AUDIT | ntdll!NtAccessCheckAndAuditAlarm | dlls/ntdll/unix/security.c:1063 | triage-6.tsv:316[escalate] |
| SEC-CRED | advapi32!CredMarshalCredentialW | dlls/advapi32/cred.c:2198 | triage-6.tsv:62[escalate] |
| SEC-CRED | advapi32!CredReadDomainCredentialsW | dlls/advapi32/cred.c:1946 | triage-6.tsv:61[escalate] |
| SEC-CRED | advapi32!CredUnmarshalCredentialW | dlls/advapi32/cred.c:2328 | triage-6.tsv:63[escalate] |
| SEC-CRED | cryptowinrt!credentials_statics_DeleteAsync | dlls/cryptowinrt/credentials.c:154 | triage-6.tsv:702[escalate] |
| SEC-CRED | cryptowinrt!credentials_statics_OpenAsync | dlls/cryptowinrt/credentials.c:147 | triage-6.tsv:701[escalate] |
| SEC-CRED | cryptowinrt!credentials_statics_RenewAttestationAsync | dlls/cryptowinrt/credentials.c:134 | triage-6.tsv:699[escalate] |
| SEC-CRED | cryptowinrt!credentials_statics_RequestCreateAsync | dlls/cryptowinrt/credentials.c:140 | triage-6.tsv:700[escalate] |
| SEC-CRED | windows.security.authentication.onlineid!authenticator_get_User | dlls/windows.security.authentication.onlineid/authenticator.c:211 | triage-6.tsv:495[escalate] |
| SEC-CRED | windows.security.authentication.onlineid!authenticator_statics_GetForUser | dlls/windows.security.authentication.onlineid/authenticator.c:253 | triage-6.tsv:496[escalate] |
| SEC-CRED | windows.security.authentication.web.core!account2_GetPictureAsync | dlls/windows.security.authentication.web.core/objects.c:1136 | triage-6.tsv:507[escalate] |
| SEC-CRED | windows.security.authentication.web.core!account2_SignOutAsync | dlls/windows.security.authentication.web.core/objects.c:1143 | triage-6.tsv:508[escalate] |
| SEC-CRED | windows.security.authentication.web.core!account2_SignOutWithClientIdAsync | dlls/windows.security.authentication.web.core/objects.c:1150 | triage-6.tsv:509[escalate] |
| SEC-CRED | windows.security.authentication.web.core!token_result_InvalidateCacheAsync | dlls/windows.security.authentication.web.core/objects.c:1612 | triage-6.tsv:510[escalate] |
| SEC-CRYPT32 | crypt32!CDecodeMsg_Update | dlls/crypt32/msg.c:2489 | triage-6.tsv:128[escalate] |
| SEC-CRYPT32 | crypt32!CEnvelopedEncodeMsg_Update | dlls/crypt32/msg.c:1873 | triage-6.tsv:127[escalate] |
| SEC-CRYPT32 | crypt32!CRYPT_CheckUsages | dlls/crypt32/chain.c:2744 | triage-6.tsv:121[escalate] |
| SEC-CRYPT32 | crypt32!CSignedEncodeMsg_Update | dlls/crypt32/msg.c:1312 | triage-6.tsv:126[escalate] |
| SEC-CRYPT32 | crypt32!CertRegisterPhysicalStore | dlls/crypt32/store.c:1671 | triage-6.tsv:140[escalate] |
| SEC-CRYPT32 | crypt32!CertUnregisterPhysicalStore | dlls/crypt32/store.c:1683 | triage-6.tsv:141[escalate] |
| SEC-CRYPT32 | crypt32!CertVerifyCTLUsage | dlls/crypt32/cert.c:4113 | triage-6.tsv:123[escalate] |
| SEC-CRYPT32 | crypt32!CertVerifyCertificateChainPolicy | dlls/crypt32/chain.c:4060 | triage-6.tsv:27[escalate] |
| SEC-CRYPT32 | crypt32!CryptSetKeyIdentifierProperty | dlls/crypt32/cert.c:4157 | triage-6.tsv:124[escalate] |
| SEC-CRYPT32 | cryptext!CryptExtAddPFX | dlls/cryptext/cryptext_main.c:32 | triage-7.tsv:432[escalate] |
| SEC-CRYPT32 | cryptext!CryptExtAddPFXW | dlls/cryptext/cryptext_main.c:41 | triage-7.tsv:433[escalate] |
| SEC-CRYPT32 | cryptnet!CRYPT_CreatePKCS7 | dlls/cryptnet/cryptnet_main.c:1282 | triage-7.tsv:139[escalate] |
| SEC-CRYPT32* | cryptui!CryptUIDlgViewContext | dlls/cryptui/main.c:4426 | fast-b.tsv:1900[escalate]; triage-7.tsv:426[windows-matches] |
| SEC-CRYPTOPOL | bcrypt!BCryptGetFipsAlgorithmMode | dlls/bcrypt/bcrypt_main.c:369 | triage-6.tsv:109[escalate] |
| SEC-CRYPTOPOL | ncrypt!NCryptExportKey | dlls/ncrypt/main.c:1251 | fast-b.tsv:1897[escalate]; triage-7.tsv:165[escalate] |
| SEC-CRYPTOPOL | secur32!schan_QueryCredentialsAttributes | dlls/secur32/schannel.c:273 | triage-6.tsv:407[escalate] |
| SEC-CRYPTOPOL | secur32!schan_QueryCredentialsAttributesA | dlls/secur32/schannel.c:345 | triage-6.tsv:408[escalate] |
| SEC-CRYPTOPOL | secur32!schan_QueryCredentialsAttributesW | dlls/secur32/schannel.c:365 | triage-6.tsv:409[escalate] |
| SEC-EDP* | windows.security.enterprisedata!factory_ActivateInstance | dlls/windows.security.enterprisedata/main.c:386 | fast-b.tsv:1875[windows-matches]; triage-7.tsv:322[escalate] |
| SEC-EFS | advapi32!CloseEncryptedFileRaw | dlls/advapi32/crypt.c:338 | triage-6.tsv:64[escalate] |
| SEC-EFS | advapi32!OpenEncryptedFileRawA | dlls/advapi32/crypt.c:2255 | triage-6.tsv:65[escalate] |
| SEC-EFS | advapi32!OpenEncryptedFileRawW | dlls/advapi32/crypt.c:2274 | triage-6.tsv:66[escalate] |
| SEC-EFS | advapi32!ReadEncryptedFileRaw | dlls/advapi32/crypt.c:2293 | triage-6.tsv:67[escalate] |
| SEC-EFS | advapi32!WriteEncryptedFileRaw | dlls/advapi32/crypt.c:2660 | triage-6.tsv:69[escalate] |
| SEC-LOGON | kerberos!kerberos_LsaApCallPackageUntrusted | dlls/kerberos/krb5_ap.c:340 | triage-7.tsv:393[escalate] |
| SEC-LOGON | klist!get | programs/klist/main.c:261 | triage-7.tsv:264[escalate] |
| SEC-LOGON | klist!purge | programs/klist/main.c:254 | triage-7.tsv:263[escalate] |
| SEC-LOGON | klist!tgt | programs/klist/main.c:247 | triage-7.tsv:262[escalate] |
| SEC-LOGON | secur32!AddSecurityPackageA | dlls/secur32/secur32.c:1063 | triage-6.tsv:627[escalate] |
| SEC-LOGON | secur32!AddSecurityPackageW | dlls/secur32/secur32.c:1069 | triage-6.tsv:628[escalate] |
| SEC-LOGON | secur32!DeleteSecurityPackageA | dlls/secur32/secur32.c:1075 | triage-6.tsv:629[escalate] |
| SEC-LOGON | secur32!DeleteSecurityPackageW | dlls/secur32/secur32.c:1081 | triage-6.tsv:630[escalate] |
| SEC-LOGON | secur32!LsaEnumerateLogonSessions | dlls/secur32/lsa.c:166 | triage-6.tsv:23[escalate] |
| SEC-LOGON | secur32!LsaGetLogonSessionData | dlls/secur32/lsa.c:183 | triage-6.tsv:35[escalate] |
| SEC-LOGON | secur32!LsaLogonUser | dlls/secur32/lsa.c:212 | triage-6.tsv:11[escalate] |
| SEC-LOGON | secur32!LsaRegisterLogonProcess | dlls/secur32/lsa.c:140 | triage-6.tsv:410[escalate] |
| SEC-LOGON | secur32!lsa_AddCredential | dlls/secur32/lsa.c:239 | triage-6.tsv:413[escalate] |
| SEC-LOGON | secur32!lsa_CreateLogonSession | dlls/secur32/lsa.c:227 | triage-6.tsv:411[escalate] |
| SEC-LOGON | secur32!lsa_DeleteCredential | dlls/secur32/lsa.c:255 | triage-6.tsv:415[escalate] |
| SEC-LOGON | secur32!lsa_DeleteLogonSession | dlls/secur32/lsa.c:233 | triage-6.tsv:412[escalate] |
| SEC-LOGON | secur32!lsa_GetCredentials | dlls/secur32/lsa.c:247 | triage-6.tsv:414[escalate] |
| SEC-LOGON | secur32!lsa_RegisterCallback | dlls/secur32/lsa.c:316 | triage-6.tsv:416[escalate] |
| SEC-LOGON | secur32!nego_LsaApCallPackageUntrusted | dlls/secur32/negotiate.c:71 | triage-6.tsv:417[escalate] |
| SEC-LSA | advapi32!LsaAddAccountRights | dlls/advapi32/lsa.c:196 | triage-6.tsv:19[escalate] |
| SEC-LSA | advapi32!LsaCreateTrustedDomainEx | dlls/advapi32/lsa.c:228 | triage-6.tsv:40[escalate] |
| SEC-LSA | advapi32!LsaDeleteTrustedDomain | dlls/advapi32/lsa.c:244 | triage-6.tsv:41[escalate] |
| SEC-LSA | advapi32!LsaEnumerateAccountRights | dlls/advapi32/lsa.c:254 | triage-6.tsv:42[escalate] |
| SEC-LSA | advapi32!LsaEnumerateAccounts | dlls/advapi32/lsa.c:270 | triage-6.tsv:43[escalate] |
| SEC-LSA | advapi32!LsaEnumerateAccountsWithUserRight | dlls/advapi32/lsa.c:286 | triage-6.tsv:44[escalate] |
| SEC-LSA | advapi32!LsaEnumerateTrustedDomains | dlls/advapi32/lsa.c:318 | triage-6.tsv:45[escalate] |
| SEC-LSA | advapi32!LsaEnumerateTrustedDomainsEx | dlls/advapi32/lsa.c:336 | triage-6.tsv:46[escalate] |
| SEC-LSA | advapi32!LsaLookupNames | dlls/advapi32/lsa.c:386 | triage-6.tsv:47[escalate] |
| SEC-LSA | advapi32!LsaOpenTrustedDomainByName | dlls/advapi32/lsa.c:780 | triage-6.tsv:48[escalate] |
| SEC-LSA | advapi32!LsaQueryInformationPolicy | dlls/advapi32/lsa.c:804 | triage-6.tsv:49[escalate] |
| SEC-LSA | advapi32!LsaQueryTrustedDomainInfo | dlls/advapi32/lsa.c:953 | triage-6.tsv:50[escalate] |
| SEC-LSA | advapi32!LsaQueryTrustedDomainInfoByName | dlls/advapi32/lsa.c:967 | triage-6.tsv:51[escalate] |
| SEC-LSA | advapi32!LsaRegisterPolicyChangeNotification | dlls/advapi32/lsa.c:981 | triage-6.tsv:52[escalate] |
| SEC-LSA | advapi32!LsaRemoveAccountRights | dlls/advapi32/lsa.c:993 | triage-6.tsv:53[escalate] |
| SEC-LSA | advapi32!LsaRetrievePrivateData | dlls/advapi32/lsa.c:1018 | triage-6.tsv:54[escalate] |
| SEC-LSA | advapi32!LsaSetInformationPolicy | dlls/advapi32/lsa.c:1041 | triage-6.tsv:55[escalate] |
| SEC-LSA | advapi32!LsaSetSecret | dlls/advapi32/lsa.c:1065 | triage-6.tsv:56[escalate] |
| SEC-LSA | advapi32!LsaSetTrustedDomainInfoByName | dlls/advapi32/lsa.c:1079 | triage-6.tsv:57[escalate] |
| SEC-LSA | advapi32!LsaSetTrustedDomainInformation | dlls/advapi32/lsa.c:1093 | triage-6.tsv:58[escalate] |
| SEC-LSA | advapi32!LsaStorePrivateData | dlls/advapi32/lsa.c:1117 | triage-6.tsv:59[escalate] |
| SEC-LSA | advapi32!LsaUnregisterPolicyChangeNotification | dlls/advapi32/lsa.c:1130 | triage-6.tsv:60[escalate] |
| SEC-PSTORE | pstorec!PStore_fnCloseItem | dlls/pstorec/pstorec.c:293 | triage-6.tsv:736[escalate] |
| SEC-PSTORE | pstorec!PStore_fnCreateSubtype | dlls/pstorec/pstorec.c:167 | triage-6.tsv:726[escalate] |
| SEC-PSTORE | pstorec!PStore_fnCreateType | dlls/pstorec/pstorec.c:135 | triage-6.tsv:723[escalate] |
| SEC-PSTORE | pstorec!PStore_fnDeleteItem | dlls/pstorec/pstorec.c:243 | triage-6.tsv:733[escalate] |
| SEC-PSTORE | pstorec!PStore_fnDeleteSubtype | dlls/pstorec/pstorec.c:190 | triage-6.tsv:728[escalate] |
| SEC-PSTORE | pstorec!PStore_fnDeleteType | dlls/pstorec/pstorec.c:157 | triage-6.tsv:725[escalate] |
| SEC-PSTORE | pstorec!PStore_fnEnumItems | dlls/pstorec/pstorec.c:304 | triage-6.tsv:737[escalate] |
| SEC-PSTORE | pstorec!PStore_fnEnumSubtypes | dlls/pstorec/pstorec.c:233 | triage-6.tsv:732[escalate] |
| SEC-PSTORE | pstorec!PStore_fnEnumTypes | dlls/pstorec/pstorec.c:223 | triage-6.tsv:731[escalate] |
| SEC-PSTORE | pstorec!PStore_fnGetInfo | dlls/pstorec/pstorec.c:106 | triage-6.tsv:720[escalate] |
| SEC-PSTORE | pstorec!PStore_fnGetProvParam | dlls/pstorec/pstorec.c:115 | triage-6.tsv:721[escalate] |
| SEC-PSTORE | pstorec!PStore_fnGetSubtypeInfo | dlls/pstorec/pstorec.c:179 | triage-6.tsv:727[escalate] |
| SEC-PSTORE | pstorec!PStore_fnGetTypeInfo | dlls/pstorec/pstorec.c:147 | triage-6.tsv:724[escalate] |
| SEC-PSTORE | pstorec!PStore_fnOpenItem | dlls/pstorec/pstorec.c:281 | triage-6.tsv:706[escalate] |
| SEC-PSTORE | pstorec!PStore_fnReadAccessRuleset | dlls/pstorec/pstorec.c:201 | triage-6.tsv:729[escalate] |
| SEC-PSTORE | pstorec!PStore_fnReadItem | dlls/pstorec/pstorec.c:254 | triage-6.tsv:734[escalate] |
| SEC-PSTORE | pstorec!PStore_fnSetProvParam | dlls/pstorec/pstorec.c:125 | triage-6.tsv:722[escalate] |
| SEC-PSTORE | pstorec!PStore_fnWriteAccessRuleset | dlls/pstorec/pstorec.c:212 | triage-6.tsv:730[escalate] |
| SEC-PSTORE | pstorec!PStore_fnWriteItem | dlls/pstorec/pstorec.c:267 | triage-6.tsv:735[escalate] |
| SEC-SCARD | winscard!SCardAccessStartedEvent | dlls/winscard/winscard.c:112 | triage-7.tsv:193[escalate] |
| SEC-SCARD | winscard!SCardListCardsA | dlls/winscard/winscard.c:188 | triage-7.tsv:195[escalate] |
| SEC-SCARD | winscard!SCardReleaseStartedEvent | dlls/winscard/winscard.c:418 | triage-7.tsv:194[escalate] |
| SEC-TOKEN | kernelbase!RegOpenUserClassesRoot | dlls/kernelbase/registry.c:3607 | triage-6.tsv:245[escalate] |
| SEC-TOKEN | ntdll!NtAdjustGroupsToken | dlls/ntdll/unix/security.c:854 | triage-6.tsv:587[escalate] |
| SEC-TOKEN | ntdll!NtCompareTokens | dlls/ntdll/unix/server.c:1943 | triage-6.tsv:324[escalate] |
| SEC-TOKEN | ntdll!NtImpersonateAnonymousToken | dlls/ntdll/unix/security.c:999 | triage-6.tsv:315[escalate] |
| SEC-TOKEN | ntdll!NtQueryInformationToken | dlls/ntdll/unix/security.c:268 | triage-6.tsv:313[escalate] |
| SEC-TOKEN | ntdll!NtSetInformationToken | dlls/ntdll/unix/security.c:717 | triage-6.tsv:314[escalate] |
| SEC-TOKEN | user32!DdeSetQualityOfService | dlls/user32/dde_misc.c:231 | triage-6.tsv:444[escalate] |
| SEC-TOKEN | user32!ImpersonateDdeClientWindow | dlls/user32/dde_misc.c:221 | triage-6.tsv:443[escalate] |
| SEC-TOKEN | user32!RegisterLogonProcess | dlls/user32/misc.c:220 | triage-6.tsv:425[escalate] |
| SEC-TOKEN | user32!RegisterServicesProcess | dlls/user32/user_main.c:399 | triage-6.tsv:445[escalate] |
| SEC-TOKEN | user32!SetLogonNotifyWindow | dlls/user32/misc.c:229 | triage-6.tsv:426[escalate] |
| SEC-TOKEN | user32!SetWindowStationUser | dlls/user32/misc.c:211 | triage-6.tsv:424[escalate] |
| SEC-TOKEN | user32!UserHandleGrantAccess | dlls/user32/misc.c:475 | triage-6.tsv:435[escalate] |
| SEC-TOKEN | win32u!NtUserOpenInputDesktop | dlls/win32u/winstation.c:576 | triage-6.tsv:447[escalate] |
| SEC-TOKEN | wow64!wow64_NtAdjustGroupsToken | dlls/wow64/security.c:104 | triage-6.tsv:743[escalate] |
| SEC-TOKEN | wow64!wow64_NtCreateLowBoxToken | dlls/wow64/security.c:137 | triage-6.tsv:717[escalate] |
| SEC-TOKEN | wow64!wow64_NtSetInformationToken | dlls/wow64/security.c:516 | triage-6.tsv:744[escalate] |
| SEC-WINTRUST | advapi32!CRYPT_VerifyImage | dlls/advapi32/crypt.c:197 | triage-6.tsv:641[escalate] |
| SEC-WINTRUST | advapi32!SaferSetLevelInformation | dlls/advapi32/security.c:4423 | triage-6.tsv:104[escalate] |
| SEC-WINTRUST | advapi32!SystemFunction035 | dlls/advapi32/crypt.c:2379 | triage-6.tsv:68[escalate] |
| SEC-WINTRUST | cryptdlg!CertTrustFinalPolicy | dlls/cryptdlg/main.c:296 | triage-7.tsv:430[escalate] |
| SEC-WINTRUST | cryptui!CryptUIWizDigitalSign | dlls/cryptui/main.c:7515 | fast-b.tsv:1901[escalate]; triage-7.tsv:429[escalate] |
| SEC-WINTRUST | kernelbase!SetCachedSigningLevel | dlls/kernelbase/security.c:1641 | triage-6.tsv:31[escalate] |
| SEC-WINTRUST* | mssign32!PvkFreeCryptProv | dlls/mssign32/mssign32_main.c:55 | fast-b.tsv:1922[no-ground-truth]; triage-7.tsv:425[escalate] |
| SEC-WINTRUST* | mssign32!PvkGetCryptProv | dlls/mssign32/mssign32_main.c:33 | fast-b.tsv:1920[no-ground-truth]; triage-7.tsv:423[escalate] |
| SEC-WINTRUST* | mssign32!PvkPrivateKeyAcquireContextFromMemory | dlls/mssign32/mssign32_main.c:44 | fast-b.tsv:1921[no-ground-truth]; triage-7.tsv:424[escalate] |
| SEC-WINTRUST | wintrust!CryptCATAdminAcquireContext2 | dlls/wintrust/crypt.c:157 | triage-6.tsv:37[escalate] |
| SEC-WINTRUST | wintrust!CryptSIPCreateIndirectData | dlls/wintrust/crypt.c:1973 | triage-6.tsv:514[escalate] |
| SEC-WINTRUST | wintrust!CryptSIPGetSignedDataMsg | dlls/wintrust/crypt.c:2854 | triage-6.tsv:515[escalate] |
| SEC-WINTRUST | wintrust!CryptSIPPutSignedDataMsg | dlls/wintrust/crypt.c:2897 | triage-6.tsv:516[escalate] |
| SEC-WINTRUST | wintrust!CryptSIPRemoveSignedDataMsg | dlls/wintrust/crypt.c:2921 | triage-6.tsv:517[escalate] |
| SEC-WINTRUST | wintrust!CryptSIPVerifyIndirectData | dlls/wintrust/crypt.c:2932 | triage-6.tsv:518[escalate] |
| SEC-WINTRUST | wintrust!DllUnregisterServer | dlls/wintrust/register.c:1161 | triage-6.tsv:522[escalate] |
| SEC-WINTRUST | wintrust!DriverCleanupPolicy | dlls/wintrust/softpub.c:66 | triage-6.tsv:529[escalate] |
| SEC-WINTRUST | wintrust!DriverFinalPolicy | dlls/wintrust/softpub.c:72 | triage-6.tsv:530[escalate] |
| SEC-WINTRUST | wintrust!DriverInitializePolicy | dlls/wintrust/softpub.c:60 | triage-6.tsv:528[escalate] |
| SEC-WINTRUST | wintrust!GenericChainFinalProv | dlls/wintrust/softpub.c:1697 | triage-6.tsv:535[escalate] |
| SEC-WINTRUST | wintrust!HTTPSCertificateTrust | dlls/wintrust/softpub.c:1812 | triage-6.tsv:693[escalate] |
| SEC-WINTRUST | wintrust!HTTPSFinalProv | dlls/wintrust/softpub.c:1818 | triage-6.tsv:694[escalate] |
| SEC-WINTRUST | wintrust!SoftpubAuthenticode | dlls/wintrust/softpub.c:1572 | triage-6.tsv:534[escalate] |
| SEC-WINTRUST | wintrust!SoftpubCheckCert | dlls/wintrust/softpub.c:1285 | triage-6.tsv:532[escalate] |
| SEC-WINTRUST | wintrust!SoftpubDllUnregisterServer | dlls/wintrust/register.c:1220 | triage-6.tsv:523[escalate] |
| SEC-WINTRUST | wintrust!SoftpubLoadMessage | dlls/wintrust/softpub.c:671 | triage-6.tsv:531[escalate] |
| SEC-WINTRUST | wintrust!WINTRUST_CertActionVerify | dlls/wintrust/wintrust_main.c:507 | triage-6.tsv:520[escalate] |
| SEC-WINTRUST | wintrust!WINTRUST_CertVerifyObjTrust | dlls/wintrust/wintrust_main.c:365 | triage-6.tsv:519[escalate] |
| SEC-WINTRUST | wintrust!WINTRUST_CreateChainForSigner | dlls/wintrust/softpub.c:1430 | triage-6.tsv:533[escalate] |
| SEC-WINTRUST | wintrust!WinVerifyTrust | dlls/wintrust/wintrust_main.c:668 | triage-6.tsv:14[escalate] |
| SEC-XMLSIG | cryptxml!CryptXmlGetDocContext | dlls/cryptxml/main.c:62 | triage-7.tsv:413[escalate] |
| SEC-XMLSIG | cryptxml!CryptXmlGetReference | dlls/cryptxml/main.c:76 | triage-7.tsv:415[escalate] |
| SEC-XMLSIG | cryptxml!CryptXmlGetSignature | dlls/cryptxml/main.c:69 | triage-7.tsv:414[escalate] |
| SEC-XMLSIG | cryptxml!CryptXmlGetStatus | dlls/cryptxml/main.c:50 | triage-7.tsv:411[escalate] |
| SEC-XMLSIG | cryptxml!CryptXmlOpenToDecode | dlls/cryptxml/main.c:35 | triage-7.tsv:410[escalate] |
| SEC-XMLSIG | cryptxml!CryptXmlVerifySignature | dlls/cryptxml/main.c:56 | triage-7.tsv:412[escalate] |
| SH-ADESK | shell32!active_desktop_AddDesktopItemWithUI | dlls/shell32/shfldr_desktop.c:1055 | fast-b.tsv:1797[escalate] |
| SH-ADESK | shell32!active_desktop_AddUrl | dlls/shell32/shfldr_desktop.c:1104 | fast-b.tsv:1803[escalate] |
| SH-DEFAPP | shell32!ApplicationAssociationRegistration_SetAppAsDefault | dlls/shell32/assoc.c:1123 | fast-b.tsv:2213[escalate] |
| SH-DIALOGS | gamingtcui!ShowProfileCardUI | dlls/gamingtcui/gamingtcui_main.c:52 | triage-7.tsv:373[escalate] |
| SH-DIALOGS | rasdlg!RasEntryDlgW | dlls/rasdlg/rasdlg_main.c:30 | triage-7.tsv:275[escalate] |
| SH-DIALOGS | shell32!InitNetworkAddressControl | dlls/shell32/shell32_main.c:1446 | fast-b.tsv:1224[escalate] |
| SH-DIALOGS | shell32!PickIconDlg | dlls/shell32/dialogs.c:80 | fast-b.tsv:1209[escalate] |
| SH-DIALOGS | shell32!SHFormatDrive | dlls/shell32/shellord.c:2222 | fast-b.tsv:1226[escalate] |
| SH-DIALOGS | shell32!SHMultiFileProperties | dlls/shell32/shlfileop.c:2272 | fast-b.tsv:1213[escalate] |
| SH-DIALOGS | wiaservc!wiadevmgr_AddDeviceDlg | dlls/wiaservc/wiadevmgr.c:265 | fast-b.tsv:203[escalate] |
| SH-DPI | win32u!NtUserGetSystemDpiForProcess | dlls/win32u/sysparams.c:4476 | triage-6c.tsv:3[escalate] |
| SH-INPUT | user32!CreateSyntheticPointerDevice | dlls/user32/input.c:955 | triage-6.tsv:25[escalate] |
| SH-INPUT | user32!DisplayConfigSetDeviceInfo | dlls/user32/sysparams.c:1179 | triage-6.tsv:442[escalate] |
| SH-INPUT | user32!LoadKeyboardLayoutEx | dlls/user32/input.c:473 | triage-6.tsv:440[escalate] |
| SH-INPUT | user32!LoadKeyboardLayoutW | dlls/user32/input.c:417 | triage-6.tsv:36[escalate] |
| SH-INPUT | user32!RegisterTouchHitTestingWindow | dlls/user32/misc.c:935 | triage-6c.tsv:7[escalate] |
| SH-INPUT | user32!UnloadKeyboardLayout | dlls/user32/input.c:484 | triage-6.tsv:24[escalate] |
| SH-INPUT* | user32!WINNLSEnableIME | dlls/user32/misc.c:419 | triage-6.tsv:431[no-ground-truth]; triage-6c.tsv:4[escalate] |
| SH-INPUT* | user32!WINNLSGetEnableStatus | dlls/user32/misc.c:429 | triage-6.tsv:432[no-ground-truth]; triage-6c.tsv:6[escalate] |
| SH-INPUT | win32u!NtUserEnableMouseInPointer | dlls/win32u/input.c:2570 | triage-6.tsv:476[escalate] |
| SH-MISC | query!LoadIFilter | dlls/query/query_main.c:75 | triage-7.tsv:278[escalate] |
| SH-MISC | shell32!DragSourceHelper2_SetFlags | dlls/shell32/dragdrophelper.c:208 | fast-b.tsv:2195[escalate] |
| SH-MISC | shell32!ExtractAssociatedIconExW | dlls/shell32/iconcache.c:961 | fast-b.tsv:2297[escalate] |
| SH-MISC | shell32!FolderView2_SetGroupSubsetCount | dlls/shell32/shlview.c:4836 | fast-b.tsv:2298[escalate] |
| SH-MISC | shell32!ISF_Desktop_fnSetNameOf | dlls/shell32/shfldr_desktop.c:732 | fast-b.tsv:1198[escalate] |
| SH-MISC | shell32!IShellFolder_Printers_fnSetNameOf | dlls/shell32/shfldr_printers.c:484 | fast-b.tsv:1192[escalate] |
| SH-MISC | shell32!SHCreateLinks | dlls/shell32/shlfolder.c:776 | fast-b.tsv:1243[escalate] |
| SH-PROPSTORE | shell32!IShellItemArray_fnBindToHandler | dlls/shell32/shellitem.c:1434 | fast-b.tsv:1230[escalate] |
| SH-PROPSTORE | shell32!IShellItemArray_fnGetPropertyDescriptionList | dlls/shell32/shellitem.c:1458 | fast-b.tsv:1229[escalate] |
| SH-PROPSTORE | shell32!IShellItemArray_fnGetPropertyStore | dlls/shell32/shellitem.c:1447 | fast-b.tsv:1228[escalate] |
| SH-PROPSTORE | shell32!ShellItem2_GetPropertyDescriptionList | dlls/shell32/shellitem.c:597 | fast-b.tsv:1227[escalate] |
| SH-PROPSTORE | shell32!file_operation_ApplyPropertiesToItem | dlls/shell32/shlfileop.c:3132 | fast-b.tsv:1215[escalate] |
| SH-PROPSTORE | shell32!file_operation_ApplyPropertiesToItems | dlls/shell32/shlfileop.c:3139 | fast-b.tsv:1216[escalate] |
| SH-PROPSTORE | shell32!file_operation_SetProperties | dlls/shell32/shlfileop.c:3116 | fast-b.tsv:1214[escalate] |
| SYS-AWE | kernelbase!AllocateUserPhysicalPages | dlls/kernelbase/memory.c:1455 | triage-6.tsv:212[escalate] |
| SYS-AWE | kernelbase!FreeUserPhysicalPages | dlls/kernelbase/memory.c:1467 | triage-6.tsv:213[escalate] |
| SYS-AWE | kernelbase!MapUserPhysicalPages | dlls/kernelbase/memory.c:1557 | triage-6.tsv:214[escalate] |
| SYS-INPUT | user32!BlockInput | dlls/user32/input.c:405 | triage-6.tsv:439[escalate] |
| SYS-INPUT | user32!SetSystemCursor | dlls/user32/cursoricon.c:1609 | triage-6.tsv:418[escalate] |
| SYS-PRIV | advapi32!NotifyBootConfigStatus | dlls/advapi32/security.c:1259 | triage-6.tsv:918[escalate] |
| SYS-PRIV | advapi32!RegReplaceKeyW | dlls/advapi32/registry.c:565 | triage-6.tsv:107[escalate] |
| SYS-PRIV | kernel32!SetFirmwareEnvironmentVariableA | dlls/kernel32/process.c:786 | triage-6.tsv:156[escalate] |
| SYS-PRIV | kernel32!SetFirmwareEnvironmentVariableW | dlls/kernel32/process.c:796 | triage-6.tsv:157[escalate] |
| SYS-PRIV | ntdll!NtCreatePagingFile | dlls/ntdll/unix/virtual.c:6496 | triage-6.tsv:323[escalate] |
| SYS-PRIV | ntdll!NtInitiatePowerAction | dlls/ntdll/unix/system.c:5330 | triage-6.tsv:300[escalate] |
| SYS-PRIV | ntdll!NtLoadDriver | dlls/ntdll/unix/system.c:5288 | triage-6.tsv:297[escalate] |
| SYS-PRIV | ntdll!NtQuerySystemEnvironmentValue | dlls/ntdll/unix/system.c:4270 | triage-6.tsv:294[escalate] |
| SYS-PRIV | ntdll!NtReplaceKey | dlls/ntdll/unix/registry.c:1445 | triage-6.tsv:321[escalate] |
| SYS-PRIV | ntdll!NtSetDebugFilterState | dlls/ntdll/unix/debug.c:389 | triage-6.tsv:335[escalate] |
| SYS-PRIV | ntdll!NtSetSystemInformation | dlls/ntdll/unix/system.c:4250 | triage-6.tsv:293[escalate] |
| SYS-PRIV | ntdll!NtShutdownSystem | dlls/ntdll/unix/system.c:4406 | triage-6.tsv:686[escalate] |
| SYS-PRIV | ntdll!NtUnloadDriver | dlls/ntdll/unix/system.c:5298 | triage-6.tsv:298[escalate] |
| SYS-PRIV | ntdll!enum_firmware_info | dlls/ntdll/unix/system.c:2191 | triage-6.tsv:583[escalate] |
| SYS-PRIV | ntdll!get_firmware_info | dlls/ntdll/unix/system.c:2211 | triage-6.tsv:584[escalate] |
| SYS-PRIV | services!svcctl_NotifyBootConfigStatus | programs/services/rpc.c:2372 | fast-a.tsv:689[escalate] |
| SYS-SETINFO | ntdll!CDROM_GetDeviceNumber | dlls/ntdll/unix/cdrom.c:725 | triage-6.tsv:871[escalate] |
| SYS-SETINFO | ntdll!CDROM_GetMediaType | dlls/ntdll/unix/cdrom.c:761 | triage-6.tsv:872[escalate] |
| SYS-SETINFO | ntdll!NtFsControlFile | dlls/ntdll/unix/file.c:7401 | triage-6.tsv:868[escalate] |
| SYS-SETINFO | ntdll!NtSetEaFile | dlls/ntdll/unix/file.c:8471 | triage-6.tsv:279[escalate] |
| SYS-SETINFO | ntdll!NtSetInformationJobObject | dlls/ntdll/unix/sync.c:925 | triage-6.tsv:869[escalate] |
| SYS-SETINFO | ntdll!NtSetInformationKey | dlls/ntdll/unix/registry.c:439 | triage-6.tsv:873[escalate] |
| SYS-SETINFO | ntdll!NtSetInformationObject | dlls/ntdll/unix/file.c:8704 | triage-6.tsv:577[escalate] |
| SYS-SETINFO | ntdll!NtSetInformationThread | dlls/ntdll/unix/thread.c:2522 | triage-6.tsv:4[escalate] |
| SYS-SETINFO | ntdll!NtSetVolumeInformationFile | dlls/ntdll/unix/file.c:8436 | triage-6.tsv:870[escalate] |
| SYS-SETINFO | wow64!wow64_NtSetInformationProcess | dlls/wow64/process.c:942 | triage-6.tsv:708[escalate] |
| UIA | uiautomationcore!UiaEventRemoveWindow | dlls/uiautomationcore/uia_client.c:4743 | fast-b.tsv:1561[escalate] |
| UIA | uiautomationcore!UiaGetPropertyValue | dlls/uiautomationcore/uia_client.c:3505 | fast-b.tsv:2333[escalate] |
| UIA | uiautomationcore!UiaGetUpdatedCache | dlls/uiautomationcore/uia_client.c:4259 | fast-b.tsv:2334[escalate] |
| UIA | uiautomationcore!bridge_GetIDsOfNames | dlls/uiautomationcore/uia_bridge.c:221 | fast-b.tsv:1564[escalate] |
| UIA | uiautomationcore!bridge_GetTypeInfo | dlls/uiautomationcore/uia_bridge.c:215 | fast-b.tsv:1563[escalate] |
| UIA | uiautomationcore!bridge_Invoke | dlls/uiautomationcore/uia_bridge.c:227 | fast-b.tsv:1565[escalate] |
| UIA | uiautomationcore!call_pattern_method | dlls/uiautomationcore/uia_patterns.c:276 | fast-b.tsv:2331[escalate] |
| UIA | uiautomationcore!clone_uia_node | dlls/uiautomationcore/uia_client.c:765 | fast-b.tsv:1557[escalate] |
| UIA | uiautomationcore!create_uia_pattern | dlls/uiautomationcore/uia_com_client.c:3833 | fast-b.tsv:2332[escalate] |
| UIA | uiautomationcore!create_uia_property_condition_iface | dlls/uiautomationcore/uia_com_client.c:483 | fast-b.tsv:2325[escalate] |
| UIA | uiautomationcore!msaa_provider_GetPatternProvider | dlls/uiautomationcore/uia_provider.c:687 | fast-b.tsv:2324[escalate] |
| UIA | uiautomationcore!node_fragment_SetFocus | dlls/uiautomationcore/uia_patterns.c:226 | fast-b.tsv:2330[escalate] |
| UIA | uiautomationcore!uia_element_GetCachedPropertyValueEx | dlls/uiautomationcore/uia_com_client.c:2458 | fast-b.tsv:2327[escalate] |
| UIA | uiautomationcore!uia_element_GetCurrentPropertyValueEx | dlls/uiautomationcore/uia_com_client.c:2409 | fast-b.tsv:2326[escalate] |
| UIA | uiautomationcore!uia_iface_AddActiveTextPositionChangedEventHandler | dlls/uiautomationcore/uia_com_client.c:7524 | fast-b.tsv:2328[escalate] |
| UIA | uiautomationcore!uia_iface_CreateProxyFactoryEntry | dlls/uiautomationcore/uia_com_client.c:6786 | fast-b.tsv:1549[escalate] |
| UIA | uiautomationcore!uia_iface_RemoveActiveTextPositionChangedEventHandler | dlls/uiautomationcore/uia_com_client.c:7532 | fast-b.tsv:2329[escalate] |
| UIA | uiautomationcore!uia_iface_get_ProxyFactoryMapping | dlls/uiautomationcore/uia_com_client.c:6793 | fast-b.tsv:1550[escalate] |
| UIA | uiautomationcore!uia_nested_node_provider_get_prop_val | dlls/uiautomationcore/uia_client.c:2813 | fast-b.tsv:1558[escalate] |
| UIA | uiautomationcore!uia_nested_node_provider_respond_to_win_event | dlls/uiautomationcore/uia_client.c:2947 | fast-b.tsv:1559[escalate] |
| W-MEDIA | windows.media.devices!DefaultAudioCapture/RenderDeviceChanged events | docs/stubs/escalate-5.md:4 | triage-5.tsv:445[escalate] |
| W-MEDIA | windows.media.devices!default device change events (agent note 3) | docs/stubs/escalate-5.md:45 | triage-5.tsv:456[escalate] |
| W-MEDIA | windows.media.mediacontrol!AudioStateMonitor creators, DisplayUpdater VideoProperties/ImageProperties/CopyFromFileAsync | docs/stubs/escalate-5.md:3 | triage-5.tsv:444[escalate] |
| W-MEDIA | windows.media.mediacontrol!AudioStateMonitor exports (agent note 1) | docs/stubs/escalate-5.md:32 | triage-5.tsv:454[escalate] |
| W-MEDIA | windows.media.mediacontrol!VideoProperties/ImageProperties/CopyFromFileAsync (agent note 2) | docs/stubs/escalate-5.md:40 | triage-5.tsv:455[escalate] |
| W-MEDIA | windows.media.mediacontrol!display_updater_CopyFromFileAsync | dlls/windows.media.mediacontrol/main.c:870 | triage-5.tsv:27[escalate] |
| W-OTHER | geolocation!Geocoordinate.SatelliteData | dlls/geolocation/position.c | fast-b.tsv:2186[escalate] |
| W-OTHER | geolocation!Geocoordinate.VenueData | dlls/geolocation/position.c | fast-b.tsv:2187[escalate] |
| W-OTHER* | windows.networking!SetSocketMediaStreamingMode | dlls/windows.networking/main.c:26 | fast-a-cloud.tsv:69[escalate]; recheck-fast-a.tsv:108[windows-matches]; triage-7.tsv:147[no-ground-truth] |
| W-OTHER | windows.networking.connectivity!IConnectionProfileFilter2/3 | dlls/windows.networking.connectivity/network_information.c | fast-b.tsv:2185[escalate] |
| W-OTHER | windows.networking.connectivity!connection_profile_QueryInterface | dlls/windows.networking.connectivity/network_information.c:864 | fast-b.tsv:1022[escalate] |
| W-OTHER | windows.web!content_get_Headers | dlls/windows.web/http_content.c:92 | fast-a.tsv:198[escalate] |
| W-OTHER | windows.web!request_get_TransportInformation | dlls/windows.web/http_request.c:136 | fast-a.tsv:186[escalate] |
| W-SPEECH* | windows.media.speech!SystemSpeechLanguage / TrySetSystemSpeechLanguageAsync | ? | recheck-fast-a.tsv:206[escalate]; triage-5.tsv:451[no-ground-truth] |
| W-SPEECH | windows.media.speech!recognizer_factory_Create without a capture device | docs/stubs/escalate-5.md:26 | triage-5.tsv:453[escalate] |
| W-SPEECH | windows.media.speech!speech recognition engine | docs/stubs/escalate-5.md:12 | triage-5.tsv:449[escalate] |
| W-SPEECH | windows.media.speech!speech synthesis voices | docs/stubs/escalate-5.md:18 | triage-5.tsv:450[escalate] |
| XML-PARSER | msxml3!domdoctype_get_entities | dlls/msxml3/doctype.c:497 | fast-a.tsv:99[escalate] |
| XML-PARSER | msxml3!domdoctype_get_notations | dlls/msxml3/doctype.c:506 | fast-a.tsv:100[escalate] |
| XML-PARSER | msxml3!schema_cache_getDeclaration | dlls/msxml3/schema.c:1564 | fast-a.tsv:107[escalate] |
| XML-PARSER | msxml3!schema_cache_getSchema | dlls/msxml3/schema.c:1554 | fast-a.tsv:106[escalate] |
| XML-PARSER | msxml3!xmlparser_Abort | dlls/msxml3/xmlparser.c:134 | fast-a.tsv:458[escalate] |
| XML-PARSER | msxml3!xmlparser_ExpandEntity | dlls/msxml3/xmlparser.c:289 | fast-a.tsv:468[escalate] |
| XML-PARSER | msxml3!xmlparser_GetAbsolutePosition | dlls/msxml3/xmlparser.c:161 | fast-a.tsv:580[escalate] |
| XML-PARSER | msxml3!xmlparser_GetErrorInfo | dlls/msxml3/xmlparser.c:189 | fast-a.tsv:460[escalate] |
| XML-PARSER | msxml3!xmlparser_GetLastError | dlls/msxml3/xmlparser.c:180 | fast-a.tsv:459[escalate] |
| XML-PARSER | msxml3!xmlparser_GetLineBuffer | dlls/msxml3/xmlparser.c:170 | fast-a.tsv:581[escalate] |
| XML-PARSER | msxml3!xmlparser_GetLineNumber | dlls/msxml3/xmlparser.c:143 | fast-a.tsv:578[escalate] |
| XML-PARSER | msxml3!xmlparser_GetLinePosition | dlls/msxml3/xmlparser.c:152 | fast-a.tsv:579[escalate] |
| XML-PARSER | msxml3!xmlparser_GetRoot | dlls/msxml3/xmlparser.c:306 | fast-a.tsv:470[escalate] |
| XML-PARSER | msxml3!xmlparser_GetSecureBaseURL | dlls/msxml3/xmlparser.c:369 | fast-a.tsv:475[escalate] |
| XML-PARSER | msxml3!xmlparser_GetURL | dlls/msxml3/xmlparser.c:207 | fast-a.tsv:461[escalate] |
| XML-PARSER | msxml3!xmlparser_Load | dlls/msxml3/xmlparser.c:227 | fast-a.tsv:463[escalate] |
| XML-PARSER | msxml3!xmlparser_LoadDTD | dlls/msxml3/xmlparser.c:261 | fast-a.tsv:465[escalate] |
| XML-PARSER | msxml3!xmlparser_LoadEntity | dlls/msxml3/xmlparser.c:271 | fast-a.tsv:466[escalate] |
| XML-PARSER | msxml3!xmlparser_ParseEntity | dlls/msxml3/xmlparser.c:281 | fast-a.tsv:467[escalate] |
| XML-PARSER | msxml3!xmlparser_PushData | dlls/msxml3/xmlparser.c:253 | fast-a.tsv:464[escalate] |
| XML-PARSER | msxml3!xmlparser_Reset | dlls/msxml3/xmlparser.c:340 | fast-a.tsv:473[escalate] |
| XML-PARSER | msxml3!xmlparser_Run | dlls/msxml3/xmlparser.c:315 | fast-a.tsv:471[escalate] |
| XML-PARSER | msxml3!xmlparser_SetRoot | dlls/msxml3/xmlparser.c:297 | fast-a.tsv:469[escalate] |
| XML-PARSER | msxml3!xmlparser_SetSecureBaseURL | dlls/msxml3/xmlparser.c:360 | fast-a.tsv:474[escalate] |
| XML-PARSER | msxml3!xmlparser_SetURL | dlls/msxml3/xmlparser.c:217 | fast-a.tsv:462[escalate] |
| XML-PARSER | msxml3!xmlparser_Suspend | dlls/msxml3/xmlparser.c:331 | fast-a.tsv:472[escalate] |

## 4. Escalations that look mis-filed (small, documented, implementable now)

Each row names the item and the small action a drafter can take straight away. 'Close' means the evidence itself says the work is done or fixed elsewhere.

| item | action | basis |
|---|---|---|
| advapi32!GetNamedSecurityInfoExA | A form: convert strings/TRUSTEEs and call the W function (W of BuildSecurityDescriptor already exists) | pure A->W wrappers, no policy |
| advapi32!GetSecurityInfoExA | A form: convert strings/TRUSTEEs and call the W function (W of BuildSecurityDescriptor already exists) | pure A->W wrappers, no policy |
| advapi32!LookupSecurityDescriptorPartsA | A form: convert strings/TRUSTEEs and call the W function (W of BuildSecurityDescriptor already exists) | pure A->W wrappers, no policy |
| advapi32!BuildSecurityDescriptorA | A form: convert strings/TRUSTEEs and call the W function (W of BuildSecurityDescriptor already exists) | pure A->W wrappers, no policy |
| advapi32!GetAuditedPermissionsFromAclA | A form: convert strings/TRUSTEEs and call the W function (W of BuildSecurityDescriptor already exists) | pure A->W wrappers, no policy |
| advapi32!GetEffectiveRightsFromAclA | A form: convert strings/TRUSTEEs and call the W function (W of BuildSecurityDescriptor already exists) | pure A->W wrappers, no policy |
| advapi32!ObjectCloseAuditAlarmA | A->W plus the documented SeAuditPrivilege check (ERROR_PRIVILEGE_NOT_HELD) and set *GenerateOnClose | documented error table; token privilege check already exists |
| advapi32!ObjectOpenAuditAlarmA | A->W plus the documented SeAuditPrivilege check (ERROR_PRIVILEGE_NOT_HELD) and set *GenerateOnClose | documented error table; token privilege check already exists |
| advapi32!ObjectPrivilegeAuditAlarmA | A->W plus the documented SeAuditPrivilege check (ERROR_PRIVILEGE_NOT_HELD) and set *GenerateOnClose | documented error table; token privilege check already exists |
| advapi32!PrivilegedServiceAuditAlarmA | A->W plus the documented SeAuditPrivilege check (ERROR_PRIVILEGE_NOT_HELD) and set *GenerateOnClose | documented error table; token privilege check already exists |
| kernelbase!AccessCheckByType | fix inverted status ("*status = TRUE; return !*status") - plain bug | test security.c, learn.microsoft.com AccessCheckByType |
| kernelbase!SetAclInformation | SetAclInformation: AclRevisionInformation per docs; SetPrivateObjectSecurity(Ex): call RtlSetSecurityObject(Ex); GetWindowsAccountDomainSid: validate domain SID (4 subauthorities, S-1-5-21 prefix) and fail otherwise | documented, no new subsystem |
| kernelbase!SetPrivateObjectSecurity | SetAclInformation: AclRevisionInformation per docs; SetPrivateObjectSecurity(Ex): call RtlSetSecurityObject(Ex); GetWindowsAccountDomainSid: validate domain SID (4 subauthorities, S-1-5-21 prefix) and fail otherwise | documented, no new subsystem |
| kernelbase!SetPrivateObjectSecurityEx | SetAclInformation: AclRevisionInformation per docs; SetPrivateObjectSecurity(Ex): call RtlSetSecurityObject(Ex); GetWindowsAccountDomainSid: validate domain SID (4 subauthorities, S-1-5-21 prefix) and fail otherwise | documented, no new subsystem |
| kernelbase!GetWindowsAccountDomainSid | SetAclInformation: AclRevisionInformation per docs; SetPrivateObjectSecurity(Ex): call RtlSetSecurityObject(Ex); GetWindowsAccountDomainSid: validate domain SID (4 subauthorities, S-1-5-21 prefix) and fail otherwise | documented, no new subsystem |
| ntdll!RtlAddAccessAllowedObjectAce | documented ACCESS_*_OBJECT_ACE layouts (GUID flags); RtlDefaultNpAcl builds the documented default DACL (SYSTEM/Administrators full, owner, Everyone read) instead of NULL | ACE layout is public |
| ntdll!RtlAddAccessDeniedObjectAce | documented ACCESS_*_OBJECT_ACE layouts (GUID flags); RtlDefaultNpAcl builds the documented default DACL (SYSTEM/Administrators full, owner, Everyone read) instead of NULL | ACE layout is public |
| ntdll!RtlAddAuditAccessObjectAce | documented ACCESS_*_OBJECT_ACE layouts (GUID flags); RtlDefaultNpAcl builds the documented default DACL (SYSTEM/Administrators full, owner, Everyone read) instead of NULL | ACE layout is public |
| ntdll!RtlDefaultNpAcl | documented ACCESS_*_OBJECT_ACE layouts (GUID flags); RtlDefaultNpAcl builds the documented default DACL (SYSTEM/Administrators full, owner, Everyone read) instead of NULL | ACE layout is public |
| ntdll!RtlDeleteSecurityObject | close: evidence says behaviour already matches Windows | already matches |
| ntdll!NtSetInformationThread | one documented information class each: ThreadPriority mapping, TokenUIAccess, TokenSessionId validation, KeyWriteTime, replace bogus STATUS_ACCESS_VIOLATION with STATUS_INVALID_HANDLE/ the documented status, IOCTL_STORAGE structures | documented classes with tests |
| ntdll!NtQueryInformationToken | one documented information class each: ThreadPriority mapping, TokenUIAccess, TokenSessionId validation, KeyWriteTime, replace bogus STATUS_ACCESS_VIOLATION with STATUS_INVALID_HANDLE/ the documented status, IOCTL_STORAGE structures | documented classes with tests |
| ntdll!NtSetInformationToken | one documented information class each: ThreadPriority mapping, TokenUIAccess, TokenSessionId validation, KeyWriteTime, replace bogus STATUS_ACCESS_VIOLATION with STATUS_INVALID_HANDLE/ the documented status, IOCTL_STORAGE structures | documented classes with tests |
| ntdll!NtSetInformationKey | one documented information class each: ThreadPriority mapping, TokenUIAccess, TokenSessionId validation, KeyWriteTime, replace bogus STATUS_ACCESS_VIOLATION with STATUS_INVALID_HANDLE/ the documented status, IOCTL_STORAGE structures | documented classes with tests |
| ntdll!NtRollbackTransaction | one documented information class each: ThreadPriority mapping, TokenUIAccess, TokenSessionId validation, KeyWriteTime, replace bogus STATUS_ACCESS_VIOLATION with STATUS_INVALID_HANDLE/ the documented status, IOCTL_STORAGE structures | documented classes with tests |
| ntdll!CDROM_GetDeviceNumber | one documented information class each: ThreadPriority mapping, TokenUIAccess, TokenSessionId validation, KeyWriteTime, replace bogus STATUS_ACCESS_VIOLATION with STATUS_INVALID_HANDLE/ the documented status, IOCTL_STORAGE structures | documented classes with tests |
| ntdll!CDROM_GetMediaType | one documented information class each: ThreadPriority mapping, TokenUIAccess, TokenSessionId validation, KeyWriteTime, replace bogus STATUS_ACCESS_VIOLATION with STATUS_INVALID_HANDLE/ the documented status, IOCTL_STORAGE structures | documented classes with tests |
| ntdll!symbolic link relative target and directory link removal | close: fixed in stubs-6 (handed to drafter 6) | evidence says fixed |
| msvcp140!std::filesystem conformance failures | close: fixed with the ntdll symlink change in stubs-6 | evidence says fixed |
| msvcrt!_heapmin (round 4) | RtlCompactHeap in ntdll then _heapmin works | root cause identified |
| msvcrt!_heapmin | RtlCompactHeap in ntdll then _heapmin works | root cause identified |
| kernel32!SetFirmwareEnvironmentVariableA | A->W once the W behaviour is decided (privilege check / mount point layout) | pure A->W wrappers |
| kernel32!FindFirstVolumeMountPointA | A->W once the W behaviour is decided (privilege check / mount point layout) | pure A->W wrappers |
| kernel32!SetVolumeMountPointA | A->W once the W behaviour is decided (privilege check / mount point layout) | pure A->W wrappers |
| advapi32!CredMarshalCredentialW | CRED_MARSHAL_TYPE blob encoding and NULL-target parameter errors are documented and tested (cred.c:209-221) | documented encoding, tests |
| advapi32!CredUnmarshalCredentialW | CRED_MARSHAL_TYPE blob encoding and NULL-target parameter errors are documented and tested (cred.c:209-221) | documented encoding, tests |
| advapi32!CredReadDomainCredentialsW | CRED_MARSHAL_TYPE blob encoding and NULL-target parameter errors are documented and tested (cred.c:209-221) | documented encoding, tests |
| advapi32!SetServiceBits | SetServiceBits stores validated bits; PerfAddCounters size checks (perf.c:246-254); WmiOpenBlock returns ERROR_WMI_GUID_NOT_FOUND not success; NotifyBootConfigStatus per docs | documented, tests |
| advapi32!PerfAddCounters | SetServiceBits stores validated bits; PerfAddCounters size checks (perf.c:246-254); WmiOpenBlock returns ERROR_WMI_GUID_NOT_FOUND not success; NotifyBootConfigStatus per docs | documented, tests |
| advapi32!WmiOpenBlock | SetServiceBits stores validated bits; PerfAddCounters size checks (perf.c:246-254); WmiOpenBlock returns ERROR_WMI_GUID_NOT_FOUND not success; NotifyBootConfigStatus per docs | documented, tests |
| advapi32!NotifyBootConfigStatus | SetServiceBits stores validated bits; PerfAddCounters size checks (perf.c:246-254); WmiOpenBlock returns ERROR_WMI_GUID_NOT_FOUND not success; NotifyBootConfigStatus per docs | documented, tests |
| services!svcctl_NotifyBootConfigStatus | BootAcceptable path: first call succeeds, second fails as documented | documented |
| user32!LoadKeyboardLayoutW | KLF_* flags and HKL return value (input.c:3605, 6040; imm32.c:3725); BlockInput flag on the window station; SetSystemCursor through the cursor table | tests with todo_wine, Firefox/VSCode hit them |
| user32!LoadKeyboardLayoutEx | KLF_* flags and HKL return value (input.c:3605, 6040; imm32.c:3725); BlockInput flag on the window station; SetSystemCursor through the cursor table | tests with todo_wine, Firefox/VSCode hit them |
| user32!UnloadKeyboardLayout | KLF_* flags and HKL return value (input.c:3605, 6040; imm32.c:3725); BlockInput flag on the window station; SetSystemCursor through the cursor table | tests with todo_wine, Firefox/VSCode hit them |
| user32!BlockInput | KLF_* flags and HKL return value (input.c:3605, 6040; imm32.c:3725); BlockInput flag on the window station; SetSystemCursor through the cursor table | tests with todo_wine, Firefox/VSCode hit them |
| user32!SetSystemCursor | KLF_* flags and HKL return value (input.c:3605, 6040; imm32.c:3725); BlockInput flag on the window station; SetSystemCursor through the cursor table | tests with todo_wine, Firefox/VSCode hit them |
| wtsapi32!WTSEnumerateProcessesExW | honour session id; wait on a session event | documented, tests wtsapi32 |
| wtsapi32!WTSWaitSystemEvent | honour session id; wait on a session event | documented, tests wtsapi32 |
| rtworkq!RtwqSetDeadline | timers and IO-completion association per docs | documented |
| rtworkq!RtwqSetDeadline2 | timers and IO-completion association per docs | documented |
| rtworkq!RtwqCancelDeadline | timers and IO-completion association per docs | documented |
| rtworkq!RtwqJoinWorkQueue | timers and IO-completion association per docs | documented |
| rtworkq!RtwqUnjoinWorkQueue | timers and IO-completion association per docs | documented |
| kernelbase!AddConsoleAliasA | per-console alias table in conhost, documented in learn.microsoft.com/windows/console | documented, self-contained |
| kernelbase!AddConsoleAliasW | per-console alias table in conhost, documented in learn.microsoft.com/windows/console | documented, self-contained |
| kernelbase!GetConsoleAliasA | per-console alias table in conhost, documented in learn.microsoft.com/windows/console | documented, self-contained |
| kernelbase!GetConsoleAliasW | per-console alias table in conhost, documented in learn.microsoft.com/windows/console | documented, self-contained |
| kernelbase!GetConsoleAliasesLengthA | per-console alias table in conhost, documented in learn.microsoft.com/windows/console | documented, self-contained |
| kernelbase!GetConsoleAliasesLengthW | per-console alias table in conhost, documented in learn.microsoft.com/windows/console | documented, self-contained |
| kernelbase!GetConsoleAliasExesLengthA | per-console alias table in conhost, documented in learn.microsoft.com/windows/console | documented, self-contained |
| kernelbase!GetConsoleAliasExesLengthW | per-console alias table in conhost, documented in learn.microsoft.com/windows/console | documented, self-contained |
| kernelbase!EnumResourceLanguagesExA | honour RESOURCE_ENUM_MUI / LN / VALIDATE flags per docs (enumerate the .mui file) | documented flags |
| kernelbase!EnumResourceLanguagesExW | honour RESOURCE_ENUM_MUI / LN / VALIDATE flags per docs (enumerate the .mui file) | documented flags |
| kernelbase!EnumResourceNamesExA | honour RESOURCE_ENUM_MUI / LN / VALIDATE flags per docs (enumerate the .mui file) | documented flags |
| kernelbase!EnumResourceNamesExW | honour RESOURCE_ENUM_MUI / LN / VALIDATE flags per docs (enumerate the .mui file) | documented flags |
| kernelbase!EnumResourceTypesExA | honour RESOURCE_ENUM_MUI / LN / VALIDATE flags per docs (enumerate the .mui file) | documented flags |
| hal!KfLowerIrql | per-thread IRQL value shared with ntoskrnl | tiny, documented |
| hal!KfRaiseIrql | per-thread IRQL value shared with ntoskrnl | tiny, documented |
| hal!KeGetCurrentIrql | per-thread IRQL value shared with ntoskrnl | tiny, documented |
| ntoskrnl.exe!KfRaiseIrql | IRQL value; probe checks (alignment, wrap, user range) per docs; priority from the thread | documented, driver.c tests |
| ntoskrnl.exe!KeLowerIrql | IRQL value; probe checks (alignment, wrap, user range) per docs; priority from the thread | documented, driver.c tests |
| ntoskrnl.exe!ProbeForRead | IRQL value; probe checks (alignment, wrap, user range) per docs; priority from the thread | documented, driver.c tests |
| ntoskrnl.exe!ProbeForWrite | IRQL value; probe checks (alignment, wrap, user range) per docs; priority from the thread | documented, driver.c tests |
| ntoskrnl.exe!KeQueryPriorityThread | IRQL value; probe checks (alignment, wrap, user range) per docs; priority from the thread | documented, driver.c tests |
| ntoskrnl.exe!MmCopyVirtualMemory | IRQL value; probe checks (alignment, wrap, user range) per docs; priority from the thread | documented, driver.c tests |
| d3d11!d3d11_device_ReadFromSubresource | ReadFromSubresource = map+copy; MinLOD store/clamp | documented |
| d3d11!d3d11_device_context_SetResourceMinLOD | ReadFromSubresource = map+copy; MinLOD store/clamp | documented |
| d3d11!d3d11_device_context_GetResourceMinLOD | ReadFromSubresource = map+copy; MinLOD store/clamp | documented |
| ddraw!ddraw_surface_create | allow DDSCAPS_FLIP|TEXTURE per docs | documented |
| cabinet!FDITruncateCabinet | FDITruncateCabinet is documented; XPRESS / XPRESS Huffman are published specs ([MS-XCA]) | open specs |
| cabinet!create_handle | FDITruncateCabinet is documented; XPRESS / XPRESS Huffman are published specs ([MS-XCA]) | open specs |
| uianimation!animation_storyboard_RepeatBetweenKeyframes | documented repeat semantics | documented |
| hid!HidP_TranslateUsagesToI8042ScanCodes | documented scan-code translation tables | documented |
| dwrite!layout_apply_text_alignment | JUSTIFIED: distribute extra space across spaces per docs | documented |
| sensapi!IsDestinationReachableA | route/DNS reachability check, documented return | documented |
| sensapi!IsDestinationReachableW | route/DNS reachability check, documented return | documented |
| connect!IsInternetConnected | default-route check, documented S_OK/S_FALSE | documented |
| ws2_32!WS2_DisconnectEx | TF_REUSE_SOCKET: dup a fresh socket | documented |
| mshtml!HTMLPerformance_toJSON | toJSON via script Object helper; responseBody SAFEARRAY of bytes; frames collection; travel log lookup | documented, small |
| mshtml!HTMLPerformanceTiming_toJSON | toJSON via script Object helper; responseBody SAFEARRAY of bytes; frames collection; travel log lookup | documented, small |
| mshtml!HTMLPerformanceNavigation_toJSON | toJSON via script Object helper; responseBody SAFEARRAY of bytes; frames collection; travel log lookup | documented, small |
| mshtml!HTMLXMLHttpRequest_get_responseBody | toJSON via script Object helper; responseBody SAFEARRAY of bytes; frames collection; travel log lookup | documented, small |
| mshtml!HTMLWindow2_get_frames | toJSON via script Object helper; responseBody SAFEARRAY of bytes; frames collection; travel log lookup | documented, small |
| mshtml!OmHistory_go | toJSON via script Object helper; responseBody SAFEARRAY of bytes; frames collection; travel log lookup | documented, small |
| shell32!active_desktop_AddUrl | documented behaviours; AddDesktopItemWithUI dialog then existing persist code | documented, gate exists for active desktop |
| shell32!active_desktop_AddDesktopItemWithUI | documented behaviours; AddDesktopItemWithUI dialog then existing persist code | documented, gate exists for active desktop |
| shell32!SHCreateLinks | documented behaviours; AddDesktopItemWithUI dialog then existing persist code | documented, gate exists for active desktop |
| shell32!IShellFolder_Printers_fnSetNameOf | documented behaviours; AddDesktopItemWithUI dialog then existing persist code | documented, gate exists for active desktop |
| shell32!ExtractAssociatedIconExW | documented behaviours; AddDesktopItemWithUI dialog then existing persist code | documented, gate exists for active desktop |
| shell32!FolderView2_SetGroupSubsetCount | documented behaviours; AddDesktopItemWithUI dialog then existing persist code | documented, gate exists for active desktop |
| shell32!ISF_Desktop_fnSetNameOf | documented behaviours; AddDesktopItemWithUI dialog then existing persist code | documented, gate exists for active desktop |
| query!LoadIFilter | read HKCR\.ext PersistentHandler / IFilter registration | documented |
| tdh!TdhGetProperty | manifest parser already available (patch 3675) | documented structures |
| tdh!TdhGetPropertySize | manifest parser already available (patch 3675) | documented structures |
| tdh!TdhFormatProperty | manifest parser already available (patch 3675) | documented structures |
| tdh!TdhGetEventMapInformation | manifest parser already available (patch 3675) | documented structures |
| tdh!TdhEnumerateManifestProviderEvents | manifest parser already available (patch 3675) | documented structures |
| setupapi!CM_Request_Device_EjectA | local machine name accepted; others ERROR_INVALID_MACHINENAME; eject via sg-admind/udisks | documented |
| setupapi!CM_Request_Device_EjectW | local machine name accepted; others ERROR_INVALID_MACHINENAME; eject via sg-admind/udisks | documented |
| setupapi!SetupDiCreateDeviceInfoListExW | local machine name accepted; others ERROR_INVALID_MACHINENAME; eject via sg-admind/udisks | documented |
| setupapi!SetupDiGetClassDevsExW | local machine name accepted; others ERROR_INVALID_MACHINENAME; eject via sg-admind/udisks | documented |
| setupapi!SetupDiOpenClassRegKeyExW | local machine name accepted; others ERROR_INVALID_MACHINENAME; eject via sg-admind/udisks | documented |
| sfc_os!SfcIsFileProtected | documented FALSE for unprotected; WRP list optional | documented |
| sfc_os!SfcIsKeyProtected | documented FALSE for unprotected; WRP list optional | documented |
| mstask!MSTASK_ITask_SetTaskFlags | persist flags in the .job file; patch 3651 already keeps them in memory | partial already landed |
| mstask!MSTASK_ITask_GetTaskFlags | persist flags in the .job file; patch 3651 already keeps them in memory | partial already landed |
| msscript.ocx!ScriptControl_put_Timeout | already stores the value (patch 3658): only enforcement missing - close or downgrade | evidence says Done |
| user32!WINNLSEnableIME | partial landed (patches 5213, 5208): close as implemented | evidence says PARTIAL landed |
| user32!WINNLSGetEnableStatus | partial landed (patches 5213, 5208): close as implemented | evidence says PARTIAL landed |
| user32!RegisterTouchHitTestingWindow | partial landed (patches 5213, 5208): close as implemented | evidence says PARTIAL landed |
| gdi32!UpdateICMRegKeyA | patch 5223 implements the three commands: close | evidence says PARTIAL landed |
| gdi32!UpdateICMRegKeyW | patch 5223 implements the three commands: close | evidence says PARTIAL landed |
| scrobj!parse_scriptlet_public | resolved by merging fast-a patches 3080/3096 | evidence says rewritten by unmerged fast-a patches |
| scrobj!parse_scriptlet_registration | resolved by merging fast-a patches 3080/3096 | evidence says rewritten by unmerged fast-a patches |
| scrobj!parse_scriptlet_file | resolved by merging fast-a patches 3080/3096 | evidence says rewritten by unmerged fast-a patches |
| scrobj!parse_scriptlet_script | resolved by merging fast-a patches 3080/3096 | evidence says rewritten by unmerged fast-a patches |
| scrobj!instance_cpc_EnumConnectionPoints | resolved by merging fast-a patches 3080/3096 | evidence says rewritten by unmerged fast-a patches |
| scrobj!instance_cp_EnumConnections | resolved by merging fast-a patches 3080/3096 | evidence says rewritten by unmerged fast-a patches |
| scrobj!ActiveScriptSite_GetDocVersionString | resolved by merging fast-a patches 3080/3096 | evidence says rewritten by unmerged fast-a patches |
| scrobj!global_GetNameSpaceParent | resolved by merging fast-a patches 3080/3096 | evidence says rewritten by unmerged fast-a patches |
| scrobj!scriptlet_GetNameSpaceParent | resolved by merging fast-a patches 3080/3096 | evidence says rewritten by unmerged fast-a patches |
| wbemdisp!sink_cp_EnumConnections | resolved by merging fast-a patch 3093 | evidence says sink defined in unmerged fast-a patch |
| wbemdisp!sink_cpc_EnumConnectionPoints | resolved by merging fast-a patch 3093 | evidence says sink defined in unmerged fast-a patch |
| wbemdisp!services_ExecNotificationQueryAsync | resolved by merging fast-a patch 3093 | evidence says sink defined in unmerged fast-a patch |
| winex11.drv!X11DRV_ConfigureNotify | map synthetic ConfigureNotify through the desktop offset | small, documented |
| winex11.drv!X11DRV_GravityNotify | map synthetic ConfigureNotify through the desktop offset | small, documented |
| windows.web!content_get_Headers | typed header collection / transport information objects | documented classes |
| windows.web!request_get_TransportInformation | typed header collection / transport information objects | documented classes |
| cldapi!CfConnectSyncRoot | DECISIONS 1 already says implement | decision exists |
| cldapi!CfOpenFileWithOplock | DECISIONS 1 already says implement | decision exists |
| dcomp!device_CreateSurfaceFromHandle | DECISIONS 2 already says implement | decision exists |
| dcomp!device_CreateSurfaceFromHwnd | DECISIONS 2 already says implement | decision exists |
| dcomp!compositor_interop_CreateCompositionSurfaceForHandle | DECISIONS 2 already says implement | decision exists |
| shell32!ApplicationAssociationRegistration_SetAppAsDefault | DECISIONS 5 already says implement | decision exists |

Total listed: 145.

## 5. Items escalated in one file but classified differently in another

Confirm or close:

- cryptui!CryptUIDlgViewContext: fast-b=escalate, triage-7=windows-matches
- d3d11!validate_stream_output_entries: fast-b=no-ground-truth, recheck-fast-a=escalate
- d3dx9_36!D3DXComputeTangentFrameEx: fast-b=no-ground-truth, recheck-fast-a=escalate
- extrac32!wWinMain: recheck-fast-a=escalate, triage-7=no-ground-truth
- localspl!fpSetJob: fast-b=no-ground-truth, recheck-fast-a=escalate
- mssign32!PvkFreeCryptProv: fast-b=no-ground-truth, triage-7=escalate
- mssign32!PvkGetCryptProv: fast-b=no-ground-truth, triage-7=escalate
- mssign32!PvkPrivateKeyAcquireContextFromMemory: fast-b=no-ground-truth, triage-7=escalate
- sfc_os!SfcIsFileProtected: fast-a-cloud=escalate, triage-7=windows-matches
- sfc_os!SfcIsKeyProtected: fast-a-cloud=escalate, triage-7=windows-matches
- user32!WINNLSEnableIME: triage-6=no-ground-truth, triage-6c=escalate
- user32!WINNLSGetEnableStatus: triage-6=no-ground-truth, triage-6c=escalate
- virtdisk!DetachVirtualDisk: fast-a-cloud=escalate, triage-7=windows-matches
- wimgapi!WIMGetMountedImages: fast-a-cloud=no-ground-truth, triage-7=escalate
- wimgapi!WIMRegisterMessageCallback: fast-a-cloud=no-ground-truth, triage-7=escalate
- windows.media.speech!SystemSpeechLanguage / TrySetSystemSpeechLanguageAsync: recheck-fast-a=escalate, triage-5=no-ground-truth
- windows.networking!SetSocketMediaStreamingMode: fast-a-cloud=escalate, recheck-fast-a=windows-matches, triage-7=no-ground-truth
- windows.security.enterprisedata!factory_ActivateInstance: fast-b=windows-matches, triage-7=escalate
- winemapi!MAPIAddress: fast-a-cloud=escalate, triage-7=windows-matches
- winemapi!MAPIDetails: fast-a-cloud=escalate, triage-7=windows-matches
- wineps.drv!PSDRV_ExtEscape: fast-a-cloud=windows-matches, triage-7=escalate
- winex11.drv!xic_preedit_caret: fast-a-cloud=escalate, triage-7=no-ground-truth
- wpcap!pcap_findalldevs_ex: fast-a-cloud=escalate, triage-7=no-ground-truth
- wpcap!pcap_open: fast-a-cloud=escalate, triage-7=no-ground-truth
