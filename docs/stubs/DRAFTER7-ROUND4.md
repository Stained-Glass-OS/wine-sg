# Drafter 7, round 4 (cloud session): verdict fixes, then phase-2 features

Rounds 1 and 2 are merged into the coordinator's plan; thank you. Round 3
(DRAFTER7-ROUND3.md) has not been started -- do it first, then the phase-2
groups below. Setup, rules, BATCH MODE, CAREFUL MODE for core DLLs: as in
DRAFTER7-CLOUD.md, DRAFTER6-REMOTE.md and RULES.md. Read DECISIONS-2.md and
ESCALATIONS.md (the group descriptions, proposals and examples).

## Part 1 -- round 3 as written (DRAFTER7-ROUND3.md)
Fix the weak verdicts in triage-7, triage-5, triage-6 and triage-6b
(TRIAGE-AUDIT.md lists the rows). Code on stubs-7c / stubs-6d (patches
5600-5799), verdict fixes on stubs-5f / stubs-6f. Note: the local drafters
have since landed more on stubs-6 (up to 5124 plus 3310-3314) and stubs-6c;
fetch before you branch, and do not touch rpcrt4 (local drafter's).

## Part 2 -- phase-2 feature groups (DECISIONS-2 section C)
Local drafters own: G-SHARED, RPC-NDR, NET-META/NET-HOST, video decode,
K-CALLBACKS, and the registry security fix. Do NOT touch those. Yours, in
this order, one group per branch, patch numbers 5800-5999:
1. UIA (UI Automation core: patterns, proxy factories, nested providers,
   text events) -- branch stubs-7d. Screen readers depend on it.
2. SH-DIALOGS + SH-PROPSTORE (PickIconDlg, SHFormatDrive -- format only a
   file-backed image in tests, never a real device --, multi-file
   Properties, NetworkAddressControl; property stores and IFileOperation
   property application) -- branch stubs-7e.
3. RE-TOM + RE-OLE + COM-OLELINK (RichEdit TOM units/formatted text/TOM2,
   OLE embedding and in-place, OLE linking) -- branch stubs-7f. Fast B
   already did parts (3731 TOM2 accessors, 3698/3699 units, hit point,
   drop target): fetch origin/fast-b and build on its riched20 state; note
   "applies after fast-b NNNN" where needed.
4. XML-PARSER (MSXML SOM schema objects and DOMDocumentType entities;
   IXMLParser stays won't-do) and MSH-LEGACY limited to control range,
   markup pointer unit moves and display services -- branch stubs-7g.
   fast-a owns mshtml/msxml3 history: build on origin/fast-a's state.
5. G-D2D, G-DWRITE (rest: colour glyph formats, WOFF2), G-WIC (fast
   metadata encoder, CreateImageEncoder, TIFF planar) -- branch stubs-7h.
6. G-WINRTCOMP (Windows.UI.Composition) -- branch stubs-7i, last (large).

For every group: implement what the docs define; triage the rest one item
at a time (no bulk); full conformance suites of each touched DLL 64/32
against the series-applied base (origin/main + the branch you build on);
one gate per batch with mutants shown failing; notes list guesses. Verdict
lines go to docs/stubs/triage-7r4.tsv in each branch (same columns).
Progress in docs/stubs/progress-7.md ("Round 4"). Push after every batch.
Never push to main, never send email, do not edit debian/changelog.
