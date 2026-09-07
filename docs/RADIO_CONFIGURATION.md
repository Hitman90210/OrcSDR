# Radio configuration files

OrcSDR works without Wi-Fi. Radio profiles live on the microSD card as plain
text. A clean installation does not assume a location or create a P25 system.

## P25 system profiles

Each P25 system is stored at `/orcsdr/p25/<profile-id>/profile.cfg`. OrcSDR
supports 16 installed profiles, eight control channels per profile, and eight
small talkgroup aliases in this first profile format. The Systems page lists,
selects, renames, imports, exports, reloads, and deletes profiles. Delete needs
a second confirmation tap.

```ini
# One key=value per line. Lines beginning with # or ; are comments.
version=2
system_name=My P25 System
nac=293
wacn=781824
system_id=101
rfss=1
site=2
control_channel_hz=851012500
control_channel_hz=851262500
last_control_channel_hz=851012500
auto_follow=true
encryption_skip=true
modulation=auto
cqpsk_timing_gain=0.005
cqpsk_carrier_gain=0.008
hold_talkgroup=0
talkgroup=123,Dispatch
```

Identity fields are optional hints. The dashboard reports decoded over-the-air
identity separately. Frequencies may use the RTL-SDR's supported 24-1766 MHz
range. `auto` modulation is recommended unless a known site needs forced
`c4fm` or `cqpsk` diagnosis.

To import from the screen, place a valid profile at
`/orcsdr/p25-import.cfg`. Exports go to
`/orcsdr/exports/p25-profile.cfg`. Serial commands allow explicit safe IDs and
export filenames; see [the serial API](API_SERIAL_CLI.md#p25-validation-and-replay).
The `p25_` ID prefix is reserved for signed catalog-owned packs so removing a
catalog pack cannot delete a locally imported profile.

### Migration

If profiles do not exist and `/orcsdr/P25.cfg` does, OrcSDR imports it once as
`legacy-import`, selects it, and leaves the original file untouched. Version-1
content is validated and rewritten as version 2 in the new directory. Invalid
input never replaces a working profile; saves retain a `.bak` rollback copy.

## `/orcsdr/pocsag_scan.cfg`

OrcSDR is used worldwide, so the POCSAG dashboard's "FIND PAGERS" scan list is
**not** a region baked into firmware. It ships with a small built-in default
of nationwide-allocated US paging-only channels (FCC Part 22), used only
until you save your own list. Put your own researched candidates — local
paging licenses, historical sites, known-active commercial paging channels —
in this file, one frequency in Hz per line; `#` starts a comment:

```text
# POCSAG / paging test list -- example only, not shipped as a default.
# Researched for one reference deployment (Lane County, Oregon); replace
# with candidates for your own area.

# Local FCC-licensed paging (Springfield, OR area)
454025000
454225000
454350000

# Commercial 900 MHz paging (Eugene, OR coverage; often FLEX, not POCSAG --
# the SIGNAL tab's sync/FEC stats will show that distinction)
929612500
929662500
929937500

# Historical / low-confidence
152240000
```

A missing, empty, or entirely unparseable file falls back to the built-in
default; a partially-invalid file keeps whatever lines did parse. There is
no in-app editor for this file yet — edit it on the SD card from a computer,
the same way P25 system profiles and FM presets are edited today.

## `/orcsdr/FM.cfg`

```ini
version=1
startup_frequency_hz=96100000
preset_hz=96100000
preset_hz=101700000
```

FM presets can be entered here or found with **FM → Settings → Scan Presets**.
The scan result is saved back to `FM.cfg`. Frequencies must be within the
76-108 MHz broadcast band, including Japan's 76-95 MHz allocation. The
previous saved file is retained as `FM.cfg.bak`.
