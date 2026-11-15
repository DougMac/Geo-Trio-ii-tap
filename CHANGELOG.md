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
- **Meter scaling** sensor: the scaling in use and whether it came from the meter or the defaults (see below).
- **Capture message examples** switch: logs the full hex of the first frame of each kind (for reporting new kinds).

### Meters
- **Meter scaling from the meters themselves.** Readings use each meter's own multiplier/divisor (Metering 0x0301/0x0302) and costs its currency scaling (Prepayment 0x0502) instead of a fixed /1000 and /100000. They're sent at IHD start-up and about hourly, and saved to flash. Sanity limits: unit kWh (elec) / m³ (gas), multiplier 1–1000, divisor 1–1,000,000, currency scaling 0x01 (the only code confirmed so far). Anything else is rejected, the defaults are used and the Meter scaling sensor starts "CHECK SCALING". The defaults are `substitutions` at the top of `geo-trio-tap.yaml` (`elec_divisor`, `gas_divisor`, `cost_divisor`). Until a meter's scaling has been seen once, its readings wait up to 3 minutes after boot so wrongly scaled first values don't reach the Energy dashboard. Assumption: the gas kWh figures (0x0Cxx) use the same divisor as m³.
- **Gas on endpoint 2 or 3.** Most installs get the gas meter's readings on endpoint 3 (endpoint 2 = gas tariff only), but some get them on endpoint 2. The firmware uses whichever sends gas readings first after boot (logged as `Gas meter data on endpoint N`) and ignores the other. Gas tariff is accepted from either. Electricity stays on endpoint 1; SMETS1 layouts aren't supported.

### Tariff handling
- **Future-tariff guard:** Price commands are applied only once their start time has passed; empty slots (start 0xFFFFFFFF) are ignored; a future tariff is logged once in advance.
- A standing charge of 0 is accepted (with a warning), in case standing charges are abolished.

### Hardware
- **Wi-Fi transmit power is no longer limited.** A commented-out `output_power: 12dB` line is left in the `wifi:` section for boards that restart when Wi-Fi connects (a 5 V dip on some ESP32-C3 SuperMinis).

### Security
- API encryption, **encrypted OTA** (using the API key), password-protected fallback AP, and a local web page with digest login (web OTA disabled).

### Documentation
- `trio_known.h` commented: every message kind explained, the endpoint/cluster map from the meters' own descriptors, and what's known about the non-data serial message types (0x8023 = network discovery replies, 0x802A = send failed/no ACK, 0x0072/0x800A = start-up handshake, 0x0514/0x8073/0x8024 = rescan, rejoin attempts and join result, ...).
- `dashboard/`: an example Home Assistant dashboard for these sensors (stale sensors shown as "Electricity readings" / "Gas readings").
