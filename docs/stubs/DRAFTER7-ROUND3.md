# Drafter 7, round 3 (cloud session): fix the weak verdicts the audit found

Rounds 1 and 2 are done; thank you. A local agent audited every triage file
(docs/stubs/TRIAGE-AUDIT.md, read it first). This round fixes the lines in
YOUR files and in the files of the drafters who are no longer running.
Setup, rules, CAREFUL MODE for core DLLs: as in DRAFTER7-CLOUD.md,
DRAFTER6-REMOTE.md and RULES.md.

## Files you own this round
- triage-7.tsv (your round-1 branch): 21 weak windows-matches, 14
  not-applicable-but-callable, 10 implemented-but-partial, 42 escalate
  without a proposal (row numbers in TRIAGE-AUDIT.md).
- triage-5.tsv (origin/stubs-5; drafter 5 is finished): 47 cluster lines,
  18 not-applicable-but-callable, 27 escalate without a proposal, and about
  25 "resolved by patch / not an API gap / decisions recorded" lines that are
  not verdicts.
- triage-6.tsv (origin/stubs-6, drafter 6 crashed): 52 cluster lines, 38
  not-applicable-but-callable, 25 escalate without a proposal.
- triage-6b.tsv (your stubs-6b): 30 cluster lines, 11 implemented-but-partial.

## What to do with each suspicious line
- windows-matches without a specific Wine test line or docs statement for
  THAT function (or whose own text says UNVERIFIED / page not fetchable /
  404): fetch the evidence (MicrosoftDocs sdk-api, win32, winrt-api,
  windows-driver-docs on GitHub) or change the verdict.
- not-applicable for something a Windows program can call on our system
  (examples in the audit: d3dcompiler_43 D3DCreateLinker/D3DLoadModule,
  wnaspi32, dism/ngen/aspnet_regiis/servicemodelreg/certutil tools, gdi32
  Eng* after 5225): implement it when documented, else no-ground-truth with
  the reason.
- "resolved by patch N" style lines: replace with the real verdict
  (implemented citing the patch, or windows-matches with evidence).
- implemented but partial: finish it, or change to escalate stating the
  missing part. A partial item is not implemented.
- escalate without a proposal: add a concrete Windows-matching proposal (one
  paragraph: what to build, where, what it depends on), or implement it when
  it turns out small.
- pasted clusters: give each line its own evidence; split out members that
  are easy and implement them.
- Unknown-IID QueryInterface branches (coordinator's policy): not-applicable
  with the standard text is fine, EXCEPT for WinRT objects, which must answer
  IAgileObject (and IWeakReferenceSource where the class documents it):
  implement those.

## Branches
- Code: stubs-7c from your round-1 branch for DLLs you touched in round 1;
  stubs-6d from origin/stubs-6b for core DLLs. Patch numbers 5600-5799.
- Verdict fixes: append corrected lines to the SAME tsv in a fix branch
  (stubs-5f from origin/stubs-5, stubs-6f from origin/stubs-6 -- do not push
  to stubs-5/stubs-6 themselves); the last line per dll+function wins.
- Progress: docs/stubs/progress-7.md "Round 3" section. Push after every
  batch. Never push to main. Do not edit debian/changelog.
