# Drafter 7 progress

(Round 1 is on branch claude/wine-sg-stub-drafter-7-cbbvkg; this file on stubs-6b carries only the round-2 section for task 1.)

## Round 2

Task 1, branch stubs-6b (from origin/stubs-6): patches 5300-5308, notes in patches/notes/, verdicts in docs/stubs/triage-6b.tsv, details in docs/stubs/progress-6b.md.
Implemented: crypt32 context properties, archived certificates, localized names, message parameters and signer types, CryptSignMessage, certificate signature issuers; bcrypt generic and DSA blobs.
Left as no-ground-truth with the reason in the tsv: 25, 211, 147, 148, 175, 190, 191, 760, 768, 769.

## Round 3 (verdict fixes for triage-6b.tsv)
47 corrected lines appended to docs/stubs/triage-6b.tsv (the last line per dll+function wins), each with evidence of its own: windows-matches only with a test line or a fetched docs statement; callable code no longer marked not-applicable; bookkeeping lines replaced by real verdicts; escalations carry a proposal. Items that should become code carry the interim verdict implement-pending; the code batches (patches 5600-5799, branches stubs-7c / stubs-6d) replace those lines with implemented plus the patch number.
Source of the research: the audit docs/stubs/TRIAGE-AUDIT.md rows for this file.
