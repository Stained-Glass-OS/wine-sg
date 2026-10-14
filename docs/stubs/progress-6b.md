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

## Batch 2: patches 5303-5306 (crypt32 messages and signatures)
- 5303/5304 message parameters and signer types (186, 187, 188, 189).
- 5305 CryptSignMessage flags, key-id signers (168).
- 5306 CryptVerifyCertificateSignatureEx issuer types and hash-only signatures (39).

## Batch 3: patches 5307-5308 (bcrypt)
- 5307 generic BCRYPT_PUBLIC_KEY_BLOB / BCRYPT_PRIVATE_KEY_BLOB (813).
- 5308 DSA private v1 blob and the V2 blobs, SHA-224/256 hashes and 64-byte signatures for keys over 1024 bits (817, 823).
  Full bcrypt suite 23164 tests, 0 failures, 64- and 32-bit; mutants DSA_V2 and DSA_PRIVATE_EXPORT fail the new test.
  Clean-tree build of bcrypt (series on a fresh tarball): CLEAN-OK.

## Triage of the rest
Every candidate index has a row in triage-6b.tsv (checked: no candidate is missing, none extra).
- Already implemented by drafter 6 (re-read in the base, cited by patch): cryptowinrt 1000-1014 (5057), onlineid 722-739, 911-916 (5059), web.core 741-745, 757-759, 920 (5060).
- No ground truth, left failing on purpose: 25 and 211 (struct-to-DER mapping of PKCS_PRIVATE_KEY_INFO and friends undocumented), 147 (GCM chained calls), 148 (other sign flags), 175, 190, 191, 760, 768, 769.
- Windows-matches: 204, 745.
No security-model item was found among these candidates, so there is no escalation file for 6b.
