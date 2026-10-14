# Drafter 7, round 2, task 1: stubs-6b (crypto/WinRT rest of gap-CORE, patches 5300-5499)

Branch stubs-6b is cut from origin/stubs-6 (drafter 6's patches 5000-5078). Careful mode (docs/stubs/DRAFTER6-REMOTE.md).
Candidates are the gap-CORE indexes (0-based) of DRAFTER7-ROUND2.md; verdicts are in docs/stubs/triage-6b.tsv (last column = index).
Conformance: crypt32 / bcrypt / wintrust / cryptowinrt / onlineid suites, 64- and 32-bit, one run per test name, compared with the
same suites on the series-applied base (stubs-6 without these patches): zero new failures. The base has 7 failures in crypt32
(cert.c 2 and chain.c 3 per architecture, all revocation/network: 80092013 CRYPT_E_REVOCATION_OFFLINE); the same ones remain.

## Batch 1: patches 5300-5302 (crypt32)
- 5300 documented context properties of certificates, CRLs and CTLs; user range for CRL/CTL (169, 174, 181).
- 5301 enumerations skip archived certificates (CertOpenStore CERT_STORE_ENUM_ARCHIVED_FLAG), searches do not (follows 174).
- 5302 CryptFindLocalizedName finds the registered names (182).
