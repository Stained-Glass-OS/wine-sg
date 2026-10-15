# Fast B on stubs-6c: gdi32, user32, win32u (leftover of drafter 6's CORE queue, patches 5200-5299)

Queue: the 48 open candidate items of gap-CORE.json for gdi32 (26), user32 (12), win32u (10). Method as in
DRAFTER6-REMOTE.md (CAREFUL MODE): a table-driven gate per patch (test/*-gate.sh, 64 and 32 bit, mutants shown
failing), the full gdi32/user32/win32u/usp10 suites 64/32 against the series-applied base (zero new failures),
notes (patches/notes/NNNN.txt) starting "CORE: integrator must review". Items not implemented are in
triage-6c.tsv (one line each); implemented ones are listed here.

Already done on stubs-6 before this branch: 40 (EMR_SMALLTEXTOUT, 5021; more record types below), 702 (5046).

## 2026-10-14, batch 1: patches 5200-5207
- 5200 gdi32 GetAspectRatioFilterEx (224), GdiEntry13 / DdQueryDisplaySettingsUniqueness (227, new NtUserCallNoParam code).
- 5201 uniscribe ScriptApplyLogicalWidth (972), ScriptJustify (973).
- 5202 GetCharacterPlacementW lpClass (223); wingdi.h GCPCLASS_* now the Windows values.
- 5203 colour spaces and per-DC ICM state: CreateColorSpaceA/W (216, 217), DeleteColorSpace (218), GetColorSpace (219),
  SetColorSpace (220), GetLogColorSpaceA/W (228, 229), SetICMProfileA/W (230, 231), SetICMMode; EMF playback of
  EMR_CREATECOLORSPACEW and EMR_SETICMPROFILEA/W (part of 40). New GDI object type + 4 syscalls (win32syscalls.h regenerated).
- 5204 NtGdiRemoveFontMemResourceEx (718).
- 5205 GetWindowDpiHostingBehavior (624).
- 5206 message box Help button (633). 5207 DDE conversation lists (634, 635, 636).
- Triage: 234 getGivenTabWidth no-ground-truth, 995 NtUserGetSystemDpiForProcess escalate (see triage-6c.tsv, escalate-6c.md).

Suites (gdi32 win32u user32 usp10, 64 and 32, vs the unmodified series-applied base): no new failures after 5200-5205; 5206/5207 user32 run recorded below.
(Base failures are environment ones on Xvfb: user32 input 67, monitor 35, sysparams 8, gdi32 dc 3 / font 6 / metafile 2, usp10 2.)

## 2026-10-14, batch 2: patches 5208-5214
- 5208 user32 RegisterTouchHitTestingWindow (651): validation and record; WM_TOUCHHITTESTING itself is not sent -> PARTIAL, escalated (not counted implemented).
- 5209 user32 SetDisplayConfig (659): SDC_* flag validation, source modes through ChangeDisplaySettingsEx, topology requests. CORE review: display (changes real modes; gate on Xvfb only).
- 5210 user32/sechost device events for ANSI windows (656): DBT_DEVTYP_PORT W to A, string-less structures pass through; sechost empty-filter over-read fixed.
- 5211 user32 DdeQueryConvInfo on partner handles (662); DDEML no longer dereferences foreign conversation handles.
- 5212 win32u MNS_AUTODISMISS (713).
- 5213 user32 WINNLSEnableIME / WINNLSGetEnableStatus (645, 646): state only, IME-window effect missing -> PARTIAL, escalated (not counted implemented).
- 5214 win32u D3DKMTEscape (669).
Suites (user32 win32u, 64 and 32 vs the series-applied base): no new failures (user32 143 base failures on Xvfb, win32u 0).

- 5215 win32u FlashWindowEx timers (47). 5216 win32u/wow64win accessibility SPI sets (66).

## 2026-10-14, batch 3: patches 5215-5221
- 5215 win32u FlashWindowEx timers (47). 5216 win32u/wow64win accessibility SPI sets (66).
- 5217 win32u ActivateKeyboardLayout aliased layouts (699) + WM_INPUTLANGCHANGE charset for UTF-8 locales.
- 5218 win32u ETO_NUMERICSLOCAL / ETO_NUMERICSLATIN (717).
- 5219 win32u wglShareLists for software OpenGL (720).
- 5220 win32u D3DKMTQueryAdapterInfo for 5 types (65): PARTIAL, escalated (other types need hardware KMD data).
- 5221 user32 XDnD data object GetDataHere / GetCanonicalFormatEtc (627, 628).
Suites (gdi32 user32 win32u, 64 and 32, vs the series-applied base): no new failures.

## Still open (see /var/tmp/fast-b/s6c-STATE.md)
gdi32: 40 (remaining EMR types: GLS, DRAWESCAPE, EXTESCAPE, NAMEDESCAPE, STARTDOC, COLORCORRECTPALETTE, SETLINKEDUFI, COLORMATCHTOTARGETW), 232/233 UpdateICMRegKey, 237-240 + 838 OpenType (GSUB/GPOS formats, alternate glyphs, feature filtering), 938/939/965 umpd Eng*.
user32: 627/628 clipboard data object (needs an XDnD gate), 651 RegisterTouchHitTestingWindow, 656 devnotify W->A, 659 SetDisplayConfig, 662 DdeQueryConvInfo remote.
win32u: 47 FlashWindowEx timers, 65 QueryAdapterInfo types, 66 SPI accessibility sets, 699 aliased keyboard layout, 713 MNS_AUTODISMISS, 717 ETO_NUMERICS*, 720 wglShareLists.
