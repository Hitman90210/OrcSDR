# Production map delivery: decision for approval

October 24 user decision: production catalog must not expire. The earlier expiry-renewal recommendation below is superseded by the [nonexpiring production package](2026-10-24-production-catalog/README.md). Production signing/publication were subsequently explicitly approved and executed; see the linked activation evidence. Native configured validation and firmware release packaging remain separate gates.

Prepared October 23, 2026. Recommendation only: no production publication,
account, payment, domain, key generation or firmware configuration change.

**Decision recorded:** the user approved GitHub Releases as the initial hosting
choice on October 23. This authorizes the transport choice, not production key
creation, publication or a main merge. Those approval gates remain open.

## Recommended first release

Keep the OrcMaps-owned signed catalog and use **GitHub Releases initially**.
The existing GitHub route has actual COM17 installation/resume/offline evidence;
changing hosts is not necessary to own our catalog. Keep R2 behind an owned
domain as a later transport option if measured demand warrants it. Do not put
map archives into Git history or Git LFS.

GitHub documents release assets under 2 GiB each and no total release-size or
bandwidth limit. The current 543,380,195-byte archive fits. That is an asset-size
policy, not a guarantee of 20,000 concurrent downloads or an availability SLA.
See [GitHub's release limits](https://docs.github.com/en/repositories/releasing-projects-on-github/about-releases#storage-and-bandwidth-quotas).

The product is one global regional-detail base (stored zooms 0–8), shared by the
picker, LoRa, ADS-B and FT8 cameras/overlays. No per-user build service is needed.
Optional closer-detail packs can later use the same catalog and manifest process.
Keep the firmware overview as a fallback where provisioned; neither fallback
availability nor street-level detail should be promised beyond tested packages.

## Verified current state

Read-only `gh release view` on October 23 confirmed both existing releases are
published **prereleases**, not drafts:

- [TEST-ONLY catalog](https://github.com/hardcoreerik/OrcMaps/releases/tag/maps-catalog-test-20261022): JSON, detached signature and test public key.
- [Global candidate](https://github.com/hardcoreerik/OrcMaps/releases/tag/sha256/16adde0b4f67734535db6871b89949e68c7e1dabf1c34d2bd7bd23842a9e5991): PMTiles, matching manifest and checksum.

These are not an approved production channel or production trust anchor. The
installed COM17 app uses the TEST-ONLY configuration. Merely setting location
does not download a map. The [pilot publication evidence](../evidence/maps/2026-10-22-download-pilot-publication.md)
records actual device installation; the older pilot-review proposal is historical.

## Delivery contract

Keep the existing schema, HTTPS checks, full archive SHA-256, bounds/profile/size
validation, anti-rollback catalog revision and recoverable installer. Catalog
archive URLs must contain a complete digest path segment and end in `.pmtiles`.
The validator is host-independent; changing to another HTTPS object host does not
require a map format or renderer fork. Any new host still needs native TLS,
redirect, Range/resume, timeout and failure checks before release.

Publish immutable data first, verify the public objects, then publish a matching
signed catalog. Never replace bytes beneath a digest URL. JSON and signature are
separate downloads, not an atomic pair: a mismatched pair must fail closed. Test
publication transitions and retry behavior rather than assuming atomic updates.
Keep old assets reachable for firmware/catalog versions still in use.

For the first production channel, use an explicit versioned release URL compiled
into the shipping build. This is the existing mechanism, not a new automatic
channel service. Catalog expiry requires a documented refresh/re-sign schedule
and an agreed firmware/channel update path; do not ship a short-lived pilot URL
and imply indefinite in-app catalog availability. A stable update endpoint is a
separate implementation/validation decision before promising that behavior.

Generate a **separate production P-256 key only after approval**. Keep its private
material outside the repository/device/public release; establish protected backup
and authorized signing access. Embed only its public key in production firmware.
Record key fingerprint, accepted catalog revision/expiry and signing provenance.
Never erase a device's trust floor to accept a new channel. Key rotation and a
test-to-production migration must be tested explicitly; no rotation API is claimed.

## Capacity and cost model

Existing [sizing.json](2026-10-22-pilot-review/sizing.json) gives:

| Workload | Size / requests |
|---|---:|
| One full download | 543,380,195 bytes |
| 20,000 full downloads | 10,867,603,900,000 bytes (10.8676039 decimal TB) |
| Ten retained archive versions | 5,433,801,950 bytes |
| Hypothetical 4 MiB ranges per download | 130 |
| Those ranges for 20,000 downloads | 2,600,000 GETs |

The range model is a scenario, not current firmware request behavior. The 16 KiB
RAM transfer buffer is not one HTTP request per buffer. Retries, metadata,
catalog polling, abuse, concurrent demand and update frequency add workload.

For comparison, R2 Standard lists $0.015/GB-month storage, $4.50/million Class A,
$0.36/million Class B, and free internet egress. Its monthly free allowances are
10 GB-month, 1 million A and 10 million B. The modeled storage/GET quantities fit
those allowances only if otherwise unused. Account-wide usage, rounding and
other services prevent treating this as a guaranteed zero-dollar bill.
[Current R2 pricing](https://developers.cloudflare.com/r2/pricing/).

R2 production should use an approved custom domain; the managed `r2.dev` endpoint
is development-only and rate-limited. The candidate is above the published
512 MB CDN cache limit for Free/Pro/Business plans, even treating MB as MiB.
Origin delivery and CDN caching are different gates. Do not split the pack or
add a Worker merely to avoid that limit without a demonstrated need.
[Public bucket guidance](https://developers.cloudflare.com/r2/buckets/public-buckets/),
[R2 cache limits](https://developers.cloudflare.com/cache/interaction-cloudflare-products/r2/).

## User-facing path

Keep Setup → Offline Maps and Settings → Data & Maps → Local Maps → Get Maps.
Show coverage, size, free-space requirement, progress/resume and verified install
result. Location chooses the view; it must not imply the regional archive is
already installed. The [manual SD guide](../user-guide/offline-maps.md) describes
the matching `.pmtiles` + `.manifest.json` pair in `/orcmaps/`, optional checksum,
PC verification and Rescan/selection. No ready-made ZIP or QR link exists yet.
Manual discovery checks metadata/size, not a full archive hash.

## Decisions and release gates still open

1. GitHub Releases is approved as the initial production transport. Any R2 account,
   paid service or owned-domain configuration still needs separate authorization.
2. Approve production key creation/custody, catalog lifetime/maintenance and
   test-to-production trust migration after reviewing the concrete procedure.
3. Accept source/derivation/license/attribution provenance from the existing
   pilot record; revalidate the exact production archive and public objects.
4. Pass insufficient-space, removed-card, corruption, interruption and replacement
   tests with disposable owned data. Resolve the measured approximately 12-minute
   verification/activation cost without bypassing integrity checks.
5. Pass shared firmware/map acceptance and final review, then request explicit
   main merge and production publication approval. This brief closes preparation
   of a concrete proposal; it does not close those implementation or approval gates.
