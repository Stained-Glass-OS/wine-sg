
## Round 3 (verdict fixes for triage-6.tsv)
121 corrected lines appended to docs/stubs/triage-6.tsv (the last line per dll+function wins), each with evidence of its own: windows-matches only with a test line or a fetched docs statement; callable code no longer marked not-applicable; bookkeeping lines replaced by real verdicts; escalations carry a proposal. Items that should become code carry the interim verdict implement-pending; the code batches (patches 5600-5799, branches stubs-7c / stubs-6d) replace those lines with implemented plus the patch number.
Source of the research: the audit docs/stubs/TRIAGE-AUDIT.md rows for this file.
