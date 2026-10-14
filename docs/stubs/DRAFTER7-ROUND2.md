# Drafter 7, round 2 (cloud session)

Round 1 (gap-CLOUD, patches 5500-5534) is done. Thank you. Setup, batch mode,
gates, lint and the hard rules are unchanged: read DRAFTER7-CLOUD.md and
RULES.md again first. This round has two tasks, in order.

## Task 1: the crypto/WinRT rest of gap-CORE (code)
Drafter 6 (branch stubs-6, patches 5000-5078) crashed before finishing
docs/stubs/gap-CORE.json. A local drafter now continues stubs-6 for the
system DLLs. YOU take these DLLs only: crypt32, bcrypt, wintrust,
cryptowinrt, windows.security.authentication.onlineid,
windows.security.authentication.web.core.

- Branch: stubs-6b, created from origin/stubs-6 (it builds on drafter 6's
  patches). Patch numbers 5300-5499. Read DRAFTER6-REMOTE.md (in
  origin/stubs-6): CAREFUL MODE applies (every program loads these DLLs). That
  means small patches, a gate per patch with mutants shown failing, the FULL
  conformance suites of each touched DLL 64/32 against the series-applied
  base with zero new failures, and notes files starting with "CORE:
  integrator must review".
- Candidate gap-CORE indexes, parsed roughly from progress-6.md. Several are
  probably already implemented (for example 722-739, 911-916, 1000-1014 in
  patches 5057-5060), so check progress-6.md and the source first:
  25 39 147 148 168 169 174 175 181 182 186-191 204 211 722-724 727-729
  737-739 741-743 745 757-760 768 769 813 817 823 911-916 920 1000 1001
  1003-1005 1012-1014
- Verdicts: docs/stubs/triage-6b.tsv in your branch, the same columns as
  triage-6.tsv. Implemented lines cite the patch. Every candidate ends
  implemented or triaged with its own evidence.
- Security model items (tokens, credentials, logon, crypto policy, trust
  decisions): DECISIONS.md item 9 applies (escalate with a concrete
  Windows-matching proposal). Trust and signature checks need mutants that
  wrongly ACCEPT.

## Task 2: second look at "no-ground-truth" verdicts (report, no code)
David wants maximum compatibility with Windows software. A no-ground-truth
verdict is only right when neither Microsoft's docs nor a Wine test says what
Windows does. Re-check every no-ground-truth line in:
- docs/stubs/triage-local/fast-a.tsv, fast-b.tsv (several lines per item:
  the LAST line for a dll+func wins), fast-a-cloud.tsv
- triage-5.tsv (origin/stubs-5), triage-6.tsv (origin/stubs-6),
  triage-7.tsv (your round-1 branch)

For each one, search:
- the MicrosoftDocs repositories on GitHub (sdk-api, win32, winrt-api,
  windows-driver-docs, cpp-docs, openspecs-windows protocol docs).
- Wine's tests in the series-applied tree.
- mingw-w64 / windows-rs, for the signature only.

Write docs/stubs/ngt-recheck.tsv in branch stubs-7b (from your round-1
branch), one line per item: queue file, dll, func, NEW verdict
(still-no-ground-truth | documented | tested | windows-matches), and the
evidence URL or test line plus one sentence on what Windows does. Do not
write code for task 2. The coordinator assigns the "documented" ones to an
owner, because other drafters' branches touch these DLLs. Push every few
hundred lines.

## Both tasks
- Progress: docs/stubs/progress-7.md in each branch, with a "Round 2"
  section.
- Push often. Never push to main. Never send email. Clean room as before.
  Do not edit debian/changelog.
