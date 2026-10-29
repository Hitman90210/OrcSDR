# Production location lookup: unresolved release decision

This is the address/IP lookup gate in the approved completion goal, separate
from map-file hosting. GitHub Releases hosting approval does not provision a
geocoding service. No provider, account, paid service, endpoint or key changed.

## Current implementation, checked October 24

`apps/orcsdr-tab5/ui/location_estimate.cpp` sends explicit address requests to
`https://nominatim.openstreetmap.org/search` and IP-area requests to
`https://ipwho.is/`. TLS uses the SDK certificate bundle and an OrcSDR User-Agent;
responses are bounded to 2 KiB with a 12-second timeout. Results are proposals:
the user confirms before storing coordinates. One last matching address result
is reused in RAM, and one task is allowed at a time per device. That is not a
fleet-wide cache or rate limit. Both endpoints are hardcoded in this candidate.

All three request callers were checked: wizard, Settings and serial. UI inputs
are bounded; `RTL_LOCATION LOOKUP` rejects queries of 64 bytes or longer before
calling the worker. A suspected exposed truncation issue was ruled out; no
redundant source guard was added.

## Service-policy constraint

The [public Nominatim policy](https://operations.osmfoundation.org/policies/nominatim/)
limits aggregate application traffic to one request per second, requires an
identifying User-Agent and attribution, and requires switching service without
a software update. It recommends proxying/caching, disallows autocomplete and
asks users not to submit personal or confidential information. These restrictions
apply to its public endpoint, not every independently operated Nominatim server.

Our per-device serialization and last-result reuse do not establish aggregate
compliance for 20,000 users. The hardcoded endpoint cannot be switched without
firmware replacement. A public prototype request succeeding does not close this
production gate. Full address entry also needs a clear provider/privacy notice;
do not silently treat a home address as an innocuous generic query.

## Recommended production path, for approval

Keep explicit user-triggered search and the existing confirmation/save behavior.
Use an Orc-controlled HTTPS endpoint backed by a geocoding provider whose terms,
capacity, storage/redistribution permission and privacy handling match the
product. The endpoint allows changing upstream service without updating devices.
Use server-side limits, bounded cache/expiry and minimal query retention; do not
ship a paid-provider secret in firmware. Return the existing bounded result shape
or add a small explicit adapter, not a generic online-map API framework.

Before deployment choose the actual provider, account owner, domain, budget cap,
query retention and outage policy. A self-hosted worldwide geocoder is another
option but has not been sized or approved; it is not provided by the 543 MB
PMTiles pack. Map archives are drawing data, not an on-device address index.

IP-area lookup is a second external service and needs its own terms/capacity/
privacy decision; configuring the address endpoint alone does not settle it.
No automatic IP lookup should be inferred from merely starting setup.

Required implementation/evidence after approval: configurable owned HTTPS
endpoint, visible provider/attribution/privacy text, explicit request controls,
timeout/error/cancel/fallback tests, serial parity, and bounded real-device lookup
followed by explicit save/Back checks. Network, map and direct-coordinate methods
remain useful, but they do not replace the requested functioning address method.
Do not mark setup production-ready by disabling address search to pass this gate.

## Native input-boundary evidence

COM17 `30:ED:A0:E2:E6:95`, installed source `137e244a`:

```text
python .local/maps/com17_label_bench.py maps-location-length-plan-20261024.json maps-location-length-device-20261024.log
python .local/maps/verify_location_length_check_20261024.py
```

Both exited 0. An authenticated 64-character query was rejected with
`RTL_LOCATION_ERROR query_length`; before/after lookup state matched. This
request cannot reach HTTP because the native serial handler rejects it first.
Setup and saved coordinates were left intact; no real address was submitted.
Logs `maps-location-length-{device,console,verification}-20261024.log` are saved
beside invocation. This validates the input guard, not provider availability,
policy compliance or real geocoding accuracy. COM39 untouched.
