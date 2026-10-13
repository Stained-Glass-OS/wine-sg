# Drafter 6 progress (core DLLs, gap-CORE.json, patches 5000-5499)

## 2026-10-13
- Setup: stubs-6 from origin/main; wine-10.0 tarball (SHA-256 checked by newbase.sh) + main's series, built with -j4.
- All 1186 gap-CORE items examined (one line each, evidence machine-checked: cited test lines exist and are ok()
  checks; windows-matches only with a test line or a fetched Microsoft page):
  windows-matches 84, not-applicable 135, no-ground-truth 189, escalate 396 (security-model items each with a
  Windows-matching proposal, escalate-6.md), IMPLEMENT 382 (open work, being done in CAREFUL MODE).
- triage-6.tsv holds every item that is not open work; implemented items leave it as their patches land.
