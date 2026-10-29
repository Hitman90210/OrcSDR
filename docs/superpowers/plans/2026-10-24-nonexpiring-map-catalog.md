# Production map catalog without expiry

User requirement: remove TEST-ONLY status and do not expire the map catalog.
Installed maps already remain available offline independently of catalog expiry.

Design: keep `orcmaps-catalog-v1` and its required `expires_at` field, with JSON
`null` explicitly meaning no expiry. Existing valid dated catalogs continue to
work. Old firmware rejects null safely; new firmware must ship before this
catalog is enabled. Do not use a distant fake expiry date or silently interpret
missing/invalid values as unlimited validity. Keep signatures, trusted clock /
generation checks, revision+hash rollback protection, full archive verification
and recoverable activation. Do not reuse the TEST-ONLY signing key in production.

Implementation sequence:
1. Add failing focused tests in OrcMaps builder and OrcSDR signed consumer /
   installer for null expiry, malformed/missing expiry, dated expiry and retained
   rollback/future/clock checks.
2. Update builder and consumer/installer; run those targeted checks and P4 syntax.
3. Prepare a production catalog review package reusing the immutable verified
   global archive. Record deployment/migration and signing-key requirements.
4. Before publication, obtain the goal's separate explicit approval for production
   key creation/publication. Validate configured firmware and signed public
   catalog before claiming production downloads enabled. No main merge implied.

Source implementation is authorized by the user's request. No data download,
production key or publication is needed for steps 1–3. Keep COM39 untouched.
