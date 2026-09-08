# POCSAG frequency profiles

`FIND PAGERS` scans the frequency candidates in `/orcsdr/pocsag_scan.cfg`.
This file is OrcSDR's frequency-profile database: it is local, editable on
the SD card, and deliberately separate from firmware so a device is never
limited to one city or county.

Each non-comment line is a frequency in Hz. The scanner dwells four seconds
per candidate and selects a channel only after seeing real BCH-valid POCSAG
evidence. A candidate with only corrected words is reported as weak rather
than treated as a confirmed pager channel.

## Built-in seed

When no usable SD profile exists, OrcSDR uses the small generic US paging seed
shown below. It is a starting point for discovery, not a worldwide database
and not a claim that every channel carries POCSAG.

```text
152007500
152240000
152480000
152840000
157450000
157740000
158100000
158460000
158700000
```

## Build a local or regional profile

Copy this template to `/orcsdr/pocsag_scan.cfg`, then add only candidates
whose source, jurisdiction, and date you recorded. Keep profiles by region on
your computer and copy the intended one to the SD card before scanning.

```text
# Profile: region or service area
# Source: public licensing record, monitoring notes, or operator authorization
# Reviewed: YYYY-MM-DD
433920000
```

Do not ship a local profile as a universal default. A CAPCODE directory is a
separate local database: it is built from pages the receiver has actually
decoded and does not identify a transmitter by frequency alone.
