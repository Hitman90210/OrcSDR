# Global map download pilot — review package, October 22, 2026

**Execution update:** the user approved this pilot; the candidate and TEST-ONLY
signed catalog are now published as prereleases. GitHub rejected the proposed
pure-hash tag, so the verified final data tag is `sha256/<full hash>`.
The files below preserve the original review proposal; use the
[execution record](../../evidence/maps/2026-10-22-download-pilot-publication.md)
for actual URLs, revision, hashes, commands and still-open P4 acceptance gates.

**Unpublished, unsigned candidate. Not a production catalog or enabled download.**
This package makes the next delivery step concrete. It does not approve publishing
or substitute for the remaining worldwide quality, ESP32-P4 and recovery gates.

## Proposed next step

Use two **GitHub prereleases in the owned `hardcoreerik/OrcMaps` repository** for
a bounded download pilot. This reuses an existing account and requires no new
paid storage service or domain. Keep firmware and the catalog provider-neutral.
Evaluate R2 behind an owned domain separately for production delivery.

GitHub documents a per-asset limit below 2 GiB and no aggregate release-size or
bandwidth limit. The 543,380,195-byte candidate fits that file limit. This is not
an availability SLA, a load test or a guarantee that 20,000 simultaneous devices
work. [GitHub release limits](https://docs.github.com/en/repositories/releasing-projects-on-github/about-releases#storage-and-bandwidth-quotas).

The proposed data tag is the complete archive SHA-256. This gives the existing
catalog contract its required complete SHA-256 URL path segment without weakening
validation. `git check-ref-format` accepts the proposed tag; no tag was created.
Both data and catalog prereleases would target published engine commit
`b59d47819cc3b81c1bbe50ed75b775275bb0218e` for provenance, not merge it to main.

| Item | Proposed value |
|---|---|
| Stable product | `global-regional` |
| Data release tag | `16adde0b4f67734535db6871b89949e68c7e1dabf1c34d2bd7bd23842a9e5991` |
| Data asset | `global-regional-z0-z8.pmtiles` |
| Catalog release tag | `maps-catalog-test-20261022` |
| Catalog assets | `maps-catalog-v1.json`, `maps-catalog-v1.sig` |
| Pilot signer | Newly generated **TEST-ONLY P-256** key after approval; never a production key |
| Test device | COM17 `30:ED:A0:E2:E6:95`, app-only candidate and existing recovery |
| Test release status | Prerelease, clearly marked unaccepted candidate; no production firmware release |

Full proposed URLs are in `review-status.json` and the catalog input. They are
not live endpoints. The timestamped catalog is for a pilot, not a long-term
update channel. Regenerate generation/expiry dates immediately before signing;
do not publish an expired review payload. Before accepting any test catalog on
COM17, inspect its existing trust floor; never erase trust to make a test pass.
Record the accepted test revision so a later production catalog uses a compatible
higher revision. Production trust-key establishment/rotation remains separately
approved work; this proposal does not authorize it.

## What users would receive

One common global regional-detail base: PMTiles v3, `protomaps-v4`, stored zooms
0–8, approximately 518.2 MiB (543.4 decimal MB). Longitude coverage is worldwide;
latitude is limited to Web Mercator, about ±85.0511287°. Stored detail can be
displayed at closer camera zooms; doing so does not create missing street detail.
There is no per-user map generation and no Google basemap content. Dashboard
cameras and overlays reuse this archive through the shared engine.

The built-in overview remains the offline fallback. Installing the regional base
adds available geographic detail; setting receiver location chooses the center,
not an automatic download. Optional local-detail products are not included in
this pilot and must not be advertised as available yet.

## Files and integrity scope

- `global-regional-z0-z8.manifest.json`: byte-identical copy of the existing
  candidate manifest, with source identity, derivation, bounds, versions and
  credits. Its candidate/unpublished fields remain truthful at preparation time.
- `catalog-input.review.json`: actual producer input shape, using the proposed
  URL and local manifest basename. The production builder will require the
  archive beside that manifest under the matching stem before it can sign.
- `maps-catalog-v1.REVIEW-UNSIGNED.json`: 3,040-byte canonical payload, not an
  installable catalog. No `.sig` is present; the firmware must reject unsigned data.
- `review-status.json`: explicit unpublished/unsigned state and recorded hashes.
- `sizing.json`: reproducible arithmetic for 20,000 complete downloads.

The archive remains at the existing OrcMaps worktree location:
`F:/AI/OrcSDR-TEMP/orcmaps-quality-delivery/.local/global-assessment/global-z8-candidate.pmtiles`.
Its current length matches the manifest and prior download record. The recorded
SHA-256 is `16adde0b4f67734535db6871b89949e68c7e1dabf1c34d2bd7bd23842a9e5991`.
This preparation **did not reread/hash/copy the 543 MB archive**. The full producer
hash, real engine preflight and signature verification must run before upload.
The builder's existing checks must not be bypassed using the review payload.

## Provenance and redistribution record

The input is the capped zoom 0–8 extract from the documented Source Cooperative
Protomaps mirror, not a locally regenerated planet. The upstream 134 GB object
was not fully hashed. Exact source ETag, extraction tool and local output hash
are retained in the manifest. OSM replication is 118749 at
2026-03-31T04:00:00Z; Protomaps metadata identifies v4.14.3. Exact Natural Earth
revision is unspecified; this limitation must accompany the candidate.

Protomaps describes its distributed basemap as an ODbL Produced Work requiring
OpenStreetMap attribution and discourages hotlinking its download mirror.
Copying the approved candidate to our release delivery location avoids that
runtime dependency. [Upstream download terms](https://docs.protomaps.com/basemaps/downloads).
OpenStreetMap requires contributor credit and ODbL information;
[copyright and license](https://www.openstreetmap.org/copyright).
Natural Earth data are public domain;
[terms](https://www.naturalearthdata.com/about/terms-of-use/).

Publish attribution/license/source information alongside the archive and keep
it available in the device map information. Retain the required visible credit
`Protomaps | (c) OpenStreetMap contributors | Natural Earth`. These statements
record upstream terms and this candidate's provenance; they are not a claim of
reproducing the source planet or accepting every map tile/script on hardware.

## Size and delivery costs

20,000 complete copies transfer **10.8676039 decimal TB**. Four-MiB request ranges
would require 130 requests per user, 2.6 million total, excluding metadata/retries.
Request range size is not a RAM allocation. Ten retained candidate versions use
5.43380195 decimal GB. Exact arithmetic is in `sizing.json`.

Current R2 Standard rates are $0.015/GB-month, $4.50/million Class A and
$0.36/million Class B operations, with free internet egress. Standard allowances
are 10 GB-month, one million A and ten million B operations monthly. This modeled
archive storage and 2.6 million GETs fit those allowances if unused elsewhere;
it is not a total bill quote. Domains, other services, retries and account usage
are excluded. [Official pricing, checked October 22](https://developers.cloudflare.com/r2/pricing/).
Production R2 should use an approved owned domain; `r2.dev` is intended for
rate-limited development traffic. Do not assume the 543 MB file is CDN-cacheable
on a particular plan. [Public-bucket guidance](https://developers.cloudflare.com/r2/buckets/public-buckets/).

## Executed checks

Preparation checks current file length against the recorded manifest/download
metadata, exact copied-manifest bytes, the actual builder's `immutable_url`
policy and canonical encoding/size limits. It does not run the final archive
hash/signing pipeline. Log: root `map-delivery-review-prepare-20261022.log`.

A bounded public GitHub asset probe reads exactly two 128-byte ranges from the
official go-pmtiles v1.28.2 Windows ZIP. HTTPS certificate validation is enabled,
redirects are limited to four HTTPS hops without credentials/control characters,
and temporary redirect URLs are not logged. Both responses are HTTP 206 with
exact Content-Range/Length and the same strong ETag; the second uses If-Range.
Total body read: **256 bytes**. Logs: root `map-github-range-probe-20261022.json`
and `.log`; script `.local/maps/probe_github_ranges_20261022.py`.
This is a host transport probe of another asset, not a P4 install or endpoint
capacity test. The actual published OrcMaps objects must be checked separately.

## Approval and execution boundary

The requested next authorization is limited to the **GitHub download pilot**:
fresh local archive hash/preflight, TEST-ONLY signing material, signing and
verification with the existing producer, creating/uploading/publishing the two
named prereleases, bounded real-endpoint tests and COM17 download/recovery work.
The 543 MB upload is included explicitly. No production key, paid account/domain,
main merge, existing draft replacement, COM39 access or user-data erasure is
included. The existing draft `world-overview-1` must remain untouched.

After approval, stage metadata and archive under matching basenames in a new
`.local/maps/` directory; invoke OrcMaps `tools/pack-builder/build_map_catalog.py`
with the current real pack verifier, operator-selected TEST-ONLY private key and
matching public key. It must rehash/preflight the archive and verify the detached
signature before output. Inspect output identity/size/expiry and public-only
firmware inputs before publication or flash. Do not upload this unsigned review
payload as a signed catalog. Store private material outside tracked files.

Then test COM17: trusted HTTPS catalog acceptance, free-space handling, bounded
transfer/cancel/resume, hash/schema rejection, activation/recovery and offline
use. Preserve working packs, NVS/world/C6 and recovery. Destructive card removal,
formatting or power-cut campaigns require separate scope. Update the evidence
and report every failed gate; do not declare downloader completion merely from
the first successful transfer. Production delivery and worldwide quality remain
separate acceptance decisions.
