# Triage quality audit (Fast B, 2026-10-14)

Scope: the ten triage files below, 5,100 lines. Read-only: no tsv was changed. Row N means line N of the tsv as stored (line 1 is the header where the file has one; the recheck file has no header). Method: scripts over every line (exact and near-duplicate evidence clusters, citation regexes, admission keywords, patch-note cross-check against patches/notes/*.txt on each branch), then reading every hit and the not-applicable and escalate lines of every file by eye. The no-ground-truth verdicts already rechecked in ngt-recheck.tsv were not repeated. Counts are of lines I judge suspicious; a line can sit in more than one category. Where a cluster is "genuinely identical" I say so and do not count it.

## Summary table

| file | source | lines | verdicts | (a) cluster | (b) weak wm | (c) n/a callable | (d) partial impl. | (e) esc. no proposal |
|---|---|---|---|---|---|---|---|---|
| coordinator | coordinator.tsv | 283 | not-283 | 0 | 0 | 0 | 0 | 0 |
| fast-a | fast-a.tsv | 720 | no-g382 not-124 esca124 wind90 | 149 | 7 | 0 | 0 | 22 |
| fast-a-cloud | fast-a-cloud.tsv | 112 | no-g30 esca27 impl26 not-23 wind6 | 0 | 0 | 0 | 2 | 0 |
| recheck-fast-a | recheck-fast-a.tsv | 127 | impl59 wind51 no-g16 NOT-1 | 0 | 3 | 0 | 1 | 0 |
| fast-b | fast-b.tsv | 1925 | impl859 esca466 no-g296 wind194 not-56 open54 | 524 | 29 | 2 | 75 | 122 |
| triage-5 | docs/stubs/triage-5.tsv | 498 | wind258 esca112 no-g65 not-63 | 47 | 0 | 18 | 0 | 27 |
| triage-6 | docs/stubs/triage-6.tsv | 919 | esca417 no-g206 not-136 wind86 impl74 | 52 | 3 | 38 | 2 | 25 |
| triage-6b | docs/stubs/triage-6b.tsv | 56 | impl44 no-g10 wind2 | 30 | 0 | 0 | 11 | 0 |
| triage-6c | docs/stubs/triage-6c.tsv | 11 | esca9 no-g1 impl1 | 0 | 0 | 0 | 0 | 1 |
| triage-7 | docs/stubs/triage-7.tsv | 467 | esca149 impl102 no-g96 not-69 wind51 | 0 | 21 | 14 | 10 | 42 |
| total | | 5118 | | 802 | 63 | 72 | 101 | 239 |

Verdict abbreviations: impl = implemented, esca = escalate, wind = windows-matches, no-g = no-ground-truth, not- = not-applicable, open, NOT- = NOT-windows-matches. The triage-6b file has a 7th column (index); the verdict and evidence columns are as in the others.

## Headline findings

1. Worst bulk families: fast-b escalates (d3d11 video 77, riched20 TOM2 47, mscoree ICorDebug 32) bundle trivially answerable accessors with genuinely hard members; fast-a "unknown-IID QueryInterface = not-applicable" (81) and its fast-b/triage-7 twin labelled windows-matches (about 25) use the same argument with opposite verdicts; fast-b "implemented" lines with only "patch NNNN" or one patch summary (explorer 88, d3drm 86, gphoto2 41, dcomp 23, cldapi 21, d2d1 20, wined3d 15, gdiplus 9, dbghelp 8) whose notes say "Not done".
2. Not-applicable lines that are not verdicts: triage-5 has about 25 "resolved by patch N / not an API gap / decisions recorded" lines filed as not-applicable.
3. Same function, different verdicts across files: see the "Cross-file conflicts" section (119 functions outside the fast-a family; for example tbs Tbsi_Context_Create is windows-matches in fast-a-cloud and implemented in triage-7). The integrator should pick one verdict per function before merging.
4. Duplicates inside a file (same dll+function listed twice): fast-b 93, triage-5 46, triage-6 7, fast-a-cloud 8, fast-a 4, triage-6b 5, triage-7 1. Not wrong but inflates the counts.

## (a) pasted-cluster

Clusters of 5 or more lines with identical (or near-identical, names stripped) evidence. Genuinely identical and left out: coordinator 283 (all dll16 members: Win16 does not exist in a new WoW64 build), triage-5 msacm.dll16 28, triage-6 ole32 StorageBaseImpl 12, triage-7 VxD 10, winemac.drv members, fast-b dwrite no-FreeType 10, fast-b wined3d pipe/no3d guards, triage-6 ntdll TAPE_* 10 (HAVE_SYS_MTIO_H defined), triage-5 mfobjects GetParameters 18 (the page says "return E_NOTIMPL for default"), triage-5 dmime/dmstyle/winegstreamer test-line clusters (each cites its own ok() line).

### fast-a

- **unknown-IID QueryInterface, 81 not-applicable (12 dlls)** (rows 5-9,80,88,96,98,101,105,108,174-176,178-184,187-195,197,199-202,587-591,593,596-598,609-627,638-654, 81 lines): Cause is the same log line, but "not applicable" assumes no Windows object answers more IIDs; WinRT factories/objects (windows.web, windows.storage, appxdeploymentclient) and hnetcfg/msado15 objects on Windows also answer IAgileObject, IMarshal, INoMarshal, IProvideClassInfo2 and similar. Verified only that the code is a QI default branch, not that each class lacks those on Windows. fast-b/triage-7 label the same pattern windows-matches (inconsistent).
- **mshtml read-only SVG props, 19** (rows 377-389,391,394-398, 19 lines): One cause stated for 19 different properties; the claim that Windows returns no error on set is asserted for all, with no page or test for any.
- **clusapi (4 near-identical texts, 22 lines)** (rows 693-717, 25 lines): The text is an unreachability argument (handle only from OpenCluster, which fails) yet the verdict is no-ground-truth; these cluster functions are callable by Windows programs (they get an invalid-handle error), so an argument-validation verdict per function is missing.
- **jscript constructors (9+ near-identical lines)** (rows 18-31,33, 15 lines): Same sentence for each builtin; the assert in jsdisp_call_value allows METHOD|CONSTRUCT together, which these switch(flags) statements do not list, so "cannot be reached" was shown for the call path but not for the DISPATCH_METHOD|DISPATCH_CONSTRUCT combination.
- **services svcctl opnums (9)** (rows 683-688,690-692, 9 lines): Correct per MS-SCMR but the evidence is a spec chapter pasted with only the opnum changed; opnum 10/22/34 map to named functions (SCSetServiceBits, GetCurrentGroupState) that Windows services do implement locally.

### fast-b

- **d3d11 video, 77 escalates with one text** (rows 1390-1466, 77 lines): Mixed causes pasted as one: the Create*/Process/Blt/decoder members do need a backend, but the VideoProcessorSet*/Get* state accessors (about 44 of them) and the caps queries (GetVideoDecoderProfileCount, CheckVideoDecoderFormat, GetContentProtectionCaps, GetVideoDecoderConfigCount) can be answered truthfully (store the state, report zero profiles) without hardware decode; split them out.
- **riched20 TOM2, 47 escalates with one text** (rows 1304-1350, 47 lines): "TOM2 object model not started" is true for stories/call manager/ITextRange2, but GetVersion, GetWindow, GetClientRect, GetDefaultTabStop/Set, SysBeep, GetCaretType/Set, GetNotificationMode/Set, GetEffectColor/Set are plain accessors or one-line forwards that do not need the object model.
- **mscoree ICorDebug, 32 escalates with one text** (rows 630-661, 32 lines): The process-enumerator members (process_enum_Next/Skip/Reset/Clone) and CorDebug_CanLaunchOrAttach/GetProcess need no debugger back end (an empty enumerator, a failure code); only Stop/ReadMemory/etc. need Mono.
- **mprapi, 11 escalates** (rows 1573-1583, 11 lines): Not an escalation: a merge-order note ("re-check after fast-a merges"). Stale once fast-a 3027 landed; should be re-triaged, not left as escalate.
- **scrobj, 10 escalates** (rows 1584-1593, 10 lines): Same: waits on fast-a patches 3080/3096, a scheduling note rather than a decision for David.
- **dbgeng remote engine, 10** (rows 420-429, 10 lines): Probably one cause, but the list mixes real wire-protocol members with local ones; spot-check before accepting.
- **windowscodecs IWICStream (6)** (rows 335-340, 6 lines): Inferred from a code comment ("native has one implementation"), not from a test of the file-handle stream.
- **shell32 GetDefaultSearchGUID/GetDefaultColumn (12)** (rows 1602-1606,1608-1613,1615, 12 lines): Cites a test that CoCreates the folder CLSID and calls the method for a class list; the same sentence is used for six folders each, so whether each folder was in the list was not checked line by line.
- **shell32 EnumSearches (8)** (rows 1616-1623, 8 lines): Docs say "min client XP, not supported later" for all eight; that is a support statement, not the return code; no test of the method itself (one line admits "weaker evidence").
- **gphoto2.ds, 41 implemented with one text** (rows 140-180, 41 lines): "Unsupported triplets answer TWCC_BADPROTOCOL" fits triplets that really are unsupported, but the same text covers ImageLayout, file transfer, setup and xfer-group members the notes call implemented; "camera paths unverified".
- **explorer, 88 implemented with one text** (rows 52-139, 88 lines): One patch summary on 88 functions (appbar message, 60 IWebBrowser2 accessors, IShellBrowser menu methods); cannot tell from the evidence which of them do real work. Notes for 3615 are shared with dcomp and list "Not done" items.
- **d3drm, 86 implemented with one text** (rows 458-543, 86 lines): Bare "see notes/3638.txt"; the notes say "Not done (no triage written, still open in gap-B): Clone methods, Save, mesh builder delete/optimize/submesh/enable/AddFrame..." although the same notes later claim some of these, so the 86 lines cannot all be verified.
- **bare "patch NNNN" evidence (cldapi 21, d2d1 20, wined3d 15, gdiplus 9, dbghelp 8, dcomp 23)** (rows 1-21,33-35,199-202,222-225,228,243-262,267-274,277-295,347-352,357-365,1073, 96 lines): No per-function evidence at all; the notes for 3627/3621/3622/3629 say "Not done" for several functions and "unverified against Windows" for structure layouts.

### triage-5

- **dpvoice, 22 escalates (2 texts)** (rows 75-77,110-132, 26 lines): Same cause (no voice transport) is real, but Initialize/Connect/Disconnect/StartSession have todo_wine tests (voice.c:121-251) that record Windows return codes (DVERR_NOTRANSPORT); at least those can be implemented.
- **rtworkq MMCSS, 9 escalates** (rows 157-170, 14 lines): "MMCSS design decision" without a proposal; registering a work-queue with an MMCSS task name could be accepted without scheduling.
- **avifil32 "resolved by patch", 7 not-applicable** (rows 472-473,488-489,491,495-496, 7 lines): Status text, not a verdict.

### triage-6

- **win32u D3DKMT, 21 escalates "same as idx 673"** (rows 453-473, 21 lines): Pasted by reference to one design item; the cause (shared-resource subsystem) is genuinely common, but NtGdiDdDDIDestroy*/Open* calls on a handle that was never created could validate the handle and fail as Windows does.
- **kernel32 TxF, 26 escalates** (rows 176-195,198-205, 28 lines): Mostly the same cause (no transaction manager). The "fake transaction" read-only group (10) could be implemented honestly as non-transacted calls only if the transaction handle is validated; asked decision is not stated.
- **netio.sys / ntoskrnl, 23 implemented, generic gate glob** (rows 814-815,840, 3 lines): "gate test/ntoskrnl-*-gate.sh" is a glob, not the gate that checks each function.

### triage-6b

- **onlineid/cryptowinrt/web.core IInspectable, 27 implemented** (rows 19-45,53-55, 30 lines): "Re-read in the base" rather than examined per function; the notes of 5057 list "unverified: GetIids order, IAgileObject, BaseTrust" for all.

## (b) weak windows-matches

windows-matches lines with no docs page or test line in the evidence, or whose evidence admits the Windows result is a guess. "COM-contract" QueryInterface lines are listed because three other files mark the same pattern not-applicable.

### fast-a

- **evidence itself says "guess"** (rows 14,211-216, 7 lines): Labelled windows-matches but the text itself uses this word about the Windows result (guessed, unverified or the page unavailable); the verdict should be no-ground-truth unless a docs sentence elsewhere in the line settles it.

### recheck-fast-a

- **no test line and no docs page cited** (rows 39-40,55, 3 lines): Evidence is reasoning from code or the COM contract only (COM-contract E_NOINTERFACE cases, opnum tables, "FIXME is for unknown kinds"): nothing ties the return code to Windows for that function.

### fast-b

- **no test line and no docs page cited** (rows 297,354,368,402,873,875,887-890,1210,1474,1856,1859,1866,1868,1870, 17 lines): Evidence is reasoning from code or the COM contract only (COM-contract E_NOINTERFACE cases, opnum tables, "FIXME is for unknown kinds"): nothing ties the return code to Windows for that function.
- **evidence itself says "weaker evidence"** (rows 1607,1614,1616-1623, 10 lines): Labelled windows-matches but the text itself uses this word about the Windows result (guessed, unverified or the page unavailable); the verdict should be no-ground-truth unless a docs sentence elsewhere in the line settles it.
- **manual: weak evidence** (rows 1695, 1 lines): riched20 TxGetCharFormat: "no test calls the internal host"
- **manual: weak evidence** (rows 1696, 1 lines): riched20 TxGetExtent: "no test calls the internal host"

### triage-6

- **no test line and no docs page cited** (rows 108,339, 2 lines): Evidence is reasoning from code or the COM contract only (COM-contract E_NOINTERFACE cases, opnum tables, "FIXME is for unknown kinds"): nothing ties the return code to Windows for that function.
- **evidence itself says "404"** (rows 162, 1 lines): Labelled windows-matches but the text itself uses this word about the Windows result (guessed, unverified or the page unavailable); the verdict should be no-ground-truth unless a docs sentence elsewhere in the line settles it.

### triage-7

- **no test line and no docs page cited** (rows 92,145,199,216-219,301,305,308,312,314-315,317,319,324,389,392, 18 lines): Evidence is reasoning from code or the COM contract only (COM-contract E_NOINTERFACE cases, opnum tables, "FIXME is for unknown kinds"): nothing ties the return code to Windows for that function.
- **evidence itself says "unverified"** (rows 187, 1 lines): Labelled windows-matches but the text itself uses this word about the Windows result (guessed, unverified or the page unavailable); the verdict should be no-ground-truth unless a docs sentence elsewhere in the line settles it.
- **manual: weak evidence** (rows 462, 1 lines): rpcss ept_inq_object: "exact Windows status undocumented"
- **manual: weak evidence** (rows 463, 1 lines): rpcss ept_mgmt_delete: "exact Windows status undocumented"

## (c) not-applicable but callable

### fast-b

- **d3dcompiler_43** (rows 913-914, 2 lines): D3DCreateLinker and D3DLoadModule are exports of d3dcompiler_47 (same compiler.c) that Windows programs call; "not exported by _43" does not make the shared code not-applicable for _47.

### triage-5

- **windows.media** (rows 457, 1 lines): Status or bookkeeping lines marked not-applicable (resolved by patch, test-runner notes, intermittent hang); they are not triage verdicts for a function and hide whether the item was examined.
- **windows.media.mediacontrol** (rows 458, 1 lines): Status or bookkeeping lines marked not-applicable (resolved by patch, test-runner notes, intermittent hang); they are not triage verdicts for a function and hide whether the item was examined.
- **dmsynth/dmusic/xact3** (rows 460, 1 lines): Status or bookkeeping lines marked not-applicable (resolved by patch, test-runner notes, intermittent hang); they are not triage verdicts for a function and hide whether the item was examined.
- **evr** (rows 463, 1 lines): Status or bookkeeping lines marked not-applicable (resolved by patch, test-runner notes, intermittent hang); they are not triage verdicts for a function and hide whether the item was examined.
- **avifil32** (rows 472, 1 lines): Status or bookkeeping lines marked not-applicable (resolved by patch, test-runner notes, intermittent hang); they are not triage verdicts for a function and hide whether the item was examined.
- **winegstreamer** (rows 473, 1 lines): Status or bookkeeping lines marked not-applicable (resolved by patch, test-runner notes, intermittent hang); they are not triage verdicts for a function and hide whether the item was examined.
- **mediacontrol/mfplat** (rows 474, 1 lines): Status or bookkeeping lines marked not-applicable (resolved by patch, test-runner notes, intermittent hang); they are not triage verdicts for a function and hide whether the item was examined.
- **winmm** (rows 477, 1 lines): Status or bookkeeping lines marked not-applicable (resolved by patch, test-runner notes, intermittent hang); they are not triage verdicts for a function and hide whether the item was examined.
- **(test runner)** (rows 479, 1 lines): Status or bookkeeping lines marked not-applicable (resolved by patch, test-runner notes, intermittent hang); they are not triage verdicts for a function and hide whether the item was examined.
- **(review)** (rows 485, 1 lines): Status or bookkeeping lines marked not-applicable (resolved by patch, test-runner notes, intermittent hang); they are not triage verdicts for a function and hide whether the item was examined.
- **msvcp90** (rows 488,490, 2 lines): Status or bookkeeping lines marked not-applicable (resolved by patch, test-runner notes, intermittent hang); they are not triage verdicts for a function and hide whether the item was examined.
- **msvcp90/msvcrt** (rows 489, 1 lines): Status or bookkeeping lines marked not-applicable (resolved by patch, test-runner notes, intermittent hang); they are not triage verdicts for a function and hide whether the item was examined.
- **quartz** (rows 491, 1 lines): Status or bookkeeping lines marked not-applicable (resolved by patch, test-runner notes, intermittent hang); they are not triage verdicts for a function and hide whether the item was examined.
- **mfplat** (rows 495,497, 2 lines): Status or bookkeeping lines marked not-applicable (resolved by patch, test-runner notes, intermittent hang); they are not triage verdicts for a function and hide whether the item was examined.
- **msacm32** (rows 496, 1 lines): Status or bookkeeping lines marked not-applicable (resolved by patch, test-runner notes, intermittent hang); they are not triage verdicts for a function and hide whether the item was examined.
- **dinput** (rows 70, 1 lines): MOUSE_ATTRIBUTES_CHANGED check: a flag "never produced by Wine", but a Windows program feeding RAWMOUSE through a hook can set it.

### triage-6

- **ntdll** (rows 2, 1 lines): Fallback when /proc/stat is unreadable: reachable under a sandbox or container with a restricted /proc, and the made-up counters are then returned to every Windows program that polls CPU time.
- **gdi32** (rows 645-654,657-683, 37 lines): Based on "the engine only calls DrvCopyBits/DrvBitBlt". A third-party user-mode printer driver loaded through UMPD can call these Eng*/FONTOBJ_/STROBJ_/PATHOBJ_/BRUSHOBJ_ functions; with patch 5225 (EngCreateClip/EngDeleteClip) drivers can already build their own objects, so the premise is weaker than when written. CLIPOBJ_ppoGetPath is "Reachable" in its own text yet not-applicable.

### triage-7

- **wnaspi32** (rows 204-205, 2 lines): ASPI32 is called by CD burning and scanner programs (SendASPI32Command, GetASPI32DLLVersion); "no devices here" is a feature gap (Linux sg could back it), not not-applicable.
- **certutil** (rows 247, 1 lines): Command-line tools that installers and scripts run on Windows; a program can call them. Stub success or failure matters to the caller (ngen/aspnet_regiis exit codes), so not-applicable hides a decision. certutil line says "the line does not exist in this tree", i.e. the item was not examined.
- **msinfo32** (rows 249, 1 lines): Command-line tools that installers and scripts run on Windows; a program can call them. Stub success or failure matters to the caller (ngen/aspnet_regiis exit codes), so not-applicable hides a decision. certutil line says "the line does not exist in this tree", i.e. the item was not examined.
- **dism** (rows 250, 1 lines): Command-line tools that installers and scripts run on Windows; a program can call them. Stub success or failure matters to the caller (ngen/aspnet_regiis exit codes), so not-applicable hides a decision. certutil line says "the line does not exist in this tree", i.e. the item was not examined.
- **ngen** (rows 255, 1 lines): Command-line tools that installers and scripts run on Windows; a program can call them. Stub success or failure matters to the caller (ngen/aspnet_regiis exit codes), so not-applicable hides a decision. certutil line says "the line does not exist in this tree", i.e. the item was not examined.
- **aspnet_regiis** (rows 256, 1 lines): Command-line tools that installers and scripts run on Windows; a program can call them. Stub success or failure matters to the caller (ngen/aspnet_regiis exit codes), so not-applicable hides a decision. certutil line says "the line does not exist in this tree", i.e. the item was not examined.
- **servicemodelreg** (rows 257, 1 lines): Command-line tools that installers and scripts run on Windows; a program can call them. Stub success or failure matters to the caller (ngen/aspnet_regiis exit codes), so not-applicable hides a decision. certutil line says "the line does not exist in this tree", i.e. the item was not examined.
- **cmd** (rows 239, 1 lines): cmd.exe "copy /d" is used by batch files; the flag falls under what a Windows program can invoke.
- **atmlib** (rows 364-365, 2 lines): Adobe Type Manager exports are called by DTP software; "FALSE says not loaded, true here" is a valid answer but a verdict of windows-matches/implemented, not not-applicable.
- **acledit** (rows 269, 1 lines): vdmdbg VDMEnumProcessWOW/EnumTaskWOW are called by debuggers and process viewers; returning 0 is plausible but is a result, not an inapplicability.
- **vdmdbg** (rows 288-289, 2 lines): vdmdbg VDMEnumProcessWOW/EnumTaskWOW are called by debuggers and process viewers; returning 0 is plausible but is a result, not an inapplicability.

## (d) implemented but partial

Lines whose own evidence admits something is not done, plus the patch-level table at the end of this section (notes files of the patches cited by implemented lines that say "Not done", "NOT available", "Left as stubs", "partial", "NOT rendered").

### fast-a-cloud

- row 34: wscript EventSink_GetTypeInfo: ...mes via it; DISP_E_BADINDEX / DISP_E_UNKNOWNNAME per IDispatch docs); no gate drives the sink directly (existing fastscrobj/fastwbemsink gates pass)...
- row 35: wscript EventSink_GetIDsOfNames: ...mes via it; DISP_E_BADINDEX / DISP_E_UNKNOWNNAME per IDispatch docs); no gate drives the sink directly (existing fastscrobj/fastwbemsink gates pass)...

### recheck-fast-a

- row 114: urlmon CoInternetIsFeatureZoneElevationEnabled: ...hows a yes/no prompt (yes S_FALSE, no S_OK); prompt wording invented, not covered by the gate...

### fast-b

- row 140: gphoto2.ds GPHOTO2_CIEColorGet: ...3617 gphoto2.ds: unsupported triplets answer TWCC_BADPROTOCOL per TWAIN, image layout, file transfer + setup, xfer group, query support, UI-only, units; gate fastb-gapgp ...
- row 141: gphoto2.ds GPHOTO2_ExtImageInfoGet: ...3617 gphoto2.ds: unsupported triplets answer TWCC_BADPROTOCOL per TWAIN, image layout, file transfer + setup, xfer group, query support, UI-only, units; gate fastb-gapgp ...
- row 142: gphoto2.ds GPHOTO2_GrayResponseReset: ...3617 gphoto2.ds: unsupported triplets answer TWCC_BADPROTOCOL per TWAIN, image layout, file transfer + setup, xfer group, query support, UI-only, units; gate fastb-gapgp ...
- row 143: gphoto2.ds GPHOTO2_GrayResponseSet: ...3617 gphoto2.ds: unsupported triplets answer TWCC_BADPROTOCOL per TWAIN, image layout, file transfer + setup, xfer group, query support, UI-only, units; gate fastb-gapgp ...
- row 144: gphoto2.ds GPHOTO2_ImageFileXferGet: ...3617 gphoto2.ds: unsupported triplets answer TWCC_BADPROTOCOL per TWAIN, image layout, file transfer + setup, xfer group, query support, UI-only, units; gate fastb-gapgp ...
- row 145: gphoto2.ds GPHOTO2_ImageLayoutGet: ...3617 gphoto2.ds: unsupported triplets answer TWCC_BADPROTOCOL per TWAIN, image layout, file transfer + setup, xfer group, query support, UI-only, units; gate fastb-gapgp ...
- row 146: gphoto2.ds GPHOTO2_ImageLayoutGetDefault: ...3617 gphoto2.ds: unsupported triplets answer TWCC_BADPROTOCOL per TWAIN, image layout, file transfer + setup, xfer group, query support, UI-only, units; gate fastb-gapgp ...
- row 147: gphoto2.ds GPHOTO2_ImageLayoutReset: ...3617 gphoto2.ds: unsupported triplets answer TWCC_BADPROTOCOL per TWAIN, image layout, file transfer + setup, xfer group, query support, UI-only, units; gate fastb-gapgp ...
- row 148: gphoto2.ds GPHOTO2_ImageLayoutSet: ...3617 gphoto2.ds: unsupported triplets answer TWCC_BADPROTOCOL per TWAIN, image layout, file transfer + setup, xfer group, query support, UI-only, units; gate fastb-gapgp ...
- row 149: gphoto2.ds GPHOTO2_JPEGCompressionGet: ...3617 gphoto2.ds: unsupported triplets answer TWCC_BADPROTOCOL per TWAIN, image layout, file transfer + setup, xfer group, query support, UI-only, units; gate fastb-gapgp ...
- row 150: gphoto2.ds GPHOTO2_JPEGCompressionGetDefault: ...3617 gphoto2.ds: unsupported triplets answer TWCC_BADPROTOCOL per TWAIN, image layout, file transfer + setup, xfer group, query support, UI-only, units; gate fastb-gapgp ...
- row 151: gphoto2.ds GPHOTO2_JPEGCompressionReset: ...3617 gphoto2.ds: unsupported triplets answer TWCC_BADPROTOCOL per TWAIN, image layout, file transfer + setup, xfer group, query support, UI-only, units; gate fastb-gapgp ...
- row 152: gphoto2.ds GPHOTO2_JPEGCompressionSet: ...3617 gphoto2.ds: unsupported triplets answer TWCC_BADPROTOCOL per TWAIN, image layout, file transfer + setup, xfer group, query support, UI-only, units; gate fastb-gapgp ...
- row 153: gphoto2.ds GPHOTO2_Palette8Get: ...3617 gphoto2.ds: unsupported triplets answer TWCC_BADPROTOCOL per TWAIN, image layout, file transfer + setup, xfer group, query support, UI-only, units; gate fastb-gapgp ...
- row 154: gphoto2.ds GPHOTO2_Palette8GetDefault: ...3617 gphoto2.ds: unsupported triplets answer TWCC_BADPROTOCOL per TWAIN, image layout, file transfer + setup, xfer group, query support, UI-only, units; gate fastb-gapgp ...
- row 155: gphoto2.ds GPHOTO2_Palette8Reset: ...3617 gphoto2.ds: unsupported triplets answer TWCC_BADPROTOCOL per TWAIN, image layout, file transfer + setup, xfer group, query support, UI-only, units; gate fastb-gapgp ...
- row 156: gphoto2.ds GPHOTO2_Palette8Set: ...3617 gphoto2.ds: unsupported triplets answer TWCC_BADPROTOCOL per TWAIN, image layout, file transfer + setup, xfer group, query support, UI-only, units; gate fastb-gapgp ...
- row 157: gphoto2.ds GPHOTO2_RGBResponseReset: ...3617 gphoto2.ds: unsupported triplets answer TWCC_BADPROTOCOL per TWAIN, image layout, file transfer + setup, xfer group, query support, UI-only, units; gate fastb-gapgp ...
- row 158: gphoto2.ds GPHOTO2_RGBResponseSet: ...3617 gphoto2.ds: unsupported triplets answer TWCC_BADPROTOCOL per TWAIN, image layout, file transfer + setup, xfer group, query support, UI-only, units; gate fastb-gapgp ...
- row 159: gphoto2.ds GPHOTO2_ICAPUnits: ...3617 gphoto2.ds: unsupported triplets answer TWCC_BADPROTOCOL per TWAIN, image layout, file transfer + setup, xfer group, query support, UI-only, units; gate fastb-gapgp ...
- row 160: gphoto2.ds GPHOTO2_CapabilityQuerySupport: ...3617 gphoto2.ds: unsupported triplets answer TWCC_BADPROTOCOL per TWAIN, image layout, file transfer + setup, xfer group, query support, UI-only, units; gate fastb-gapgp ...
- row 161: gphoto2.ds GPHOTO2_CustomDSDataGet: ...3617 gphoto2.ds: unsupported triplets answer TWCC_BADPROTOCOL per TWAIN, image layout, file transfer + setup, xfer group, query support, UI-only, units; gate fastb-gapgp ...
- row 162: gphoto2.ds GPHOTO2_CustomDSDataSet: ...3617 gphoto2.ds: unsupported triplets answer TWCC_BADPROTOCOL per TWAIN, image layout, file transfer + setup, xfer group, query support, UI-only, units; gate fastb-gapgp ...
- row 163: gphoto2.ds GPHOTO2_ChangeDirectory: ...3617 gphoto2.ds: unsupported triplets answer TWCC_BADPROTOCOL per TWAIN, image layout, file transfer + setup, xfer group, query support, UI-only, units; gate fastb-gapgp ...
- row 164: gphoto2.ds GPHOTO2_CreateDirectory: ...3617 gphoto2.ds: unsupported triplets answer TWCC_BADPROTOCOL per TWAIN, image layout, file transfer + setup, xfer group, query support, UI-only, units; gate fastb-gapgp ...
- row 165: gphoto2.ds GPHOTO2_FileSystemDelete: ...3617 gphoto2.ds: unsupported triplets answer TWCC_BADPROTOCOL per TWAIN, image layout, file transfer + setup, xfer group, query support, UI-only, units; gate fastb-gapgp ...
- row 166: gphoto2.ds GPHOTO2_FormatMedia: ...3617 gphoto2.ds: unsupported triplets answer TWCC_BADPROTOCOL per TWAIN, image layout, file transfer + setup, xfer group, query support, UI-only, units; gate fastb-gapgp ...
- row 167: gphoto2.ds GPHOTO2_FileSystemGetClose: ...3617 gphoto2.ds: unsupported triplets answer TWCC_BADPROTOCOL per TWAIN, image layout, file transfer + setup, xfer group, query support, UI-only, units; gate fastb-gapgp ...
- row 168: gphoto2.ds GPHOTO2_FileSystemGetFirstFile: ...3617 gphoto2.ds: unsupported triplets answer TWCC_BADPROTOCOL per TWAIN, image layout, file transfer + setup, xfer group, query support, UI-only, units; gate fastb-gapgp ...
- row 169: gphoto2.ds GPHOTO2_FileSystemGetInfo: ...3617 gphoto2.ds: unsupported triplets answer TWCC_BADPROTOCOL per TWAIN, image layout, file transfer + setup, xfer group, query support, UI-only, units; gate fastb-gapgp ...
- row 170: gphoto2.ds GPHOTO2_FileSystemGetNextFile: ...3617 gphoto2.ds: unsupported triplets answer TWCC_BADPROTOCOL per TWAIN, image layout, file transfer + setup, xfer group, query support, UI-only, units; gate fastb-gapgp ...
- row 171: gphoto2.ds GPHOTO2_FileSystemRename: ...3617 gphoto2.ds: unsupported triplets answer TWCC_BADPROTOCOL per TWAIN, image layout, file transfer + setup, xfer group, query support, UI-only, units; gate fastb-gapgp ...
- row 172: gphoto2.ds GPHOTO2_PassThrough: ...3617 gphoto2.ds: unsupported triplets answer TWCC_BADPROTOCOL per TWAIN, image layout, file transfer + setup, xfer group, query support, UI-only, units; gate fastb-gapgp ...
- row 173: gphoto2.ds GPHOTO2_SetupFileXferGet: ...3617 gphoto2.ds: unsupported triplets answer TWCC_BADPROTOCOL per TWAIN, image layout, file transfer + setup, xfer group, query support, UI-only, units; gate fastb-gapgp ...
- row 174: gphoto2.ds GPHOTO2_SetupFileXferGetDefault: ...3617 gphoto2.ds: unsupported triplets answer TWCC_BADPROTOCOL per TWAIN, image layout, file transfer + setup, xfer group, query support, UI-only, units; gate fastb-gapgp ...
- row 175: gphoto2.ds GPHOTO2_SetupFileXferReset: ...3617 gphoto2.ds: unsupported triplets answer TWCC_BADPROTOCOL per TWAIN, image layout, file transfer + setup, xfer group, query support, UI-only, units; gate fastb-gapgp ...
- row 176: gphoto2.ds GPHOTO2_SetupFileXferSet: ...3617 gphoto2.ds: unsupported triplets answer TWCC_BADPROTOCOL per TWAIN, image layout, file transfer + setup, xfer group, query support, UI-only, units; gate fastb-gapgp ...
- row 177: gphoto2.ds GPHOTO2_EnableDSUIOnly: ...3617 gphoto2.ds: unsupported triplets answer TWCC_BADPROTOCOL per TWAIN, image layout, file transfer + setup, xfer group, query support, UI-only, units; gate fastb-gapgp ...
- row 178: gphoto2.ds GPHOTO2_XferGroupGet: ...3617 gphoto2.ds: unsupported triplets answer TWCC_BADPROTOCOL per TWAIN, image layout, file transfer + setup, xfer group, query support, UI-only, units; gate fastb-gapgp ...
- row 179: gphoto2.ds GPHOTO2_XferGroupSet: ...3617 gphoto2.ds: unsupported triplets answer TWCC_BADPROTOCOL per TWAIN, image layout, file transfer + setup, xfer group, query support, UI-only, units; gate fastb-gapgp ...
- row 180: gphoto2.ds DS_Entry: ...3617 gphoto2.ds: unsupported triplets answer TWCC_BADPROTOCOL per TWAIN, image layout, file transfer + setup, xfer group, query support, UI-only, units; gate fastb-gapgp ...
- row 556: d3dx9_36 d3dx_effect_GetFunctionDesc: ...le exists: GetFunctionDesc -> D3DERR_INVALIDCALL; test in test_effect_unsupported_shader...
- row 557: d3dx9_36 d3dx_effect_GetFunction: ...ct with Functions == 0 (docs: NULL for an invalid index); test_effect_unsupported_shader...
- row 558: d3dx9_36 d3dx_effect_GetFunctionByName: ...36: no function of that name (docs: NULL when not found); test_effect_unsupported_shader...
- row 559: d3dx9_36 d3dx_effect_SetArrayRange: ...hader.c set_constants); validation -> D3DERR_INVALIDCALL; test_effect_unsupported_shader (validation only: the device path is not covered by a test)...
- row 676: d3dx9_36 d3dx_load_pixels_from_pixels: ...onversion; test_sg_gap4_surface, mutant PACKED_SWAP caught; A1 source not done (bit order undocumented)...
- row 690: imagehlp BindImageEx: ...c test_bind (own PE32/PE32+ images), mutants BIND_ORD NOUPD IAT STAMP PARTIAL FWDREF ALL GROW caught...
- row 757: shcore shstream_LockRegion: ...pi/tests/istream.c:149-165, Vista behaviour); memory/registry streams stay E_NOTIMPL (no ground truth); mutant SHCORE_LOCK caught...
- row 758: shcore shstream_UnlockRegion: ...pi/tests/istream.c:149-165, Vista behaviour); memory/registry streams stay E_NOTIMPL (no ground truth); mutant SHCORE_LOCK caught...
- row 759: shcore shstream_Clone: ...tream.c:812 todo_wine removed: Win8+ returns S_OK); file stream Clone stays E_NOTIMPL per istream.c:177; mutant SHCORE_CLONE caught...
- row 827: propsys PROPVAR_ConvertNumber: ...r types are outside the documented PropVariantChangeType table (docs: unsupported coercion returns an error), E_NOTIMPL code itself unverified...
- row 917: diasymreader ClassFactoryImpl_LockServer: ...sFactory::LockServer returns S_OK (supersedes the earlier open line); not covered by the gate: diasymreader.dll cannot be loaded by name from a gate prefix...
- row 1053: oledb32 convert_GetConversionSize: ...TR/WSTR/BSTR -> BYTES size + matching DataConvert; other source types stay E_NOTIMPL like DataConvert (no documented rule): hex rule GUESSED; gate fastb-oledb32, 4 mutant...
- row 1173: explorer webbrowser_get_Document: ... IShellFolderViewDual via GetItemObject(SVGIO_BACKGROUND, IDispatch); not covered by the gate (needs the shell windows COM server)...
- row 1676: windowscodecs IMILUnknown2Impl_QueryInterface: ...public interface (only IMILUnknown1's undocumented members would), so no gate can reach it: build-verified, windowscodecs conformance run...
- row 1677: windowscodecs IMILUnknown2Impl_AddRef: ...public interface (only IMILUnknown1's undocumented members would), so no gate can reach it: build-verified, windowscodecs conformance run...
- row 1678: windowscodecs IMILUnknown2Impl_Release: ...public interface (only IMILUnknown1's undocumented members would), so no gate can reach it: build-verified, windowscodecs conformance run...
- row 1724: shell32 FolderView2_SetGroupSubsetCount: ...3694 (gate fastb-sfview, 11 mutants caught): stored (zero turns off); NOT rendered: comctl32 listview has no group subset support...
- row 1736: riched20 textrange_expand: .../window/charformat/paraformat/cell/row/table/object added; tomSection stays E_NOTIMPL (documented in the ITextRange unit remarks for rich edit controls), tomColumn not co...
- row 1737: riched20 textrange_startof: .../window/charformat/paraformat/cell/row/table/object added; tomSection stays E_NOTIMPL (documented in the ITextRange unit remarks for rich edit controls), tomColumn not co...
- row 1738: riched20 textrange_endof: .../window/charformat/paraformat/cell/row/table/object added; tomSection stays E_NOTIMPL (documented in the ITextRange unit remarks for rich edit controls), tomColumn not co...
- row 1739: riched20 textrange_move: .../window/charformat/paraformat/cell/row/table/object added; tomSection stays E_NOTIMPL (documented in the ITextRange unit remarks for rich edit controls), tomColumn not co...
- row 1740: riched20 textrange_movestart: .../window/charformat/paraformat/cell/row/table/object added; tomSection stays E_NOTIMPL (documented in the ITextRange unit remarks for rich edit controls), tomColumn not co...
- row 1741: riched20 textrange_moveend: .../window/charformat/paraformat/cell/row/table/object added; tomSection stays E_NOTIMPL (documented in the ITextRange unit remarks for rich edit controls), tomColumn not co...
- row 1781: shell32 ShellExtInit_Initialize: ...patch 3702 (gate fastb-cmenu, 6 mutants caught); New menu verbs stay E_NOTIMPL (no documented names), help texts are our wording...
- row 1782: shell32 ObjectWithSite_SetSite: ...patch 3702 (gate fastb-cmenu, 6 mutants caught); New menu verbs stay E_NOTIMPL (no documented names), help texts are our wording...
- row 1783: shell32 ObjectWithSite_GetSite: ...patch 3702 (gate fastb-cmenu, 6 mutants caught); New menu verbs stay E_NOTIMPL (no documented names), help texts are our wording...
- row 1784: shell32 context_menu_GetCommandString: ...patch 3702 (gate fastb-cmenu, 6 mutants caught); New menu verbs stay E_NOTIMPL (no documented names), help texts are our wording...
- row 1785: shell32 context_menu_HandleMenuMsg: ...patch 3702 (gate fastb-cmenu, 6 mutants caught); New menu verbs stay E_NOTIMPL (no documented names), help texts are our wording...
- row 1786: shell32 context_menu_HandleMenuMsg2: ...patch 3702 (gate fastb-cmenu, 6 mutants caught); New menu verbs stay E_NOTIMPL (no documented names), help texts are our wording...
- row 1787: shell32 object_with_site_SetSite: ...patch 3702 (gate fastb-cmenu, 6 mutants caught); New menu verbs stay E_NOTIMPL (no documented names), help texts are our wording...
- row 1788: shell32 object_with_site_GetSite: ...patch 3702 (gate fastb-cmenu, 6 mutants caught); New menu verbs stay E_NOTIMPL (no documented names), help texts are our wording...
- row 1789: shell32 BackgroundMenu_GetCommandString: ...patch 3702 (gate fastb-cmenu, 6 mutants caught); New menu verbs stay E_NOTIMPL (no documented names), help texts are our wording...
- row 1790: shell32 zipmenu_GetCommandString: ...patch 3702: validate, verbs, help text; NO gate (needs a zip pidl): unverified beyond the build...
- row 1919: mssign32 SignerSignEx: ...3728: as SignerSign, returns SIGNER_CONTEXT, index; timestamps unsupported (ERROR_NOT_SUPPORTED, guess)...

### triage-6

- row 877: ole32 DefaultHandler_GetDataHere: ...nning object, OLE_E_NOTRUNNING otherwise; cache GetDataHere is itself still a stub)...
- row 908: rpcrt4 RpcAsyncGetCallStatus: ...e.sh 64+32 with widl stubs; two mutants fail it. Server-side async is not supported (see 557/558/587)....

### triage-6b

- row 3: crypt32 CertContext_SetProperty: ...SABLE_CRL, ..., KEY_SPEC) is kept; the ones the page calls 'reserved' stay failing; patch 5301 makes ARCHIVED hide certificates from enumeration (CertOpenStore CERT_STORE...
- row 11: crypt32 CryptSignMessage: ..._CONTENT_OUT_FLAG, SILENT_KEYSET_FLAG (CRYPT_SIGN_MESSAGE_PARA page); still FIXME: ENCAPSULATED_CONTENT_OUT_FLAG and dwInnerContentType (no CMS encapsulation in CryptMsgO...
- row 13: crypt32 CryptVerifyCertificateSignatureEx: ...ail the crypt32 cert test; OCSP subject and strong-property flags are not done (formats undocumented)...
- row 16: bcrypt key_export: ...s, mutants GENERIC_BLOB and GENERIC_IMPORT fail; symmetric keys still unsupported there (the page gives no generic blob for them); LEGACY_DH_* blobs not done (no layout i...
- row 39: windows.security.authentication.web.core async_inspectable_GetIids: .../webcore-inspectable-gate.sh mutant NO_IIDS; re-read in the base: the remaining E_NOTIMPL in async_inspectable_GetRuntimeClassName is only for an IID outside the four res...
- row 40: windows.security.authentication.web.core async_inspectable_GetRuntimeClassName: .../webcore-inspectable-gate.sh mutant NO_IIDS; re-read in the base: the remaining E_NOTIMPL in async_inspectable_GetRuntimeClassName is only for an IID outside the four res...
- row 41: windows.security.authentication.web.core async_inspectable_GetTrustLevel: .../webcore-inspectable-gate.sh mutant NO_IIDS; re-read in the base: the remaining E_NOTIMPL in async_inspectable_GetRuntimeClassName is only for an IID outside the four res...
- row 42: windows.security.authentication.web.core factory_GetIids: .../webcore-inspectable-gate.sh mutant NO_IIDS; re-read in the base: the remaining E_NOTIMPL in async_inspectable_GetRuntimeClassName is only for an IID outside the four res...
- row 43: windows.security.authentication.web.core factory_GetRuntimeClassName: .../webcore-inspectable-gate.sh mutant NO_IIDS; re-read in the base: the remaining E_NOTIMPL in async_inspectable_GetRuntimeClassName is only for an IID outside the four res...
- row 44: windows.security.authentication.web.core factory_GetTrustLevel: .../webcore-inspectable-gate.sh mutant NO_IIDS; re-read in the base: the remaining E_NOTIMPL in async_inspectable_GetRuntimeClassName is only for an IID outside the four res...
- row 45: windows.security.authentication.web.core interop_GetRuntimeClassName: .../webcore-inspectable-gate.sh mutant NO_IIDS; re-read in the base: the remaining E_NOTIMPL in async_inspectable_GetRuntimeClassName is only for an IID outside the four res...

### triage-7

- row 50: fltmgr.sys FltRegisterFilter: ...nter uniquely identifies the minifilter); volume attach and callbacks not done (no filter manager); mutant FLT_FIXED fails...
- row 73: uxtheme HitTestThemeBackground: ...estthemebackground ; the HTTB_* options beyond HTTB_BACKGROUNDSEG are not handled (their page is 404); test_HitTestThemeBackground, mutant HIT_ALWAYS fails...
- row 111: ws2_32 WSAJoinLeaf: ... multipoint (WSA_FLAG_MULTIPOINT_C_LEAF/D_LEAF as multicast joins) is not done; test_WSAJoinLeaf_and_QOS, mutant JOIN_NOCHECK fails...
- row 113: userenv CreateAppContainerProfile: ...le ; test_app_container_profile, mutant AC_NOEXIST fails; folder ACLs not done...
- row 131: comctl32 EDIT_EM_FmtLines: ...n32/controls/em-fmtlines (WM_GETTEXT / WM_GETTEXTLENGTH; EM_GETHANDLE not done); test_EM_FMTLINES, mutant FMT_NOOP fails...
- row 167: qwave QOSAddSocketToFlow: ..., which is not here, so a real handle still gets ERROR_NOT_SUPPORTED (stays a FIXME on purpose)...
- row 210: setx wmain: ...YPE each fail 1; file mode /f /a /r /x /d and remote /s /u /p answer 'not supported' with exit 1 (the coordinates of file mode are not specified well enough to implement)...
- row 283: pnputil wmain: ...-syntax); device enumeration/enable/disable/restart/remove fail with 'not supported'; make test-pnputil 15 checks, mutant NOSUMMARY fails 4...
- row 455: schedsvc parse_element: ...AST fails 2; taskschd's own trigger objects still read them as daily (NOT DONE)...
- row 467: fontsub CreateFontPackage: ...bit, mutant NOCLOSURE fails; package formats and name-language filter not done (documented in the notes)...

### Patch-level: implemented lines whose patch notes admit unfinished work

One row per patch; "n" is the number of implemented lines citing it. The excerpt is the first admission in patches/notes/NNNN.txt. Which of the n lines is the partial one is not recoverable from the tsv.

| file | patch | n | example line | admission in notes |
|---|---|---|---|---|
| fast-a-cloud | 3238 | 3 | spoolss BuildOtherNamesFromMachineName | EVERT/NOLOOP caught. spoolss tests 64/32 ours 0 failures (stock fails the unmarked todo, expected). Left as stubs (undocumented): IsLocalCall, SplIsUpgrade, SpoolerInit, SpoolerHasInitialized, WaitForSpoolerInitialization.  |
| fast-a-cloud | 3248 | 3 | qwave QOSCreateHandle | APTIVEMARK/MULTI/FIRSTONLY caught. qwave tests 64/32 pass (stock fails 5 incl. the unmarked todos). Not done: QOSCancel/Query/Notify/Enumerate/Track* (qWAVE service).  |
| fast-b | 3615 | 105 | dcomp DCompositionCreateDevice3 | evice+context and bumps a generation (drawing surfaces reallocate at next BeginDraw, content lost). Not done: IDCompositionDevice3 effects, CreateSurfaceFromHandle/Hwnd (ESCALATE.md), RenderingDeviceReplaced event. Gate fastb-gapd |
| fast-b | 3619 | 9 | winspool.drv GetPrinterW | r (Windows' exact default differs), the error codes for protected/unknown processors and providers. Not done: AddPrintProvidor (loads a DLL: ESCALATE), UploadPrinterDriverPackage (driver install), PerfOpen/PerfCollect counters ("P |
| fast-b | 3621 | 20 | d2d1 d2d_device_context_EndDraw | ff,0xffffffff}; texture Create with an existing id returns the existing one (as vertex buffers do). Not done: effect invalid/required input rectangles (4 calls), SetVertexProcessing, BlendImage, color/SVG glyph runs and images, Co |
| fast-b | 3622 | 8 | dbghelp SymRegisterFunctionEntryCallback | t); GetLineFromName returns only exact line matches; the index values of a JG PDB (signature, age). Not done: SymQueryInlineTrace when the start context differs, SymGetSourceFileToken(W) (needs the PDB srcsrv stream), MapDebugInfo |
| fast-b | 3623 | 4 | wiaservc wiadevmgr_RegisterEventCallbackProgram | ster, 4 default handler (documented values). GetImageDlg -> WIA_S_NO_DEVICE_AVAILABLE. AddDeviceDlg still E_NOTIMPL (wizard UI). Gate fastb-gapwia (wiaservc conformance test + new test_RegisterEventCallback); mutant SG_MUTANT_WIA_ |
| fast-b | 3625 | 1 | shell32 ApplicationAssociationRegistration_SetAppAsDefault | \<kind>). Unknown type -> E_INVALIDARG. SetAppAsDefaultAll unchanged (docs give no publisher rule). Not done in this patch: QueryAppIsDefault/QueryAppIsDefaultAll/ClearUserAssociations (separate FIXMEs). Gate appdefault-gate.sh (p |
| fast-b | 3626 | 3 | dcomp device_CreateSurfaceFromHandle | IDARG). CreateCompositionSurfaceForHandle wraps the surface in a swapchain_surface (draws nothing). Not done: a producer for the handle (IDXGIFactoryMedia::CreateSwapChainForCompositionSurfaceHandle is not in our dxgi), live windo |
| fast-b | 3627 | 21 | cldapi CfRegisterSyncRoot | -mode surface of cldapi.spec in dlls/cldapi/main.c from Microsoft's cfapi documentation (cfapi.h is not available: structure layouts CF_SYNC_REGISTRATION/POLICIES/CALLBACK_INFO/PARAMETERS/OPERATION_INFO/PARAMETERS/PLACEHOLDER_CREA |
| fast-b | 3628 | 4 | d3d11 d3d11_device_context_Signal | U wait in the caller: not GPU-side). Fence waits are polled, not kernel-signalled across processes. Not done in d3d11 (open in work/gap/open.txt): class linkage/dynamic linking (CreateClassInstance, GetClassInstance, shader SetSha |
| fast-b | 3636 | 9 | dbgeng debugclient_GetKernelConnectionOptions | ymbol failure code ERROR_NOT_FOUND, EXISTING re-attach semantics, CreateClient shares only targets. Not done (escalated in triage): dump files, remote/process servers, kernel attach, Assemble, WdbgExts APIs, Request 7/12.  |
| fast-b | 3637 | 13 | d3drm d3drm_viewport2_Render | odes, mesh builder default colour 0 (Wine) renders as alpha 0 (invisible) until SetColor is called. Not done (no triage written, still open in gap-B): d3drm Clone methods, Save, mesh builder delete/optimize/submesh/enable/AddFrame |
| fast-b | 3640 | 46 | d3dx9_36 D3DXOptimizeFaces | n empty linker, FROMWFILE-unknown -> BADVALUE, RegisterEnumTemplates private interface (Wine only). NOT done (open.txt): keyframed Compress, RenderToEnvMap sphere/hemisphere/parabolic, CreateTeapot, DisassembleEffect, palettized S |
| fast-b | 3641 | 11 | d3dx9_36 d3dx9_keyframed_animation_Compress | e support for the surface format here), D16_LOCKABLE fill stores component x as 16 bit unorm depth. NOT done (open.txt): A1 and the packed formats R8G8_B8G8/G8R8_G8B8/UYVY/YUY2 as conversion sources (documented RGBG layout, YUV ma |
| fast-b | 3643 | 1 | d3dx9_36 d3dx_load_pixels_from_pixels | gapd3dx2 (surface), mutant PACKED_SWAP caught; 64+32 0 failures (i386 texture.c:3042 pre-existing). Not done: D3DFMT_A1 as a conversion source (bit order not documented), packed formats as conversion DESTINATIONS (undocumented enc |
| fast-b | 3644 | 2 | mscoree parse_startup | _POLICY caught. mscoree conformance 64+32 identical to stock (environment has no .NET: tests report not available). GUESSED: ICorConfiguration setters do not enforce 'before runtime start' ordering (unspecified in docs), the polic |
| fast-b | 3645 | 4 | imagehlp GetImageConfigInformation | figInformation does not update the checksum; Set copies min(directory size in file, sizeof struct). NOT done in imagehlp (gap-REST-busy idx 114 BindImageEx modification, 115 ReBaseImage, 116 RemovePrivateCvSymbolic, 118 SplitSymbo |
| fast-b | 3646 | 6 | imagehlp BindImageEx | k (Wine builtin DLLs forward). Gate fastb-gapimagehlp2; 20 mutants caught (BIND_ORD NOUPD IAT STAMP PARTIAL FWDREF ALL GROW, REBASE_RELOC DOWN STAMP SYS CHECK DBG, CV_KEEPALL, SPLIT_NOPRIV FLAG TRUNC, UPD_EX BASE). imagehlp confor |
| fast-b | 3651 | 5 | mstask MSTASK_ITask_EditWorkItem | rding of the property sheet (English only); mstask now imports user32 and loads comctl32 on demand. NOT done: SetAccountInformation password storage (triaged).  |
| fast-b | 3652 | 9 | wintab32 WTMgrPacketHookExA | E with ERROR_INVALID_PARAMETER. Marks compare raw device pressure. WTMgrDeviceConfig (149) triaged: stays a stub.  |
| fast-b | 3658 | 2 | msscript.ocx ActiveScriptSite_GetItemInfo | InitFromData/GetClipboardData/SetColorScheme stay E_NOTIMPL (ATL default), FIXME turned into TRACE. Not done: Timeout event and enforcement of the Timeout property, IPersistStreamInit Load/Save/GetSizeMax (stream format unknown),  |
| fast-b | 3660 | 4 | wbemprox sysrestore_create | in32_SystemRestore instances are still the single placeholder row (enumerating the stored points is not done). ESCALATE: applying a restore point needs a design (map to the Btrfs snapshot rollback work). SpawnDerivedClass (58) nee |
| fast-b | 3661 | 1 | sechost parse_acl | INVALID_ACL (the existing path for malformed ACEs). Callback ACEs (XA XD XU XL) with conditions are not done. Triaged: QueryServiceConfig2 level 9 and TraceSetInformation open; notification filter windows-matches (docs). Gate fast |
| fast-b | 3664 | 1 | wsdapi xml_text_to_wide_string | N: received Resolve/Probe parsing in the publisher (the publisher has no multicast listener yet) is NOT done. CORE review: none.  |
| fast-b | 3669 | 1 | windows.networking.connectivity factory_QueryInterface | rGuid matches nothing because profiles carry none; NetworkCostType compared with GetConnectionCost. NOT DONE: IConnectionProfileFilter2/3 (IsRoaming, IsOverDataLimit, RawData...) not exposed; they need IBuffer/IReference<boolean>  |
| fast-b | 3670 | 2 | geolocation coordinate_QueryInterface | ackToConsentlessPositions is implemented in patch 3671 (fallback decision made by the coordinator). NOT DONE (escalate): GeocoordinateSatelliteData/VenueData classes; the geolocation test for GetIids now expects the 2 interfaces.  |
| fast-b | 3675 | 1 | tdh TdhLoadManifest | d by Microsoft, so a provider binary's manifest is not decoded: ESCALATE.md "tdh binary manifests". Not done either: TdhEnumerateManifestProviderEvents, TdhGetEventMapInformation, TdhFormatProperty, TdhGetProperty* (stubs without  |
| fast-b | 3681 | 1 | wbemprox process_create | gs of CreateProcess, so PriorityClass reaches CreateProcess but its effect cannot be observed here. NOT DONE: wbemdisp (SWbemObject IDispatch) cannot Put/Get a CIM_OBJECT property yet, so the VBScript form objProc.Create(cmd, Null |
| fast-b | 3685 | 28 | shell32 ShellImageData_NextFrame | g/SaveAsCMYK, GIF SaveFlag, none for BMP/PNG) and their value sets are not verified against native. Not done: GetProperties (IPropertySetStorage of the metadata: property ids undocumented), animation delay not exercised by the gat |
| fast-b | 3686 | 12 | shell32 DragSourceHelper2_InitializeFromBitmap |  windows that answer ShellGetDragImage. DAD_SetDragImage/DAD_ShowDragImage (legacy image list drag) not done. winspool Perf* stay stubs: no consumer exists (kernelbase HKEY_PERFORMANCE_DATA loads no performance DLLs) and the count |
| fast-b | 3687 | 18 | shell32 shellfolderviewdual_get_Folder |  name or an empty string; ViewOptions come from SHGetSettings; PopupItemMenu, Script and Parent are not done; FilterView is not done. shell32 tests: shelldispatch.c get_Document todo_wine removed (passes now).  |
| fast-b | 3699 | 2 | riched20 fnTextSrv_TxQueryHitPoint | e point). Unverified: the distance of CLOSE (4 px, no documented value), autoscroll while dragging (not done), CF_HDROP and OLE objects are not taken. CORE review: riched20. Gate fastb-rdrop; mutants HIT_OPAQUE HIT_CLOSE DROP_RO D |
| fast-b | 3704 | 8 | shell32 active_desktop_SetDesktopItemOptions | finds the item by dwID; the position clamp rule; the HTML markup; URL type rules (extension based). Not done: the confirmation/zone/subscription dialogs of AddDesktopItemWithUI/AddUrl (no HTML desktop exists here), the dwID renumb |
| fast-b | 3710 | 5 | userenv CreateAppContainerProfile | CreateLinkFile (no public docs: parameters follow Wine's comment) and which window states it keeps. Not done: the user's hive is not unloaded by UnloadUserProfile (no unload in the server), so open keys do not make it fail; GetApp |
| fast-b | 3711 | 7 | authz AuthzInitializeResourceManager | ck, CachedAccessCheck, FreeHandle, InstallSecurityEventSource (+ new UninstallSecurityEventSource). NOT DONE (AuthzAccessCheck stays escalate in the tsv): (1) request->ObjectTypeList fails with ERROR_NOT_SUPPORTED (no ground truth |
| fast-b | 3715 | 5 | ws2_32 WSAIoctl |  the group while the original is open; WSAEINVAL for a socket made without WSA_FLAG_MULTIPOINT_* is NOT done (flags are not kept); FD_CONNECT is not posted for a non-blocking join. - WSAGetQOSByName: WSAENOTSOCK / WSAEFAULT checks |
| fast-b | 3717 | 5 | windows.security.enterprisedata manager_GetIids | ut. Unknown-IID E_NOINTERFACE and ActivateInstance E_NOTIMPL (static class) are right as they were. NOT done: requests never block or ask for consent (consent/blocked results only matter with a policy, and none can exist here). Ga |
| fast-b | 3718 | 7 | sxs cache_CreateAssemblyCacheItem | mory; Commit lays them out in a private temp dir and runs the same install code as InstallAssembly. Partial names (no arch/token/version) are FUSION_E_INVALID_NAME (documented), a name that does not match the manifest is refused a |
| fast-b | 3722 | 2 | cryptui CryptUIDlgViewSignerInfoA |  My store. A cert in no system store (e.g. from a memory store) shows an empty Location (GUESS).  - Not done (ESCALATE.md): CRL/CTL contexts in CryptUIDlgViewContext, CryptUIWizDigitalSign. Gate fastb-cryptui (opens the real dialo |
| fast-b | 3723 | 4 | amsi AmsiInitialize | sion numbers the sessions per context; AmsiCloseSession tells every provider (CloseSession(id)).  - Not done: AmsiUac* (stubs), notifications (IAntimalwareProvider2). No provider ships with SG yet: wiring SG Defender (ClamAV) in a |
| fast-b | 3728 | 2 | mssign32 SignerSign | le), NOPUT (success without writing), NOKEYCMP (wrong key accepted), FIRSTCERT (wrong certificate). NOT done / guesses: timestamps (SignerTimeStamp*, http_time_stamp -> ERROR_NOT_SUPPORTED, guess), SIGNER_SUBJECT_BLOB (ERROR_NOT_S |
| recheck-fast-a | 3255 | 4 | riched20 ITextSelection_fnGetFlags | ICHED_STYLE RICHED_STYLEDUP caught (work/muts-riched.sh). ShowObject/OnShowWindow are not gated and PARTIAL: they return S_OK, the object is not scrolled into view nor shaded. CORE review: riched20 suite 64/32 = same as without th |
| triage-6 | 5084 | 1 | ntoskrnl.exe KeInsertQueue |  (waits are not alertable); CurrentCount is not maintained (concurrency limiting by MaximumCount is not done, MaximumCount only records the count, default the processor count); one process-wide lock and condition variable serve al |
| triage-6 | 5088 | 1 | ntoskrnl.exe IoReportResourceForDetection | t is read; the claim is kept in memory, not in the registry's ResourceMap; IoReportResourceUsage is still a stub. The descriptors are parsed with the real 4-byte packed 20-byte layout (the CM_PARTIAL_RESOURCE_DESCRIPTOR of Wine's  |
| triage-6 | 5090 | 1 | ntoskrnl.exe IoGetConfigurationInformation | CORE: integrator must review 5090 ntoskrnl: gap-CORE item 1083 (IoGetConfigurationInformation "partial stub"). Docs: learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-iogetconfigurationinformation ("contains the cu |
| triage-6 | 5092 | 1 | ntoskrnl.exe IoGetDeviceInterfaces | r an invalid PDO; freed with ExFreePool). Not as Windows: the "default interface first" ordering is not done (the order is setupapi's); the interfaces come from setupapi's registry view (the HKLM DeviceClasses keys written by IoRe |
| triage-6 | 5096 | 1 | netio.sys wsk_control_socket | indows-hardware/drivers/ddi/wsk/nc-wsk-pfn_wsk_control_socket (no numeric values there). Unverified/not done: the WSK-only options (SO_WSK_EVENT_CALLBACK, SO_WSK_SECURITY, SO_CONDITIONAL_ACCEPT handling) are not known to Winsock a |
| triage-6 | 5098 | 2 | netio.sys wsk_socket_connect | hardware/drivers/ddi/wsk/nc-wsk-pfn_wsk_socket_connect and nc-wsk-pfn_wsk_get_name_info. Unverified/not done: OwningProcess/OwningThread/SecurityDescriptor are not used (the docs tie them to the caller's security context for shari |
| triage-6 | 5107 | 4 | ole32 DefaultHandler_GetDataHere | not made weak. Unverified: the order cache / object for GetDataHere (the cache's own GetDataHere is still a stub that answers E_NOTIMPL, so in practice the object is asked); GetRunningClass of a handler that is not running answers |
| triage-6b | 5303 | 2 | crypt32 CDecodeEnvelopedMsg_GetParam | , SG_MUTANT_MSGPARAM2 (signer hash algorithm). Unverified: the other documented parameters that are still FIXMEs (CMSG_ENCRYPT_PARAM, CMSG_UNPROTECTED_ATTR_PARAM, CMSG_CMS_RECIPIENT_INFO_PARAM, the recipient index parameters after |
| triage-6b | 5305 | 2 | crypt32 CryptSignMessage |  key id signer). Unverified: CRYPT_MESSAGE_ENCAPSULATED_CONTENT_OUT_FLAG and dwInnerContentType are still FIXME (the encoder has no CMSG_CMS_ENCAPSULATED_CONTENT_FLAG support and the documentation does not say which inner OID a CM |
| triage-6b | 5306 | 1 | crypt32 CryptVerifyCertificateSignatureEx | D2_MD4_ALLOWED (the flag ignored). Unverified: the error for a malformed chain (E_INVALIDARG here); not done: CRYPT_VERIFY_CERT_SIGN_SUBJECT_OCSP_BASIC_SIGNED_RESPONSE, SET_STRONG_PROPERTIES and RETURN_STRONG_PROPERTIES (the prope |
| triage-7 | 5500 | 3 | dwmapi DwmRegisterThumbnail | rocess sees them); DwmQueryThumbnailSourceSize when the source is gone (DWM_E_COMPOSITIONDISABLED). NOT DONE: overlapping thumbnails in one destination draw in turn, not composed; a source that is not a top-level window is refused |
| triage-7 | 5502 | 2 | apphelp ShimFlushCache | GetAppPatchDir; whether hSDB (optional) changes the answer; the last error for a non-administrator. NOT DONE here (see docs/stubs/triage-7.tsv and escalate-7.md): SdbCreateDatabase/OpenDatabase/InitDatabase/CloseDatabase/GetFirstC |
| triage-7 | 5503 | 3 | newdev InstallSelectedDriver | late-7.md); the restart flag follows DI_NEEDREBOOT/DI_NEEDRESTART of the device install parameters. NOT DONE: InstallNewDevice (a wizard: escalated), DiInstallDevice / DiUninstallDevice / DiRollbackDriver (spec stubs, not in the q |
| triage-7 | 5504 | 12 | difxapi DriverPackagePreinstallA | fuses a package installed on devices (it does not: setupapi's SetupUninstallOEMInf does not check). NOT DONE: nonpresent devices (CONFIGFLAG_REINSTALL), the Programs and Features entry DIFx adds for a package, DRIVER_PACKAGE_DELET |
| triage-7 | 5505 | 1 | appxpackaging bundle_factory_CreateBundleManifestReader | ond "syntactically invalid XML" (the page says only that); whether the schema namespace is checked. NOT DONE (escalated, see escalate-7.md): IAppxFactory::CreatePackageWriter / CreateBlockMapReader / CreateValidatedBlockMapReader, |
| triage-7 | 5506 | 11 | fltmgr.sys FltInitializePushLock | ); FLT_REGISTRATION Version/Size are not validated (the version value is not in an allowed source). NOT DONE: attaching a registered minifilter to volumes and passing I/O to it; NdisRegisterProtocol (needs an NDIS binding model, e |
| triage-7 | 5507 | 6 | uxtheme IsCompositionActive | e screen; HitTestThemeBackground when both a rectangle and a region are given (we test the region). NOT DONE: HitTestThemeBackground's HTTB_RESIZINGBORDER_*/CAPTION/FIXEDBORDER options (the page listing them is gone, 404; they wou |
| triage-7 | 5508 | 2 | sxs name_IsEqual | Equal and case-insensitive name comparison are not stated; E_INVALIDARG for NULL arguments is ours. NOT DONE (triaged): SetProperty/GetProperty/Finalize/Reserved (pages are signatures only), IAssemblyCache::CreateAssemblyCacheItem |
| triage-7 | 5509 | 6 | iertutil uri_GetIids | gument constructor); IStringable on Uri (not implemented; what ToString returns is not documented). NOT DONE: IStringable for Uri; IUriRuntimeClassFactory2/other Windows 10 Uri interfaces beyond the two named above.  |
| triage-7 | 5511 | 4 | ws2_32 WSACancelAsyncRequest |  none); that an empty template list is a single NUL (the page: "terminated by a zero-length name"). NOT DONE (escalated): SIO_UDP_CONNRESET (no ICMP port-unreachable reporting for UDP), TF_REUSE_SOCKET in DisconnectEx; multipoint  |
| triage-7 | 5512 | 4 | userenv CreateAppContainerProfile | IED comes from registry access denial only; a non-NULL display name or description is not required. NOT DONE: GetAppContainerFolderPath, GetAppContainerRegistryLocation, ACLs on the folder that exclude other users and apps (needs  |
| triage-7 | 5513 | 5 | authz AuthzInitializeResourceManager |  owner; ACEs with callbacks/conditions are not evaluated (copied, treated as unknown by the check). NOT DONE (see triage-7.tsv): AuthzInitializeContextFromSid (needs the SID's groups: an S4U logon the server cannot give), AuthzIns |
| triage-7 | 5515 | 1 | comctl32 EDIT_EM_FmtLines | FMT_NOOP (2). UNVERIFIED: that the EM_GETHANDLE buffer gets the soft breaks (the page says it does; not done: the buffer is the control's own); EN_ERRSPACE when the text no longer fits a limit; user32's Edit class (dlls/user32/edi |
| triage-7 | 5516 | 1 | cryptnet CryptGetObjectUrl | empty, as for the OIDs that already worked); CRYPT_GET_URL_FROM_PROPERTY finds nothing (as before). NOT DONE: the CTL/CRL based OIDs (URL_OID_CTL_ISSUER, _CTL_NEXT_UPDATE, _CRL_ISSUER, _CRL_FRESHEST_CRL) and the cross certificate  |
| triage-7 | 5520 | 2 | setx wmain | loopback interface whose subnet holds the address) follows the page's "first applicable interface". NOT DONE: setx file mode (/f /a /r /x /d: the coordinate system is not specified well enough), remote /s /u /p for both tools; sub |
| triage-7 | 5522 | 1 | pnputil wmain | t exist). 32-bit callers get ERROR_IN_WOW64 from /install (DiInstallDriver's documented behaviour). NOT DONE: /export-driver, /enum-devices and the other device commands (setupapi can enumerate devices; the tool's output is large  |
| triage-7 | 5524 | 3 | wscript Host_GetObject | e engine, shown +1); IActiveScriptSite::OnScriptTerminate's result (its page could not be fetched). NOT DONE: WScript.ConnectObject / DisconnectObject and event prefixes, WScript.Timeout.  |
| triage-7 | 5528 | 1 | schedsvc parse_element | igger is computed in UTC from the start boundary's time of day (a DST change moves the local hour). NOT DONE: taskschd's own object model still reads these triggers as daily (IMonthlyTrigger/IMonthlyDOWTrigger, dlls/taskschd/trigg |
| triage-7 | 5529 | 1 | schedsvc SchRpcRename | ). UNVERIFIED: the HRESULTs for an invalid new name and for the existing target (MS-TSCH's text was not available): ERROR_INVALID_NAME and ERROR_ALREADY_EXISTS.  |
| triage-7 | 5533 | 3 | loadperf LoadPerfCounterTextStringsW | remote computer) and the base of 1846/1847 (Wine's stub list says 1846 is the last built-in index). Not done: the Trusted key's library validation code, event-log messages, remote computers, BackupPerfRegistryToFile etc.  |

## (e) escalate, no proposal

Escalates with no proposal, option, owner or design pointer in the evidence (keyword screen then read), plus escalates that only defer ("not started", "same as", "after fast-a merges"). Security-model escalates that name the reason are not counted.

### fast-a

- row 333: mshtml HTMLWindow4_createPopup: HTMLWindow4_createPopup: needs IHTMLPopup (a popup window object with its own document); not present
- row 335: mshtml HTMLWindow6_put_XDomainRequest: HTMLWindow6_put_XDomainRequest: same override slot as XMLHttpRequest
- row 336: mshtml HTMLWindow6_get_XDomainRequest: HTMLWindow6_get_XDomainRequest: the XDomainRequest object is not implemented in mshtml (new class)
- row 353: mshtml HTMLDocument4_createRenderStyle: HTMLDocument4_createRenderStyle: IHTMLRenderStyle does not exist in mshtml (new interface)
- row 354: mshtml HTMLDocument6_get_compatible: HTMLDocument6_get_compatible: IHTMLDocumentCompatibleInfoCollection does not exist in mshtml (new interfaces)
- row 355: mshtml DisplayServices_CreateDisplayPointer: DisplayServices_CreateDisplayPointer: IDisplayPointer needs layout-based caret positions that are not available
- row 356: mshtml DisplayServices_GetCaret: DisplayServices_GetCaret: ICaret (the visible caret) is not present
- row 357: mshtml DisplayServices_GetComputedStyle: DisplayServices_GetComputedStyle: needs ICSSStyle for a markup pointer position; no such object
- row 363: mshtml MarkupPointer2_MoveUnit: MarkupPointer2_MoveUnit line and paragraph actions need layout boxes of text lines
- row 390: mshtml SVGSVGElement_get_currentView: SVGSVGElement currentView/getIntersectionList/getEnclosureList: need Gecko SVG interfaces (nsIDOMSVGSVGElement parts) that nsiface.idl does not declar
- row 392: mshtml SVGSVGElement_getIntersectionList: SVGSVGElement currentView/getIntersectionList/getEnclosureList: need Gecko SVG interfaces (nsIDOMSVGSVGElement parts) that nsiface.idl does not declar
- row 393: mshtml SVGSVGElement_getEnclosureList: SVGSVGElement currentView/getIntersectionList/getEnclosureList: need Gecko SVG interfaces (nsIDOMSVGSVGElement parts) that nsiface.idl does not declar
- row 408: mshtml HTMLElement2_addFilter: HTMLElement2 addFilter/removeFilter: filters are an IE visual effect engine (documentmode < 10) that is not present
- row 409: mshtml HTMLElement2_removeFilter: HTMLElement2 addFilter/removeFilter: filters are an IE visual effect engine (documentmode < 10) that is not present
- row 418: mshtml DocNodeOleInPlaceObjectWindowless_GetDropTarget: GetDropTarget of the windowless in-place object: OLE drag and drop (IDropTarget) is not wired to Gecko
- row 419: mshtml DocObjOleInPlaceObjectWindowless_GetDropTarget: GetDropTarget of the windowless in-place object: OLE drag and drop (IDropTarget) is not wired to Gecko
- row 423: mshtml HTMLXMLHttpRequest_private_get_upload: HTMLXMLHttpRequest_private_get_upload: XMLHttpRequestUpload is not implemented (new class)
- row 426: mshtml HTMLStyleSheetRule_put_selectorText: HTMLStyleSheetRule_put_selectorText: needs nsIDOMCSSStyleRule, which nsiface.idl does not declare and whose IID could not be verified
- row 427: mshtml HTMLStyleSheetRule_get_style: HTMLStyleSheetRule_get_style: IHTMLRuleStyle (about 190 members) is not implemented in mshtml
- row 569: mshtml HTMLNamespaceCollection_get_length: HTMLNamespaceCollection_get_length returns 0 because no IHTMLNamespace exists (see item/add)
- row 679: webservices write_type: partly implemented in 3236 (DURATION, CUSTOM, XML_BUFFER); WS_TIMESPAN_TYPE text form undocumented and WS_ENDPOINT_ADDRESS_TYPE (large, docs give fiel
- row 681: webservices read_type: partly implemented in 3236 (DURATION, CUSTOM, XML_BUFFER); WS_TIMESPAN_TYPE text form undocumented and WS_ENDPOINT_ADDRESS_TYPE (large, docs give fiel

### fast-b

- row 324: wined3d wined3d_texture_load_drawable: depth/stencil resources loaded into DRAWABLE/renderbuffer locations: GL renderer internals needing a GPU-verified blit path
- row 325: wined3d wined3d_texture_load_renderbuffer: depth/stencil resources loaded into DRAWABLE/renderbuffer locations: GL renderer internals needing a GPU-verified blit path
- row 327: wined3d wined3d_device_apply_stateblock: D3DRS_ANTIALIAS (ddraw edge antialiasing, advertised in ddraw caps) needs GL_POLYGON_SMOOTH blend path; visual result unverifiable here
- row 328: wined3d shader_generate_code: SM2 instruction predication (setp + predicated instructions, caps advertised in directx.c:2586/2630) needs per-component predicated writes in the GLSL
- row 392: opengl32 wow64_ext_glCreateSyncFromCLeventARB: ARB_cl_event needs OpenCL context/event handles of the host; no interop exists for a 32-bit client. Patch 3635 withholds GL_ARB_cl_event from wow64 cl
- row 393: opengl32 wow64_ext_glGetVkProcAddrNV: NV_draw_vulkan_image returns a host Vulkan proc address (64-bit); patch 3635 withholds GL_NV_draw_vulkan_image from wow64 clients
- row 394: opengl32 wow64_ext_glImportSyncEXT: EXT_x11_sync_object imports an X11 fence (Linux-only, not part of a Windows driver surface); patch 3635 withholds GL_EXT_x11_sync_object from wow64 cl
- row 785: dnsapi DnsReplaceRecordSetUTF8: DnsReplaceRecordSetUTF8: same as DnsReplaceRecordSetA
- row 786: dnsapi DnsReplaceRecordSetW: DnsReplaceRecordSetW: same as DnsReplaceRecordSetA
- row 790: mscms GetPS2ColorSpaceArray: GetPS2ColorSpaceArray: PostScript CSA from an ICC profile; same PostScript generation work as GetPS2ColorRenderingDictionary; returns FALSE today
- row 811: ddraw ddraw_surface_create: Flippable textures (DDSCAPS_TEXTURE with FLIP): ddraw1.c:5357 shows Windows creates them (some versions bluescreen so the test skips); needs per-surfa
- row 1028: wbemdisp sink_cpc_EnumConnectionPoints: the SWbemSink code exists only in fast-a patch 3093-wbemdisp-sink-async (origin/fast-a), absent from main and fast-b (grep sink dlls/wbemdisp/*.c is e
- row 1029: wbemdisp sink_cp_EnumConnections: same: fast-a 3093 holds the sink; not in this tree
- row 1031: wbemdisp object_Invoke: PROPERTYPUTREF on SWbemObject belongs to the dispatch code of fast-a 3093 (not in this tree); main's locator.c has no such branch
- row 1032: rometadata metadata_open_memory: this tree's dlls/rometadata has no metadata reader at all (main.c: OpenScope/OpenScopeOnMemory/DefineScope are FIXME E_NOTIMPL stubs, no metadata.c); 
- row 1206: shell32 ExtractAssociatedIconExW: ExtractAssociatedIconExW: documented; needs icon extraction by association (GetBinaryType/association icon index plumbing in iconcache.c); not done in
- row 1294: riched20 ITextRange_fnGetFormattedText: ITextRange/ITextSelection Get/SetFormattedText: documented copy of formatted text between ranges; needs RTF/internal run copying between ranges
- row 1295: riched20 ITextRange_fnSetFormattedText: ITextRange/ITextSelection Get/SetFormattedText: documented copy of formatted text between ranges; needs RTF/internal run copying between ranges
- row 1296: riched20 ITextSelection_fnGetFormattedText: ITextRange/ITextSelection Get/SetFormattedText: documented copy of formatted text between ranges; needs RTF/internal run copying between ranges
- row 1297: riched20 ITextSelection_fnSetFormattedText: ITextRange/ITextSelection Get/SetFormattedText: documented copy of formatted text between ranges; needs RTF/internal run copying between ranges
- row 1361: riched20 ITextSelection_fnMoveLeft: ITextSelection MoveLeft/Right (units other than char/word) and MoveUp/Down (tomLine): need row layout; word/char done
- row 1362: riched20 ITextSelection_fnMoveRight: ITextSelection MoveLeft/Right (units other than char/word) and MoveUp/Down (tomLine): need row layout; word/char done
- row 1363: riched20 ITextSelection_fnMoveUp: ITextSelection MoveLeft/Right (units other than char/word) and MoveUp/Down (tomLine): need row layout; word/char done
- row 1364: riched20 ITextSelection_fnMoveDown: ITextSelection MoveLeft/Right (units other than char/word) and MoveUp/Down (tomLine): need row layout; word/char done
- row 1467: d3d11 d3d11_device_OpenSharedResource1: OpenSharedResource1/ByName/OpenSharedResource: shared resources need cross-device/process resource backing in wined3d (none exists, same wall as D3D9E
- row 1468: d3d11 d3d11_device_OpenSharedResourceByName: OpenSharedResource1/ByName/OpenSharedResource: shared resources need cross-device/process resource backing in wined3d (none exists, same wall as D3D9E
- row 1469: d3d11 d3d10_device_OpenSharedResource: OpenSharedResource1/ByName/OpenSharedResource: shared resources need cross-device/process resource backing in wined3d (none exists, same wall as D3D9E
- row 1487: gdiplus metafile_free_object_table_entry: EMF+ playback completeness (StringFormat and CustomLineCap objects, DrawString records, pen transform/dash, GdipPlayMetafileRecord for all record type
- row 1488: gdiplus METAFILE_PlaybackObject: EMF+ playback completeness (StringFormat and CustomLineCap objects, DrawString records, pen transform/dash, GdipPlayMetafileRecord for all record type
- row 1489: gdiplus GdipPlayMetafileRecord: EMF+ playback completeness (StringFormat and CustomLineCap objects, DrawString records, pen transform/dash, GdipPlayMetafileRecord for all record type
- row 1490: gdiplus METAFILE_DrawImagePointsRect: EMF+ playback completeness (StringFormat and CustomLineCap objects, DrawString records, pen transform/dash, GdipPlayMetafileRecord for all record type
- row 1505: windowscodecs ImagingFactory_CreateFastMetadataEncoderFromDecoder: IWICFastMetadataEncoder (from decoder / frame decode): documented in-place metadata rewrite using padding; needs decoders that report metadata block p
- row 1506: windowscodecs ImagingFactory_CreateFastMetadataEncoderFromFrameDecode: IWICFastMetadataEncoder (from decoder / frame decode): documented in-place metadata rewrite using padding; needs decoders that report metadata block p
- row 1518: dwrite freetype_get_aa_glyph_bitmap: colour glyph image formats (SVG, COLR-less bitmaps, PNG/JPEG/TIFF strikes) in freetype_get_aa_glyph_bitmap: needs sbix/CBDT/SVG table readers and a ra
- row 1527: d2d1 d2d_draw_info_SetVertexProcessing: ID2D1DrawInfo::SetVertexProcessing: custom effect vertex shaders (D3D vertex pipeline for effects) not supported by the effect renderer
- row 1541: uiautomationcore msaa_provider_GetPatternProvider: MSAA provider patterns other than LegacyIAccessible: the documented MSAA-to-UIA mapping (Value, Toggle, Invoke, Selection, ... from IAccessible roles/
- row 1567: uiautomationcore node_fragment_SetFocus: UIA node fragment SetFocus: needs forwarding to the provider's SetFocus plus window focus handling: not started
- row 1568: uiautomationcore call_pattern_method: call_pattern_method / create_uia_pattern for patterns without a client implementation: each pattern needs its method table (Wine implements a subset):
- row 1569: uiautomationcore create_uia_pattern: call_pattern_method / create_uia_pattern for patterns without a client implementation: each pattern needs its method table (Wine implements a subset):
- row 1571: uiautomationcore UiaGetPropertyValue: UiaGetPropertyValue (element array) / UiaGetUpdatedCache (cache scopes): FIXME paths for unsupported property types and scopes: not started
- row 1572: uiautomationcore UiaGetUpdatedCache: UiaGetPropertyValue (element array) / UiaGetUpdatedCache (cache scopes): FIXME paths for unsupported property types and scopes: not started
- row 1573: mprapi MprAdminEstablishDomainRasServer: mprapi.c is rewritten by fast-a's unmerged patches 3027-mprapi-infoblocks-config (origin/fast-a); editing it here would conflict: re-check these RRAS 
- row 1574: mprapi MprAdminGetPDCServer: mprapi.c is rewritten by fast-a's unmerged patches 3027-mprapi-infoblocks-config (origin/fast-a); editing it here would conflict: re-check these RRAS 
- row 1575: mprapi MprAdminInterfaceGetCredentials: mprapi.c is rewritten by fast-a's unmerged patches 3027-mprapi-infoblocks-config (origin/fast-a); editing it here would conflict: re-check these RRAS 
- row 1576: mprapi MprAdminInterfaceSetCredentials: mprapi.c is rewritten by fast-a's unmerged patches 3027-mprapi-infoblocks-config (origin/fast-a); editing it here would conflict: re-check these RRAS 
- row 1577: mprapi MprAdminIsDomainRasServer: mprapi.c is rewritten by fast-a's unmerged patches 3027-mprapi-infoblocks-config (origin/fast-a); editing it here would conflict: re-check these RRAS 
- row 1578: mprapi MprAdminUserGetInfo: mprapi.c is rewritten by fast-a's unmerged patches 3027-mprapi-infoblocks-config (origin/fast-a); editing it here would conflict: re-check these RRAS 
- row 1579: mprapi MprAdminUserSetInfo: mprapi.c is rewritten by fast-a's unmerged patches 3027-mprapi-infoblocks-config (origin/fast-a); editing it here would conflict: re-check these RRAS 
- row 1580: mprapi MprConfigServerInstall: mprapi.c is rewritten by fast-a's unmerged patches 3027-mprapi-infoblocks-config (origin/fast-a); editing it here would conflict: re-check these RRAS 
- row 1581: mprapi MprSetupIpInIpInterfaceFriendlyNameCreate: mprapi.c is rewritten by fast-a's unmerged patches 3027-mprapi-infoblocks-config (origin/fast-a); editing it here would conflict: re-check these RRAS 
- row 1582: mprapi MprSetupIpInIpInterfaceFriendlyNameDelete: mprapi.c is rewritten by fast-a's unmerged patches 3027-mprapi-infoblocks-config (origin/fast-a); editing it here would conflict: re-check these RRAS 
- row 1583: mprapi MprSetupIpInIpInterfaceFriendlyNameEnum: mprapi.c is rewritten by fast-a's unmerged patches 3027-mprapi-infoblocks-config (origin/fast-a); editing it here would conflict: re-check these RRAS 
- row 1584: scrobj global_GetNameSpaceParent: scrobj.c is rewritten by fast-a's unmerged patches 3080-scrobj-components-typelib and 3096-scrobj-events (origin/fast-a); these E_NOTIMPL members (Get
- row 1585: scrobj ActiveScriptSite_GetDocVersionString: scrobj.c is rewritten by fast-a's unmerged patches 3080-scrobj-components-typelib and 3096-scrobj-events (origin/fast-a); these E_NOTIMPL members (Get
- row 1586: scrobj ActiveScriptSite_OnStateChange: scrobj.c is rewritten by fast-a's unmerged patches 3080-scrobj-components-typelib and 3096-scrobj-events (origin/fast-a); these E_NOTIMPL members (Get
- row 1587: scrobj scriptlet_GetNameSpaceParent: scrobj.c is rewritten by fast-a's unmerged patches 3080-scrobj-components-typelib and 3096-scrobj-events (origin/fast-a); these E_NOTIMPL members (Get
- row 1588: scrobj instance_cpc_EnumConnectionPoints: scrobj.c is rewritten by fast-a's unmerged patches 3080-scrobj-components-typelib and 3096-scrobj-events (origin/fast-a); these E_NOTIMPL members (Get
- row 1589: scrobj instance_cp_EnumConnections: scrobj.c is rewritten by fast-a's unmerged patches 3080-scrobj-components-typelib and 3096-scrobj-events (origin/fast-a); these E_NOTIMPL members (Get
- row 1590: scrobj parse_scriptlet_script: scrobj.c is rewritten by fast-a's unmerged patches 3080-scrobj-components-typelib and 3096-scrobj-events (origin/fast-a); these E_NOTIMPL members (Get
- row 1591: scrobj parse_scriptlet_registration: scrobj.c is rewritten by fast-a's unmerged patches 3080-scrobj-components-typelib and 3096-scrobj-events (origin/fast-a); these E_NOTIMPL members (Get
- row 1592: scrobj parse_scriptlet_public: scrobj.c is rewritten by fast-a's unmerged patches 3080-scrobj-components-typelib and 3096-scrobj-events (origin/fast-a); these E_NOTIMPL members (Get
- row 1593: scrobj parse_scriptlet_file: scrobj.c is rewritten by fast-a's unmerged patches 3080-scrobj-components-typelib and 3096-scrobj-events (origin/fast-a); these E_NOTIMPL members (Get
- row 1735: shell32 FolderView2_SetGroupSubsetCount: GAP: patch 3694 only stores the count. Documented behaviour (LVM_SETGROUPSUBSETCOUNT / LVGS_SUBSETED: a group shows only N rows plus a "show all" link
- row 1028,1206,1304-1350,1470,1481-1482,1487-1490,1493,1505-1506,1508,1516,1518,1541,1544-1546,1555-1556,1567-1569,1571-1593,1595: Marked escalate but says "not started"/"same as ..."/"after fast-a merges"/"handed to drafter 6": it asks nobody for a decision and gives no way forward.

### triage-5

- row 56: msvcrt _heapmin: _heapmin returns -1 because HeapCompact returns 0 on Wine: ntdll RtlCompactHeap is a stub returning 0 (dlls/ntdll/heap.c:2278). Docs (https://learn.mi
- row 89: msvcm80 CrtImplementationDetails_ThrowModuleLoadException_inner: msvcm80 ThrowModuleLoadException(System.String^, System.Exception^): managed exception; needs CLR integration (policy)
- row 90: msvcm80 CrtImplementationDetails_ThrowNestedModuleLoadException: msvcm80 ThrowNestedModuleLoadException(System.Exception^, System.Exception^): managed exception; needs CLR integration (policy)
- row 99: dplaysvr wmain: dplaysvr.exe is the DirectPlay session/enumeration server (shared port forwarding between DirectPlay apps); Wine stub just logs args. Implementing is 
- row 151: qcap fnCaptureGraphBuilder2_RenderStream: RenderStream doc (https://learn.microsoft.com/en-us/windows/win32/api/strmif/nf-strmif-icapturegraphbuilder2-renderstream): for PIN_CATEGORY_VBI 'The 
- row 259: wmvcore WMWriter_BeginWriting: BeginWriting needs WMV/WMA encoders plus an ASF muxer/sink for the profile's streams (writer.c:723-725); a whole encoding pipeline, no tests of real w
- row 260: msvcp90 locale__Locimp__Makeushloc: patch 4576 adds the missing facets for msvcp80/90 (and time_get<unsigned short> everywhere); in msvcp100+ the FIXME stays because those DLLs have no m
- row 261: msvcp90 locale__Locimp__Makewloc: patch 4576 completes _Makewloc for msvcp80/90; msvcp100+ lack the messages/money facets entirely (see item 275)
- row 262: msvcp90 locale__Locimp__Makexloc: patch 4576 completes _Makexloc for msvcp80/90; msvcp100+ lack the messages/money facets entirely (see item 275)
- row 302: msacm32 acmFilterChooseA: acmFilterChoose: dialog box for choosing a waveform-audio filter (msacm.h); Wine has acmFormatChoose dialog (format.c:292) but no filter dialog resour
- row 303: msacm32 acmFilterChooseW: Same as acmFilterChooseA (W variant): needs filter-chooser dialog; UI work, no tests.
- row 313: msvfw32 GetFileNamePreview: GetOpenFileNamePreview doc (https://learn.microsoft.com/en-us/windows/win32/api/vfw/nf-vfw-getopenfilenamepreviewa): 'The dialog box also allows the u
- row 447: winmm midiConnect/midiDisconnect, MMIO_GETTEMP, global IO procs: midiConnect needs MIDI thru support in the drivers (MODM/MIDM thru messages), which Wine's MIDI drivers lack; MMIO_GETTEMP and global IO procedure lis
- row 455: windows.media.mediacontrol VideoProperties/ImageProperties/CopyFromFileAsync (agent note 2): same as line 3: needs full IDL definitions and Windows.Storage metadata
- row 456: windows.media.devices default device change events (agent note 3): same as line 4
- row 476: several "Not done" list (engines, dialogs, undocumented signatures): each listed item that is in gap-D5 has its own triage line (DXVAHD_CreateDevice, acmFilterChoose, mciavi32 Configure, avifil32 clipboard/Delete, msvfw
- row 478: msvcp140 std::filesystem conformance failures: caused by ntdll symbolic link handling (line 99); handed to drafter 6 (core) and fixed there
- row 484: ntdll symbolic link relative target and directory link removal: core DLL: handed to drafter 6 and fixed in stubs-6
- row 492: msvcp100+ messages/money facets for _Make*loc (round 4): msvcp100 and later have no messages, money_get, money_put or moneypunct facets; implementing them for those layouts is a separate piece of work
- row 494: msvcrt _heapmin (round 4): root cause ntdll RtlCompactHeap (core DLL), handed to drafter 6
- row 499: dmstyle chord map methods (round 4): needs 'prrf' parsing and loader resolution; return codes unverifiable here
- row 72,77,303,387-389,454-456,478,484,494: Marked escalate but says "not started"/"same as ..."/"after fast-a merges"/"handed to drafter 6": it asks nobody for a decision and gives no way forward.

### triage-6

- row 65: advapi32 OpenEncryptedFileRawA: EFS raw (b). See 94; A -> convert and call W.
- row 71: advapi32 GetThreadWaitChain: WCT (b). See 99: GetThreadWaitChain needs wineserver wait-object/owner introspection; until then ERROR_NOT_SUPPORTED is the honest answer (no fake cha
- row 103: advapi32 GetNamedSecurityInfoExA: ACLs (provider API). A form of 132: convert strings, call W.
- row 105: advapi32 LookupSecurityDescriptorPartsA: ACLs. A form of 141: convert TRUSTEE/EXPLICIT_ACCESS W->A on output.
- row 223: kernelbase GetConsoleAliasesLengthA: https://learn.microsoft.com/en-us/windows/console/getconsolealiaseslength: 'Returns the size, in bytes, of the buffer needed to store all of the conso
- row 398: rpcrt4 RpcErrorLoadErrorInfo: (b) https://learn.microsoft.com/en-us/windows/win32/api/rpcasync/nf-rpcasync-rpcerrorloaderrorinfo: 'Successful completion returns RPC_S_OK.' A blob s
- row 400: rpcrt4 RpcMgmtSetCancelTimeout: (b) https://learn.microsoft.com/en-us/windows/win32/api/rpcdce/nf-rpcdce-rpcmgmtsetcanceltimeout: 'sets the lower bound on the time to wait before tim
- row 405: rpcrt4 NdrAsyncServerCall: (b) NDR [pipe] support absent in NdrAsyncServerCall (ndr_stubless.c:2038-2042 raises RPC_X_WRONG_STUB_VERSION). Part of the same pipes subsystem (asyn
- row 412: secur32 lsa_DeleteLogonSession: LSA logon sessions. DeleteLogonSession: remove LUID from the table of 617, STATUS_NO_SUCH_LOGON_SESSION if unknown.
- row 413: secur32 lsa_AddCredential: LSA credentials (package dispatch). AddCredential: store (package_id, primary_key, credentials) per logon session in LSA-private memory (never visible
- row 414: secur32 lsa_GetCredentials: LSA credentials. GetCredentials: enumerate entries of 619 for the session/package with context/retrieve_all semantics; STATUS_NOT_FOUND when none.
- row 415: secur32 lsa_DeleteCredential: LSA credentials. DeleteCredential: delete entry of 619 by primary key; STATUS_NOT_FOUND if absent.
- row 482: win32u win32u_vkDestroySwapchainKHR: vulkan.c:389 ignores pAllocator; same as idx 705. Low priority.
- row 539: wtsapi32 WTSStartRemoteControlSessionW: Sessions (shadow). Same as 787 for W.
- row 540: wtsapi32 WTSStopRemoteControlSession: Sessions (shadow). WTSStopRemoteControlSession: no shadow can be active -> FALSE with ERROR_CTX_WINSTATION_NOT_FOUND for unknown ids / ERROR_NOT_SUPPO
- row 544: wtsapi32 WTSVirtualChannelPurgeInput: RDP virtual channels (b). No handle can exist: ERROR_INVALID_HANDLE, FALSE.
- row 545: wtsapi32 WTSVirtualChannelPurgeOutput: RDP virtual channels (b). Same as 793.
- row 546: wtsapi32 WTSVirtualChannelQuery: RDP virtual channels (b). Same as 793 (WTSVirtualFileHandle query on an invalid handle).
- row 547: wtsapi32 WTSVirtualChannelRead: RDP virtual channels (b). Same as 793.
- row 548: wtsapi32 WTSVirtualChannelWrite: RDP virtual channels (b). Same as 793.
- row 551: advapi32 GetAuditedPermissionsFromAclA: Audit/ACLs. A -> convert trustee and call W (801).
- row 777: ntoskrnl.exe PsRemoveCreateThreadNotifyRoutine: (b) Tied to 1122. https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/nf-ntddk-psremovecreatethreadnotifyroutine: 'returns STATUS_SUC
- row 787: ntoskrnl.exe ProbeForWrite: ProbeForWrite: as ProbeForRead (item 1146): same checks, same gating problem.
- row 750,912: Marked escalate but says "not started"/"same as ..."/"after fast-a merges"/"handed to drafter 6": it asks nobody for a decision and gives no way forward.

### triage-6c

- row 6: user32 WINNLSGetEnableStatus: PARTIAL: patch 5213 makes WINNLSGetEnableStatus return the state WINNLSEnableIME left; the IME-window effect of enabling/disabling is not done (see WI

### triage-7

- row 66: hal READ_PORT_ULONG: as READ_PORT_UCHAR
- row 67: hal WRITE_PORT_UCHAR: as READ_PORT_UCHAR
- row 68: hal WRITE_PORT_ULONG: as READ_PORT_UCHAR
- row 139: cryptnet CRYPT_CreatePKCS7: several PKCS #7 messages come only from a multi-result retrieval (LDAP) or a third-party scheme provider; the code is a collection store of each messa
- row 165: ncrypt NCryptExportKey: the NCryptBufferDesc parameters carry the PKCS #8 encryption choice (NCRYPTBUFFER_PKCS_ALG_OID, _ALG_PARAM, _SECRET, _KEY_NAME); exporting an encrypte
- row 176: sensapi IsDestinationReachableW: same as IsDestinationReachableA
- row 180: websocket WebSocketCreateClientHandle: the WebSocket protocol API is one state machine (handle, handshake begin/end, action queue, send/receive with masking and fragmentation); the other te
- row 181: websocket WebSocketAbortHandle: as WebSocketCreateClientHandle: nothing creates a handle, so there is nothing to abort
- row 182: websocket WebSocketDeleteHandle: as WebSocketCreateClientHandle
- row 190: t2embed TTLoadEmbeddedFont: embedded OpenType (EOT) loading: parse the EOT header, MicroType Express decompression, XOR obfuscation, then AddFontMemResourceEx; the EOT/MTX format
- row 191: t2embed TTEmbedFont: needs font subsetting and MTX compression (see TTLoadEmbeddedFont)
- row 192: t2embed TTDeleteEmbeddedFont: counterpart of TTLoadEmbeddedFont (RemoveFontMemResourceEx), nothing is loaded so nothing can be deleted; goes with it
- row 194: winscard SCardReleaseStartedEvent: https://learn.microsoft.com/en-us/windows/win32/api/winscard/nf-winscard-scardreleasestartedevent : decrements the reference count of the event SCardA
- row 206: cabinet FDITruncateCabinet: https://learn.microsoft.com/en-us/windows/win32/api/fdi/nf-fdi-fditruncatecabinet FDITruncateCabinet removes folders from the end of a cabinet and rew
- row 248: netsh wmain: the final stub is deliberate (0079): a command outside the implemented contexts (interface ip, wlan, advfirewall, firewall) is accepted quietly so ins
- row 253: regasm wmain: registers a .NET assembly's COM-visible types in the registry by reflecting on the assembly; native code cannot read assembly metadata, so it needs Wi
- row 261: dpvsetup wmain: the DirectPlay Voice setup wizard (audio device selection and a loopback test); a wizard on top of dpvoice, which is a stub library
- row 270: cdosys DllGetClassObject: CDO for Windows 2000 is a COM library (CDO.Message, CDO.Configuration, body parts, MIME writing, SMTP/pickup/NNTP sending) behind ProgIDs scripts use;
- row 303: wscript EventSink_GetIDsOfNames: same event sink as EventSink_GetTypeInfo
- row 316: windows.security.enterprisedata manager_GetIids: IInspectable::GetIids of the ProtectionPolicyManager statics-instance; the same work as patch 5525 but this DLL has no test module and adding one need
- row 318: windows.security.enterprisedata context_GetIids: as manager_GetIids
- row 320: windows.security.enterprisedata factory_GetIids: as manager_GetIids
- row 321: windows.security.enterprisedata factory_GetRuntimeClassName: as manager_GetIids
- row 350: winex11.drv X11DRV_GravityNotify: same as ConfigureNotify (shares the mapping)
- row 360: dpnhupnp DirectPlayNATHelpCreate: the UPnP NAT helper: SSDP discovery and IGD port mapping for DirectPlay8; a feature with no consumer here (see dpnhpast)
- row 361: dxtrans DllGetClassObject: DirectX Transform factory/transforms (CLSID_DXTransformFactory, filters, gradients) are a COM library; CLASS_E_CLASSNOTAVAILABLE is the honest answer 
- row 362: windowscodecsext DllGetClassObject: WIC extension codecs (HEIF/WebP/raw handlers) are Microsoft Store packages; CLASS_E_CLASSNOTAVAILABLE is right; nothing to implement without a codec
- row 363: connect IsInternetConnected: the Internet Connection Wizard helper (connect.dll) asks whether the machine is online; S_OK is faked; an honest answer shares sensapi's open question
- row 374: mgmtapi SnmpMgrOpen: https://learn.microsoft.com/en-us/windows/win32/api/mgmtapi/nf-mgmtapi-snmpmgropen : an SNMP manager session to a host (UDP 161 requests with communit
- row 377: traffic TcDeregisterClient: counterpart of TcRegisterClient; part of the same Traffic Control feature
- row 387: printui PrintUIEntryW: the rundll32 command line of printui.dll (/in install printer, /dl delete, /ia add driver, /ge etc.: https://learn.microsoft.com/en-us/windows-server/
- row 388: unidrv core_DrvGetGPDData: IPrintOemDriverUni::DrvGetGPDData returns GPD (Generic Printer Description) data for an OEM plug-in; this driver core is sg's and parses PPD/GPD into 
- row 397: wineps.drv get_glyf_pos: OpenType fonts with CFF outlines (no 'loca' table) are not converted to Type 1/42 downloads; the CFF charstring format is Adobe's, not in the allowed 
- row 410: cryptxml CryptXmlOpenToDecode: https://learn.microsoft.com/en-us/windows/win32/api/cryptxml/nf-cryptxml-cryptxmlopentodecode : XML Digital Signatures (XMLDSIG): needs exclusive cano
- row 411: cryptxml CryptXmlGetStatus: https://learn.microsoft.com/en-us/windows/win32/api/cryptxml/nf-cryptxml-cryptxmlgetstatus : XML Digital Signatures (XMLDSIG): needs exclusive canonic
- row 412: cryptxml CryptXmlVerifySignature: https://learn.microsoft.com/en-us/windows/win32/api/cryptxml/nf-cryptxml-cryptxmlverifysignature : XML Digital Signatures (XMLDSIG): needs exclusive c
- row 413: cryptxml CryptXmlGetDocContext: https://learn.microsoft.com/en-us/windows/win32/api/cryptxml/nf-cryptxml-cryptxmlgetdoccontext : XML Digital Signatures (XMLDSIG): needs exclusive can
- row 414: cryptxml CryptXmlGetSignature: https://learn.microsoft.com/en-us/windows/win32/api/cryptxml/nf-cryptxml-cryptxmlgetsignature : XML Digital Signatures (XMLDSIG): needs exclusive cano
- row 415: cryptxml CryptXmlGetReference: https://learn.microsoft.com/en-us/windows/win32/api/cryptxml/nf-cryptxml-cryptxmlgetreference : XML Digital Signatures (XMLDSIG): needs exclusive cano
- row 429: cryptui CryptUIWizDigitalSign: https://learn.microsoft.com/en-us/windows/win32/api/cryptuiapi/nf-cryptuiapi-cryptuiwizdigitalsign : the Digital Signature Wizard over mssign32's sign
- row 433: cryptext CryptExtAddPFXW: wide form of CryptExtAddPFX
- row 66-68,176,181-182,252,318,320-321,350: Marked escalate but says "not started"/"same as ..."/"after fast-a merges"/"handed to drafter 6": it asks nobody for a decision and gives no way forward.

## Cross-file conflicts (same dll+function, different verdicts, excluding the fast-a family)

119 functions. Format: dll fn: file(row)=verdict.

- amsi AmsiInitialize: fast-b(1902)=implemented; triage-7(171)=windows-matches
- amsi AmsiOpenSession: fast-b(1903)=implemented; triage-7(172)=windows-matches
- amsi AmsiScanBuffer: fast-b(1904)=implemented; triage-7(173)=windows-matches
- amsi AmsiScanString: fast-b(1905)=implemented; triage-7(174)=windows-matches
- apphelp SdbGetPermLayerKeys: fast-b(1908)=implemented; triage-7(18)=no-ground-truth
- apphelp SdbSetPermLayerKeys: fast-b(1907)=implemented; triage-7(17)=no-ground-truth
- apphelp SetPermLayerState: fast-b(1906)=implemented; triage-7(16)=no-ground-truth
- apphelp ShimFlushCache: fast-b(1912)=no-ground-truth; fast-b(1925)=implemented; triage-7(220)=implemented
- appwiz.cpl InstallCallback_OnLowResource: recheck-fast-a(67)=windows-matches; triage-7(280)=no-ground-truth
- authz AuthzAccessCheck: fast-b(1829)=escalate; fast-b(1923)=implemented; triage-7(119)=implemented
- authz AuthzInitializeContextFromSid: fast-b(1825)=implemented; triage-7(121)=escalate
- authz AuthzInstallSecurityEventSource: fast-b(1828)=implemented; triage-7(124)=escalate
- bcrypt key_asymmetric_verify: triage-6(562)=no-ground-truth; triage-6b(57)=implemented
- comctl32 PAGER_Create: fast-b(1891)=implemented; triage-7(132)=no-ground-truth
- cryptui CryptUIDlgViewContext: fast-b(1900)=escalate; triage-7(426)=windows-matches
- cryptui CryptUIDlgViewSignerInfoA: fast-b(1898)=implemented; triage-7(427)=escalate
- cryptui add_cert_to_list: fast-b(1899)=implemented; triage-7(428)=no-ground-truth
- dinput other top-level HID usages; MOUSE_VIRTUAL_DESKTOP (2878): recheck-fast-a(69)=windows-matches; triage-5(467)=no-ground-truth
- dxgi dxgi_factory_CreateSwapChainForCoreWindow: fast-b(1888)=implemented; triage-7(128)=escalate
- dxgi dxgi_factory_GetSharedResourceAdapterLuid: fast-b(1889)=implemented; triage-7(129)=no-ground-truth
- dxgi dxgi_output_GetDisplayModeList: fast-b(1886)=implemented; triage-7(126)=no-ground-truth
- dxgi dxgi_output_GetDisplayModeList1: fast-b(1887)=implemented; triage-7(127)=no-ground-truth
- dxgi dxgi_surface_GetDC: fast-b(1885)=implemented; triage-7(125)=windows-matches
- gdi32 PlayEnhMetaFileRecord: triage-6(842)=implemented; triage-6c(9)=escalate
- graphicscapture factory_QueryInterface: fast-b(976)=no-ground-truth; recheck-fast-a(70)=windows-matches
- hal HalGetBusDataByOffset: recheck-fast-a(71)=windows-matches; triage-7(70)=no-ground-truth
- hhctrl.ocx Site_GetMoniker: fast-b(986)=no-ground-truth; recheck-fast-a(72)=windows-matches
- hhctrl.ocx Site_RequestNewObjectLayout: fast-b(987)=no-ground-truth; recheck-fast-a(73)=windows-matches
- hlink IDataObject_fnSetData: fast-b(766)=no-ground-truth; recheck-fast-a(74)=windows-matches
- iccvid ICCVID_DriverProc: recheck-fast-a(75)=windows-matches; triage-7(382)=no-ground-truth
- itss ITSProtocol_Resume: fast-b(867)=no-ground-truth; recheck-fast-a(77)=windows-matches
- itss ITSProtocol_Suspend: fast-b(866)=no-ground-truth; recheck-fast-a(78)=windows-matches
- localspl fpAddPort: fast-b(924)=no-ground-truth; recheck-fast-a(79)=windows-matches
- localspl fpAddPortEx: fast-b(925)=no-ground-truth; recheck-fast-a(80)=windows-matches
- localspl fpConfigurePort: fast-b(926)=no-ground-truth; recheck-fast-a(81)=windows-matches
- localspl fpDeletePort: fast-b(927)=no-ground-truth; recheck-fast-a(82)=windows-matches
- msasn1 ASN1_CloseDecoder: fast-a-cloud(109)=implemented; triage-7(419)=no-ground-truth
- msasn1 ASN1_CloseEncoder: fast-a-cloud(108)=implemented; triage-7(417)=no-ground-truth
- mscms GenerateCopyFilePaths: fast-b(788)=no-ground-truth; recheck-fast-a(83)=windows-matches
- mscms SpoolerCopyFileEvent: fast-b(791)=no-ground-truth; recheck-fast-a(84)=windows-matches
- msctfmonitor InitLocalMsCtfMonitor: recheck-fast-a(85)=windows-matches; triage-7(287)=no-ground-truth
- msrle32 MSRLE32_DriverProc: recheck-fast-a(94)=windows-matches; triage-7(379)=no-ground-truth
- msscript.ocx OleObject_GetClipboardData: fast-b(823)=no-ground-truth; recheck-fast-a(95)=windows-matches
- msscript.ocx OleObject_InitFromData: fast-b(822)=no-ground-truth; recheck-fast-a(96)=windows-matches
- msscript.ocx OleObject_SetColorScheme: fast-b(824)=no-ground-truth; recheck-fast-a(97)=windows-matches
- mssign32 PvkFreeCryptProv: fast-b(1922)=no-ground-truth; triage-7(425)=escalate
- mssign32 PvkGetCryptProv: fast-b(1920)=no-ground-truth; triage-7(423)=escalate
- mssign32 PvkPrivateKeyAcquireContextFromMemory: fast-b(1921)=no-ground-truth; triage-7(424)=escalate
- mssign32 SignerSign: fast-b(1918)=implemented; triage-7(421)=escalate
- mssign32 SignerSignEx: fast-b(1919)=implemented; triage-7(422)=escalate
- newdev InstallNewDevice: fast-a-cloud(62)=no-ground-truth; triage-7(24)=escalate
- newdev InstallSelectedDriver: fast-a-cloud(63)=no-ground-truth; triage-7(222)=implemented
- ntdll NtOpenKeyEx: recheck-fast-a(98)=windows-matches; triage-6(28)=no-ground-truth
- ntdll RtlQueryHeapInformation: recheck-fast-a(99)=windows-matches; triage-6(247)=no-ground-truth
- ntdll RtlQueryInformationActivationContext: recheck-fast-a(100)=windows-matches; triage-6(273)=no-ground-truth
- ntoskrnl.exe MmQuerySystemSize: recheck-fast-a(101)=windows-matches; triage-6(780)=no-ground-truth
- ole32 snapshot_SetData: recheck-fast-a(102)=windows-matches; triage-6(591)=no-ground-truth
- powrprof IsAdminOverrideActive: fast-a-cloud(68)=windows-matches; triage-7(146)=no-ground-truth
- riched20 DataObjectImpl_EnumFormatEtc: fast-b(1254)=no-ground-truth; recheck-fast-a(103)=windows-matches
- riched20 DataObjectImpl_SetData: fast-b(1253)=no-ground-truth; recheck-fast-a(104)=windows-matches
- riched20 IOleClientSite_fnOnShowWindow: fast-b(1280)=no-ground-truth; recheck-fast-a(126)=implemented
- riched20 IOleClientSite_fnShowObject: fast-b(1279)=no-ground-truth; recheck-fast-a(125)=implemented
- riched20 ITextSelection_fnGetFlags: fast-b(1300)=no-ground-truth; recheck-fast-a(119)=implemented
- riched20 ITextSelection_fnSetFlags: fast-b(1301)=no-ground-truth; recheck-fast-a(120)=implemented
- riched20 TextFont_GetStyle: fast-b(1302)=no-ground-truth; recheck-fast-a(121)=implemented
- riched20 TextFont_SetStyle: fast-b(1303)=no-ground-truth; recheck-fast-a(122)=implemented
- sfc_os SfcIsFileProtected: fast-a-cloud(85)=escalate; triage-7(183)=windows-matches
- sfc_os SfcIsKeyProtected: fast-a-cloud(86)=escalate; triage-7(184)=windows-matches
- sppc SLGetSLIDList: fast-a-cloud(67)=no-ground-truth; triage-7(155)=implemented
- sxs cache_CreateAssemblyCacheItem: fast-b(1876)=implemented; triage-7(86)=escalate
- sxs cache_UninstallAssembly: fast-b(1877)=implemented; triage-7(88)=escalate
- sxs name_Finalize: fast-b(1880)=implemented; triage-7(84)=no-ground-truth
- sxs name_GetProperty: fast-b(1879)=implemented; triage-7(83)=no-ground-truth
- sxs name_SetProperty: fast-b(1878)=implemented; triage-7(82)=no-ground-truth
- tbs Tbsi_Context_Create: fast-a-cloud(1)=windows-matches; triage-7(140)=implemented
- tbs Tbsi_GetDeviceInfo: fast-a-cloud(2)=windows-matches; triage-7(141)=implemented
- url InetIsOffline: fast-a-cloud(99)=implemented; triage-7(197)=no-ground-truth
- user32 DrawAnimatedRects: recheck-fast-a(127)=NOT-windows-matches; triage-6(438)=no-ground-truth
- user32 WINNLSEnableIME: triage-6(431)=no-ground-truth; triage-6c(4)=escalate
- user32 WINNLSGetEnableStatus: triage-6(432)=no-ground-truth; triage-6c(6)=escalate
- userenv USERENV_138: fast-b(1820)=implemented; triage-7(117)=no-ground-truth
- virtdisk DetachVirtualDisk: fast-a-cloud(104)=escalate; triage-7(179)=windows-matches
- virtdisk GetStorageDependencyInformation: fast-a-cloud(102)=no-ground-truth; triage-7(177)=windows-matches
- wimgapi WIMGetMountedImages: fast-a-cloud(106)=no-ground-truth; triage-7(443)=escalate
- wimgapi WIMRegisterMessageCallback: fast-a-cloud(105)=no-ground-truth; triage-7(442)=escalate
- win32u NtGdiDdDDIEscape: triage-6(448)=no-ground-truth; triage-6c(5)=implemented
- windows.networking SetSocketMediaStreamingMode: fast-a-cloud(69)=escalate; recheck-fast-a(108)=windows-matches; triage-7(147)=no-ground-truth
- windows.security.authentication.onlineid DllGetClassObject: recheck-fast-a(109)=windows-matches; triage-6(500)=no-ground-truth
- windows.security.authentication.web.core DllGetClassObject: recheck-fast-a(110)=windows-matches; triage-6(512)=no-ground-truth
- windows.security.credentials.ui.userconsentverifier user_consent_verifier_statics_RequestVerificationAsync: fast-b(1864)=implemented; triage-7(313)=escalate
- windows.security.enterprisedata context_GetIids: fast-b(1869)=implemented; triage-7(318)=escalate
- windows.security.enterprisedata factory_ActivateInstance: fast-b(1875)=windows-matches; triage-7(322)=escalate
- windows.security.enterprisedata factory_GetIids: fast-b(1871)=implemented; triage-7(320)=escalate
- windows.security.enterprisedata factory_GetRuntimeClassName: fast-b(1872)=implemented; triage-7(321)=escalate
- windows.security.enterprisedata manager_GetIids: fast-b(1867)=implemented; triage-7(316)=escalate
- windows.security.enterprisedata no_async: fast-b(1873)=implemented; triage-7(323)=escalate
- windowscodecs copypixels_to_32bppBGRA: fast-b(1503)=no-ground-truth; recheck-fast-a(112)=windows-matches
- wined3d wined3d_output_get_raster_status: fast-b(310)=no-ground-truth; recheck-fast-a(113)=windows-matches
- winemapi MAPIAddress: fast-a-cloud(50)=escalate; triage-7(291)=windows-matches
- winemapi MAPIDeleteMail: fast-a-cloud(46)=implemented; triage-7(294)=escalate
- winemapi MAPIDetails: fast-a-cloud(51)=escalate; triage-7(292)=windows-matches
- winemapi MAPIFindNext: fast-a-cloud(47)=implemented; triage-7(295)=escalate
- winemapi MAPIReadMail: fast-a-cloud(48)=implemented; triage-7(296)=escalate
- winemapi MAPISaveMail: fast-a-cloud(49)=implemented; triage-7(293)=windows-matches
- wineps.drv PSDRV_ExtEscape: fast-a-cloud(52)=windows-matches; triage-7(396)=escalate
- wineps.drv ext_escape: fast-a-cloud(53)=windows-matches; triage-7(398)=not-applicable
- winex11.drv X11DRV_ActivateKeyboardLayout: fast-a-cloud(31)=implemented; triage-7(352)=escalate
- winex11.drv xic_preedit_caret: fast-a-cloud(28)=implemented; fast-a-cloud(70)=escalate; triage-7(348)=no-ground-truth
- wpcap pcap_findalldevs_ex: fast-a-cloud(91)=escalate; triage-7(438)=no-ground-truth
- wpcap pcap_open: fast-a-cloud(90)=escalate; triage-7(439)=no-ground-truth
- wpcap pcap_parsesrcstr: fast-a-cloud(89)=implemented; triage-7(440)=no-ground-truth
- ws2_32 WS2_DisconnectEx: fast-b(1853)=implemented; triage-7(110)=escalate
- ws2_32 WSAGetQOSByName: fast-b(1855)=no-ground-truth; triage-7(112)=implemented
- ws2_32 WSAIoctl: fast-b(1849)=implemented; triage-7(106)=escalate
- ws2_32 WSAJoinLeaf: fast-b(1854)=escalate; fast-b(1924)=implemented; triage-7(111)=implemented
- wscript EventSink_GetIDsOfNames: fast-a-cloud(35)=implemented; triage-7(303)=escalate
- wscript EventSink_GetTypeInfo: fast-a-cloud(34)=implemented; triage-7(302)=escalate
- wscript Host_Echo: fast-a-cloud(37)=not-applicable; triage-7(304)=no-ground-truth
- wscript Host_Quit: fast-a-cloud(39)=no-ground-truth; triage-7(301)=windows-matches
