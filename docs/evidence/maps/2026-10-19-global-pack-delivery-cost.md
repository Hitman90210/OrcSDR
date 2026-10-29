# Global-pack delivery sizing — October 19, 2026

This is a calculation and provider assessment, not a hosting decision,
publication, purchase, load test or availability guarantee. No account changes.

## Inputs and reproducible arithmetic

Existing candidate: 543,380,195 bytes, z0–8. The earlier recorded hash and quality
gates still apply; this assessment did not reread or rehash the large archive.
Assume 20,000 users each download exactly one complete copy in one month.

    bytes = 543380195
    users = 20000
    traffic = bytes * users = 10867603900000 bytes = 10.8676039 decimal TB
    GETs per user at 1 MiB ranges = ceil(bytes / 1048576) = 519
    GETs per user at 4 MiB ranges = ceil(bytes / 4194304) = 130

Range size is a network request size, not a RAM buffer allocation. A resumable
client can stream large ranges through small fixed SD buffers. One uninterrupted
GET resumed only after a failure could use fewer requests; these scenarios model
deliberately chunked transfers. Retries and metadata requests add operations.

PowerShell arithmetic assertions passed from the integration worktree; recorded
inputs/results are in `global-pack-delivery-arithmetic-20261019.json` beside the
invocation directory. This checks arithmetic only, not pricing or an endpoint.

## One viable candidate: R2 Standard behind an owned domain

Published R2 Standard rates are $0.015/GB-month storage, $4.50/million Class A
operations and $0.36/million Class B operations; internet egress is free. Monthly
Standard allowances are 10 GB-month, one million A and ten million B operations.
GET/HEAD are B operations. Usage rounds upward to billing units. These allowances
are shared with other account usage. Recheck before deployment.
[Official pricing](https://developers.cloudflare.com/r2/pricing/).

The following calculated B-operation estimates assume the full allowance remains
available, no cache savings, no other requests and the documented rounding:

| Scenario | GET operations | Rounded billable B operations | Estimated B cost |
| --- | ---: | ---: | ---: |
| One GET per download | 20,000 | 0 | $0 |
| 4 MiB ranges | 2,600,000 | 0 | $0 |
| 1 MiB ranges | 10,380,000 | 1,000,000 | $0.36 |
| 4 MiB ranges, twice all traffic | 5,200,000 | 0 | $0 |
| 1 MiB ranges, twice all traffic | 20,760,000 | 11,000,000 | $3.96 |

Retaining ten candidate-sized versions is 5,433,801,950 bytes, about 5.43 decimal
GB. Optional local packs, backups and daily peak accounting must be added.
These scenarios exclude domain charges, additional services, taxes and account
usage outside this model; they are not a total operating-cost quote.

Cloudflare designates r2.dev as rate-limited development access. A production
evaluation should use an owned custom domain, with caching/control configuration
reviewed separately. Do not assume this 543 MB object is CDN-cacheable on a
chosen plan; R2 serving cost above assumes no cache benefit.
[Official public-bucket guidance](https://developers.cloudflare.com/r2/buckets/public-buckets/).

## Architecture implication, pending user choice

An owned, signed catalog can identify immutable pack URLs and mirrors without
coupling the firmware to R2 APIs. Build each version once; all users download
the same artifact. No per-user global-map build or compulsory 2 GB download.
Manual compatible-pack import must work independently of catalog hosting.

Before recommending deployment, obtain the user's provider/domain decision and
publication approval, validate redistribution/provenance/attribution, measure
range/resume behavior and capacity, and configure spend/usage monitoring. Test
TLS, redirects, response identity, Content-Range, length and final SHA-256 on the
actual endpoint. An interrupted or changed object must never activate a partial
pack. Provider availability and source attribution are separate from signature
and file-integrity checks. Do not show unapproved URLs as available downloads.

The existing generic signed-data catalog does not automatically inventory
arbitrary OrcMaps PMTiles. Discovery, trusted delivery and legacy ORCMAP1 remain
distinct operations in the shared integration.
