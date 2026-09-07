# P25 research and provenance

This record distinguishes research from copied implementation. No source code
from the projects below was copied into the P25 profile work. The OrcSDR code
uses its existing parser, SD abstraction, signed catalog, and UI patterns.

| Source | License or terms | What we learned | OrcSDR derivation |
| --- | --- | --- | --- |
| [CISA Statement of P25 User Needs](https://www.cisa.gov/sites/default/files/publications/08-28-2020_P25-SPUN_FINAL_508c_0.pdf) | U.S. government publication | Phase I trunking uses a control channel to assign 12.5 kHz traffic channels to talkgroups. | Confirms the profile boundary between system/control-channel identity and talkgroup labels. |
| [FCC ULS public data](https://opendata.fcc.gov/Wireless/FCC-Universal-Licensing-System-ULS-/x28i-i4z4) | FCC public data; verify current terms for each release | License records can help identify licensed frequencies and entities, but they do not establish live control channels or trustworthy talkgroup aliases. | Candidate inputs remain subject to live verification and a release-specific source ledger. |
| [RadioReference API policy](https://support.radioreference.com/hc/en-us/articles/18844460198932-Database-Web-Service-API) | Per-user Premium subscription and approved developer access; redistribution requires the applicable agreement | Location-based radio programming is an intended API use, but mirroring or redistributing the database is outside the standard API permission. | OrcSDR publishes no RadioReference-derived system or talkgroup data. A future direct integration must use each user's account and approved access. |
| [sdrtrunk Playlist Editor](https://github.com/DSheirer/sdrtrunk/wiki/Playlist-Editor) | GPL-3.0 | Mature receivers keep channel configurations and alias lists associated with a system. | OrcSDR stores each system separately and keeps its small aliases inside that profile for now. |
| [Trunk Recorder](https://github.com/TrunkRecorder/trunk-recorder) | GPL-3.0 | A trunked receiver continuously follows control-channel grants; call handling is separate from system configuration. | This supports the existing OrcSDR separation between the profile store and receiver/voice state. No code was used. |
| [boatbod OP25](https://github.com/boatbod/op25) | GPL-3.0-or-later in reviewed source headers | Its public feature list treats multi-system selection, talkgroup tags, priority, hold/skip, replay, and encryption detection as separate receiver capabilities. | These are comparison points for OrcSDR's staged roadmap, not an implementation source. |

Reviewed 2026-09-06. Every future published P25 pack must add its own source,
license or terms decision, retrieval date, transformation steps, hash, and
maintainer contact to the data-source ledger.
