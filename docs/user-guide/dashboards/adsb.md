# ADS-B 1090

ADS-B uses 1090 MHz Mode S messages. The receiver location is entered locally in global Settings.

The dashboard always shows live receiver state. When no valid aircraft are in
range, it reports `WAITING` or `SEARCHING` and leaves unavailable fields empty.
It does not insert sample aircraft.

- **Radar** plots only aircraft with a valid decoded position.
- **List** also retains aircraft that lack a position.
- **Target** follows the same selected ICAO across views.
- **Stats** reports message and receiver health using relative signal units.
- **Settings** opens the global Location and ADS-B section.

Registration and aircraft metadata come from the local SD database when a matching ICAO exists. Missing fields display an em dash rather than zero or made-up data.
