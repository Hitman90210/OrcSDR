# Production catalog: no expiry

**October 25 install-policy update:** the user requested a quick saved-file check
instead of full payload hashing on device. The new path checks size, PMTiles
header/root and coverage; it does not scan the whole archive. Catalog signatures,
metadata identity, HTTPS and transaction/recovery protections remain. References
below to full archive verification describe the earlier activation checkpoint.
See [quick-check evidence](../../evidence/maps/2026-10-25-map-quick-file-check.md).

**Activation update:** the user approved production activation. The [production catalog](https://github.com/hardcoreerik/OrcMaps/releases/tag/maps-catalog-v1) is published and has no expiry. Public HTTPS/signature checks pass; the map archive is unchanged. See [execution evidence](../../evidence/maps/2026-10-24-production-catalog-activation.md). The unsigned/prepared descriptions below preserve the review package checkpoint.

User decision October 24: remove TEST-ONLY status and do not expire the catalog.
This supersedes the earlier recommendation to ship a catalog requiring periodic
expiry renewal. **Prepared, unsigned, unpublished.** No production key exists
from this step. Existing installed maps are already independent of expiry.

## Concrete publication package

- `catalog-unsigned.json`: one global-regional product, `expires_at: null`,
  revision greater than the pilot, generation time in UTC.
- `global-regional.manifest.json`: same map identity/profile/bounds/hash,
  production pack metadata `2026.10.24.1`, worldwide region label.
- Reuse the existing immutable 543,380,195-byte archive with SHA-256
  `16adde0b4f67734535db6871b89949e68c7e1dabf1c34d2bd7bd23842a9e5991`.
  No repeat archive upload or download is needed merely to change the catalog.
- Proposed production catalog GitHub release tag: `maps-catalog-v1`, with
  `maps-catalog-v1.json`, `maps-catalog-v1.sig`, and public-only signing key.
  Shipping firmware pins that channel URL and the new production public key.
- Promote the existing digest data release's title/status and add matching
  production manifest metadata after approval. Preserve immutable archive bytes,
  URL and historical pilot assets. Do not overwrite the pilot trust channel.

## Trust and deployment

Generate a separate P-256 production key outside the repositories, proposed
private directory `C:/Users/hardc/.orcsdr/private/maps-production/`. Restrict local
access; never put private material into Git, a device, logs or release assets.
Record fingerprint and backup/recovery instructions without printing the key.
Production key creation and publication need the goal's separate approval.

Deploy null-expiry-capable firmware before enabling this catalog. Old firmware
rejects null safely. Keep dated-catalog support and all generation/clock,
signature, hash, size, profile and revision+payload rollback checks. A nonexpiring
catalog remains usable without periodic re-signing; it does not guarantee GitHub
availability or automatic discovery of a newer catalog. User-triggered Check
fetches the configured catalog. Later catalog revisions can add compatible packs.

JSON/signature updates are separate assets: a transient mismatch must fail closed
and be retriable. Preserve old digest archives. Test pilot-to-production switch
with the same persisted revision floor, never by clearing NVS. Do not call an old
pilot-key firmware a production build merely by changing release labels.

## Validation so far

Red tests reproduced null rejection in builder, signed consumer and installer.
Green: OrcMaps builder 13 tests; OrcSDR 45 signed cases and transfer regression
1/1; four P4 syntax checks. See
`../../evidence/maps/2026-10-24-nonexpiring-catalog.md` for commands/logs.
This draft has not been production-signed, published, enabled in a release
package, or device-tested. Native build/flash authority is already recorded;
production key/publication is the remaining authorization boundary.
