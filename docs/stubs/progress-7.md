# Drafter 7 progress

(Round 1 is on branch claude/wine-sg-stub-drafter-7-cbbvkg; this file on stubs-6b carries only the round-2 section for task 1.)

## Round 2

Task 1, branch stubs-6b (from origin/stubs-6): patches 5300-5308, notes in patches/notes/, verdicts in docs/stubs/triage-6b.tsv, details in docs/stubs/progress-6b.md.
Implemented: crypt32 context properties, archived certificates, localized names, message parameters and signer types, CryptSignMessage, certificate signature issuers; bcrypt generic and DSA blobs.
Left as no-ground-truth with the reason in the tsv: 25, 211, 147, 148, 175, 190, 191, 760, 768, 769.

## Round 3 (verdict fixes for triage-6b.tsv)
47 corrected lines appended to docs/stubs/triage-6b.tsv (the last line per dll+function wins), each with evidence of its own: windows-matches only with a test line or a fetched docs statement; callable code no longer marked not-applicable; bookkeeping lines replaced by real verdicts; escalations carry a proposal. Items that should become code carry the interim verdict implement-pending; the code batches (patches 5600-5799, branches stubs-7c / stubs-6d) replace those lines with implemented plus the patch number.
Source of the research: the audit docs/stubs/TRIAGE-AUDIT.md rows for this file.

## Round 3 code, lane C1
Branch stubs-6d, patches 5600-5699 (this lane), notes in patches/notes/NNNN.txt, verdicts in docs/stubs/triage-7r3.tsv (also /var/tmp/d7/r3/final-c1.tsv).
Done so far: 5619 crypt32 decoders; 5600 msvcrt _heapmin; 5601 ntdll processor performance fallback; 5602 ntdll/server transactions; 5603 advapi32 audit/lookup/EFS; 5604 ole32 GetDataHere; 5605 ntoskrnl ProbeForWrite; 5606 crypt32 key handle properties; 5607 cryptnet PKCS7 arrays; 5608 winscard started event; 5609 bcrypt sign flags; 5610 secur32 LSA sessions; 5611 cabinet FDITruncateCabinet; 5612 crypt32 CMS encapsulated content; 5613 cryptext PFX import; 5614 uxtheme HitTestThemeBackground; 5615 user32 EM_FMTLINES; 5616 bcrypt legacy DH blobs; 5617 bcrypt GCM chaining; 5618 crypt32 OCSP verify and strong properties.
Blocked (docs/stubs/blocked-c1.tsv): userenv AppContainer items and the comctl32 half of EM_FMTLINES need stubs-7b 5512 / 5515.
Still open: console aliases, enveloped message parameters, crypt32 decoders/encoders, NCryptExportKey PKCS#8, cryptxml.
