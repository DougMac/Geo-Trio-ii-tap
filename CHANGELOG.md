# Changelog

## Fork update (November 2026)

Changes in this fork compared with the original firmware. Every change was checked against live data from a SMETS2 install (electricity meter plus a gas meter via the comms hub) before it was kept.

### Fixes
- **Meter sanity filter recovers.** A jump after a data gap is accepted once 3 consistent readings agree, instead of being rejected until the next reboot.
- **Gas kWh total never re-initialises** after a reboot or a slightly old first frame (that could make a `total_increasing` sensor drop and spike the Energy dashboard).
- **"New message kind" cap** is 200 and stops reporting when reached, instead of re-reporting on every frame.
- **ZCL "invalid" values skipped** (all bits set, or signed minimum), so a meter sending "no value" can't publish ~2.8e11 kWh.
- **Zigbee chip text (0x8028)** handled before the checksum test, and `len + 5` status frames (0x8000) accepted, so neither counts as a damaged frame.
- **Fixed frame positions:** profile at byte 9 and the ZCL header at profile + 19, with an exact ZCL length check; the old scans remain as a fallback and are counted in a "Frame mismatch count" sensor.
- `uart` debug buffer raised to 512 bytes so long frames aren't split.

### New sensors
- **Electricity export meter** (0x0702/0x0001).
- **Gas power**: the meter's own half-hour gas sample (Get Sampled Data Response, Metering cmd 0x07), the same "30 minute average" the Trio shows.
- **This week** usage and cost (elec and gas), **bill to date** (0x0A00/0x0A04), **billing period** (Price cmd 0x09).
- **Running cost totals**: `Electricity cost (running total)` and `Gas cost (running total)` (`state_class: total`, never decrease). They add up the meter's own daily costs, **standing charges included**, so the Energy dashboard's costs match the bill instead of pricing usage only. To use them, in Settings → Dashboards → Energy, edit the electricity grid consumption and the gas source, choose **"Use an entity tracking the total costs"** and pick the matching running total.
- **Cost yesterday** from the meter's daily reset.
- **Electricity average unit rate today**, **calculated unit rates** (from the meter's cost and kWh), **electricity price tier** (for time-of-use tariffs).
- **Supply status** (elec/gas binary sensors), **meter status** flags, **supplier name** (Device Management ProviderName), **supplier message** (Messaging Display Message).
- **Zigbee link quality** (LQI from each data frame).
- **Electricity data stale / Gas data stale** binary sensors (gas uses the half hour's own end time, so a hub re-sending old data still counts as stale).
- **Meter scaling**: the meters' unit, multiplier, divisor and currency scaling, saved to flash; the text starts "CHECK SCALING" if they differ from what the decoder assumes (e.g. after a meter exchange).
- **Capture message examples** switch: logs the full hex of the first frame of each kind (for reporting new kinds).

### Tariff handling
- **Future-tariff guard:** Price commands are applied only once their start time has passed; empty slots (start 0xFFFFFFFF) are ignored; a future tariff is logged once in advance.
- A standing charge of 0 is accepted (with a warning), in case standing charges are abolished.

### Security
- API encryption, **encrypted OTA** (using the API key), password-protected fallback AP, and a local web page with digest login (web OTA disabled).

### Documentation
- `trio_known.h` commented: every message kind explained, the endpoint/cluster map from the meters' own descriptors, and what's known about the non-data serial message types (0x8023 = network discovery replies, 0x802A = send failed/no ACK, 0x0072/0x800A = start-up handshake, ...).
- `dashboard/`: an example Home Assistant dashboard for these sensors.
