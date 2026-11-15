// Message kinds seen in the 5 Nov 2026 captures (checksum-valid frames only).
// Anything else is reported to HA as "new".
//
// Kind format: E<endpoint>-C<cluster>-<what>
//   endpoints: E1 = electricity meter, E2 = gas tariff (via comms hub), E3 = gas meter
//   Confirmed by the meters' own descriptors at Trio start-up (8023 replies, 15 Nov 2026):
//     elec meter ep1 (SE device 0x0500): Basic, Time, Price, DRLC, Metering, Messaging,
//       Tunneling, Prepayment, Calendar, Device Mgmt, Events, Key Establishment
//     comms hub 0x0000 ep2 (gas service, 0x0500): Basic, DRLC, Messaging, Price, Time, Calendar, Device Mgmt
//     comms hub 0x0000 ep3 (gas metering device, 0x0501): Basic, Metering, Prepayment, Events
//   Calendar is advertised by both, but the Trio's calendar request still gets G0B "unsupported".
//   Some Trios get the gas meter's Metering/Prepayment on ep2 instead: the tap then lists those as E3
//   (see "Gas meter endpoint" in geo-trio-tap.yaml), so this list doesn't need an E2 copy.
//   A<attr> = attribute value, X<cmd> = cluster command, G01 = read-attributes response,
//   G0B = default response (all seen are status 0x81 "unsupported" replies to Trio requests)
//   M<type> = non-data message type on the Trio <-> Zigbee chip serial line
// [used] = decoded into a sensor; everything else is known but ignored.
#pragma once
#include <set>
#include <string>
static const std::set<std::string> TRIO_KNOWN = {
  // ---- Time cluster (0x000A), from the hub ----
  "E1-C000A-A0000",   // Time (UTC)
  "E1-C000A-A0001",   // TimeStatus
  "E1-C000A-A0007",   // LocalTime

  // ---- Price cluster (0x0700), electricity ----
  "E1-C0700-A0300",   // CommodityType
  "E1-C0700-G0B",     // default response (unsupported)
  "E1-C0700-X00",     // Publish Price [used]
  "E1-C0700-X04",     // Publish Tariff Information (standing charge) [used]

  // ---- Metering cluster (0x0702), electricity ----
  "E1-C0702-A0000",   // CurrentSummationDelivered = import total [used]
  "E1-C0702-A0001",   // CurrentSummationReceived = export total [used]
  "E1-C0702-A0014",   // SupplyStatus [used]
  "E1-C0702-A0200",   // Status flags [used]
  "E1-C0702-A0300",   // UnitOfMeasure
  "E1-C0702-A0301",   // Multiplier
  "E1-C0702-A0302",   // Divisor
  "E1-C0702-A0303",   // SummationFormatting
  "E1-C0702-A0306",   // MeteringDeviceType
  "E1-C0702-A0400",   // InstantaneousDemand = power [used]
  "E1-C0702-A0401",   // CurrentDayConsumptionDelivered = today [used]
  "E1-C0702-A0403",   // PreviousDayConsumptionDelivered = yesterday [used]
  "E1-C0702-A0420",   // previous day 2 consumption
  "E1-C0702-A0422",   // previous day 3
  "E1-C0702-A0424",   // previous day 4
  "E1-C0702-A0426",   // previous day 5
  "E1-C0702-A0428",   // previous day 6
  "E1-C0702-A042A",   // previous day 7
  "E1-C0702-A042C",   // previous day 8
  "E1-C0702-A0430",   // CurrentWeekConsumptionDelivered (from Monday) [used]
  "E1-C0702-A0432",   // previous week
  "E1-C0702-A0434",   // previous week 2
  "E1-C0702-A0436",   // previous week 3
  "E1-C0702-A0438",   // previous week 4
  "E1-C0702-A043A",   // previous week 5
  "E1-C0702-A0440",   // CurrentMonthConsumptionDelivered [used]
  "E1-C0702-A0442",   // previous month
  "E1-C0702-A0444",   // previous month 2
  "E1-C0702-A0446",   // previous month 3
  "E1-C0702-A0448",   // previous month 4
  "E1-C0702-A044A",   // previous month 5
  "E1-C0702-A044C",   // previous month 6
  "E1-C0702-A044E",   // previous month 7
  "E1-C0702-A0450",   // previous month 8
  "E1-C0702-A0452",   // previous month 9
  "E1-C0702-A0454",   // previous month 10
  "E1-C0702-A0456",   // previous month 11
  "E1-C0702-A0458",   // previous month 12
  "E1-C0702-A045A",   // previous month 13
  "E1-C0702-A0A00",   // BillToDateDelivered [used]
  "E1-C0702-A0A04",   // BillDeliveredTrailingDigit (decimal places for 0x0A00) [used]
  "E1-C0702-G01-empty",   // read-attributes response with nothing decodable (unsupported attrs)
  "E1-C0702-G0B",     // default response: Trio's GetProfile is unsupported
  "E1-C0702-X06",   // Metering: Publish Snapshot (daily midnight register reading)

  // ---- Messaging (0x0703), Tunneling (0x0704), electricity ----
  "E1-C0703-G0B",     // default response: Messaging cmd 0x02 unsupported
  "E1-C0703-X00",   // Messaging: Display Message (e.g. a supplier welcome message) [used]
  "E1-C0704-A0000",   // Tunneling: CloseTunnelTimeout
  "E1-C0704-G01-empty",   // Tunneling: empty read-attributes response (seen with the hourly tariff burst, 12 Nov 2026)
  "E1-C0704-X00",     // Tunneling: Request Tunnel Response

  // ---- Prepayment cluster (0x0705) = cost figures, electricity ----
  "E1-C0705-A0000",   // PaymentControlConfiguration
  "E1-C0705-A0500",   // HistoricalCostConsumptionFormatting
  "E1-C0705-A0501",   // ConsumptionUnitOfMeasurement
  "E1-C0705-A0502",   // CurrencyScalingFactor
  "E1-C0705-A051C",   // cost today [used]
  "E1-C0705-A051E",   // cost yesterday [used]
  "E1-C0705-A0520",   // cost previous day 2
  "E1-C0705-A0522",   // cost previous day 3
  "E1-C0705-A0524",   // cost previous day 4
  "E1-C0705-A0526",   // cost previous day 5
  "E1-C0705-A0528",   // cost previous day 6
  "E1-C0705-A052A",   // cost previous day 7
  "E1-C0705-A052C",   // cost previous day 8
  "E1-C0705-A0530",   // cost this week [used]
  "E1-C0705-A0532",   // cost previous week
  "E1-C0705-A0534",   // cost previous week 2
  "E1-C0705-A0536",   // cost previous week 3
  "E1-C0705-A0538",   // cost previous week 4
  "E1-C0705-A053A",   // cost previous week 5
  "E1-C0705-A0540",   // cost this month [used]
  "E1-C0705-A0542",   // cost previous month
  "E1-C0705-A0544",   // cost previous month 2
  "E1-C0705-A0546",   // cost previous month 3
  "E1-C0705-A0548",   // cost previous month 4
  "E1-C0705-A054A",   // cost previous month 5
  "E1-C0705-A054C",   // cost previous month 6
  "E1-C0705-A054E",   // cost previous month 7
  "E1-C0705-A0550",   // cost previous month 8
  "E1-C0705-A0552",   // cost previous month 9
  "E1-C0705-A0554",   // cost previous month 10
  "E1-C0705-A0556",   // cost previous month 11
  "E1-C0705-A0558",   // cost previous month 12
  "E1-C0705-A055A",   // cost previous month 13

  // ---- Calendar (0x0707), Device Management (0x0708), Key Establishment (0x0800), electricity ----
  "E1-C0707-G0B",     // default response: Trio's calendar request unsupported
  "E1-C0708-A0400",   // LowMediumThreshold (IHD traffic light, 1900; Trio's green/orange is ~1.5 kW)
  "E1-C0708-A0401",   // MediumHighThreshold (IHD traffic light, 3100; matches the Trio's red)
  "E1-C0708-G01-empty",   // ProviderName etc. (string attrs; read separately for Supplier) [used]
  "E1-C0708-G0B",     // default response: Device Mgmt cmd 0x01 unsupported
  "E1-C0800-A0000",   // KeyEstablishmentSuite

  // ---- Price cluster (0x0700), gas tariff (endpoint 2) ----
  "E2-C0700-A0300",   // CommodityType
  "E2-C0700-A0301",   // StandingCharge
  "E2-C0700-A0302",   // ConversionFactor
  "E2-C0700-A0303",   // ConversionFactorTrailingDigit
  "E2-C0700-A0304",   // CalorificValue
  "E2-C0700-A0305",   // CalorificValueUnit
  "E2-C0700-A0306",   // CalorificValueTrailingDigit
  "E2-C0700-G0B",     // default response from the hub, first seen 15 Nov 2026 17:36 (the Trio restart after the
                      // 15 Nov hub outage): 18 54 0B 05 C1 = reply to Price cmd 0x05 Get Calorific Value with
                      // status 0xC1 SOFTWARE_FAILURE (all earlier G0Bs were 0x81 unsupported). Harmless: the CV
                      // still arrives in the hub's Publish Calorific Value (X03)
  "E2-C0700-X00",     // Publish Price [used]
  "E2-C0700-X02",     // Publish Conversion Factor [used]
  "E2-C0700-X03",     // Publish Calorific Value [used]
  "E2-C0700-X04",     // Publish Tariff Information (standing charge) [used]
  "E2-C0700-X09",     // Publish Billing Period [used]
  "E2-C0703-G0B",     // default response: Messaging cmd 0x02 unsupported
  "E2-C0703-X00",   // Messaging: Display Message [used]
  "E2-C0707-G0B",     // default response: calendar request unsupported
  "E2-C0708-A0200",   // ChangeOfTenancy (0)
  "E2-C0708-A0201",   // ChangeOfTenancy implementation time (0)
  "E2-C0708-G01-empty",   // ProviderName etc. (string attrs; read separately for Supplier) [used]
  "E2-C0708-G0B",     // default response: Device Mgmt cmd 0x01 unsupported

  // ---- Metering cluster (0x0702), gas meter (endpoint 3) ----
  "E3-C0702-A0000",   // CurrentSummationDelivered = m3 total [used]
  "E3-C0702-A0007",   // ReadingSnapshotTime (time of the last gas reading)
  "E3-C0702-A0014",   // SupplyStatus [used]
  "E3-C0702-A0200",   // Status flags [used]
  "E3-C0702-A0300",   // UnitOfMeasure
  "E3-C0702-A0301",   // Multiplier
  "E3-C0702-A0302",   // Divisor
  "E3-C0702-A0303",   // SummationFormatting
  "E3-C0702-A0306",   // MeteringDeviceType
  "E3-C0702-A0A00",   // BillToDateDelivered [used]
  "E3-C0702-A0A04",   // BillDeliveredTrailingDigit [used]
  "E3-C0702-A0C01",   // kWh today [used]
  "E3-C0702-A0C03",   // kWh yesterday [used]
  "E3-C0702-A0C20",   // kWh previous day 2
  "E3-C0702-A0C22",   // kWh previous day 3
  "E3-C0702-A0C24",   // kWh previous day 4
  "E3-C0702-A0C26",   // kWh previous day 5
  "E3-C0702-A0C28",   // kWh previous day 6
  "E3-C0702-A0C2A",   // kWh previous day 7
  "E3-C0702-A0C2C",   // kWh previous day 8
  "E3-C0702-A0C30",   // kWh this week [used]
  "E3-C0702-A0C32",   // kWh previous week
  "E3-C0702-A0C34",   // kWh previous week 2
  "E3-C0702-A0C36",   // kWh previous week 3
  "E3-C0702-A0C38",   // kWh previous week 4
  "E3-C0702-A0C3A",   // kWh previous week 5
  "E3-C0702-A0C40",   // kWh this month [used]
  "E3-C0702-A0C42",   // kWh previous month
  "E3-C0702-A0C44",   // kWh previous month 2
  "E3-C0702-A0C46",   // kWh previous month 3
  "E3-C0702-A0C48",   // kWh previous month 4
  "E3-C0702-A0C4A",   // kWh previous month 5
  "E3-C0702-A0C4C",   // kWh previous month 6
  "E3-C0702-A0C4E",   // kWh previous month 7
  "E3-C0702-A0C50",   // kWh previous month 8
  "E3-C0702-A0C52",   // kWh previous month 9
  "E3-C0702-A0C54",   // kWh previous month 10
  "E3-C0702-A0C56",   // kWh previous month 11
  "E3-C0702-A0C58",   // kWh previous month 12
  "E3-C0702-A0C5A",   // kWh previous month 13
  "E3-C0702-G01-empty",   // read-attributes response with nothing decodable (unsupported attrs)
  "E3-C0702-G0B",     // default response: Trio's GetProfile is unsupported
  "E3-C0702-X06",   // Metering: Publish Snapshot (gas meter's daily midnight reading)
  "E3-C0702-X07",     // Get Sampled Data Response (gas half-hour samples -> gas power) [used]

  // ---- Prepayment cluster (0x0705) = cost figures, gas ----
  "E3-C0705-A0000",   // PaymentControlConfiguration
  "E3-C0705-A0500",   // HistoricalCostConsumptionFormatting
  "E3-C0705-A0501",   // ConsumptionUnitOfMeasurement
  "E3-C0705-A0502",   // CurrencyScalingFactor
  "E3-C0705-A051C",   // cost today [used]
  "E3-C0705-A051E",   // cost yesterday [used]
  "E3-C0705-A0520",   // cost previous day 2
  "E3-C0705-A0522",   // cost previous day 3
  "E3-C0705-A0524",   // cost previous day 4
  "E3-C0705-A0526",   // cost previous day 5
  "E3-C0705-A0528",   // cost previous day 6
  "E3-C0705-A052A",   // cost previous day 7
  "E3-C0705-A052C",   // cost previous day 8
  "E3-C0705-A0530",   // cost this week [used]
  "E3-C0705-A0532",   // cost previous week
  "E3-C0705-A0534",   // cost previous week 2
  "E3-C0705-A0536",   // cost previous week 3
  "E3-C0705-A0538",   // cost previous week 4
  "E3-C0705-A053A",   // cost previous week 5
  "E3-C0705-A0540",   // cost this month [used]
  "E3-C0705-A0542",   // cost previous month
  "E3-C0705-A0544",   // cost previous month 2
  "E3-C0705-A0546",   // cost previous month 3
  "E3-C0705-A0548",   // cost previous month 4
  "E3-C0705-A054A",   // cost previous month 5
  "E3-C0705-A054C",   // cost previous month 6
  "E3-C0705-A054E",   // cost previous month 7
  "E3-C0705-A0550",   // cost previous month 8
  "E3-C0705-A0552",   // cost previous month 9
  "E3-C0705-A0554",   // cost previous month 10
  "E3-C0705-A0556",   // cost previous month 11
  "E3-C0705-A0558",   // cost previous month 12
  "E3-C0705-A055A",   // cost previous month 13

  // ---- Non-data serial messages (Trio <-> Zigbee chip); NXP serial protocol variant (as ZiGate) ----
  "M0072",   // start-up handshake (with 800A)
  "M0514",   // Trio -> chip: rescan / rejoin the network
  "M7FFF",   // request sent / end of exchange
  "M8000",   // chip acknowledges a command (start-up; incl. the chip's firmware version)
  "M8008",   // send status
  "M8009",   // send status, with profile + cluster
  "M800A",   // start-up handshake reply
  "M8012",   // unknown (not seen on this Trio)
  "M8014",   // probably a stack status during busy sending (guess)
  "M8023",   // network discovery replies at start-up (node / match / simple descriptors)
  "M8024",   // join result: 12 bytes = joined (own address, channel, PAN ID); 08 CA = failed, no network
  "M8028",   // Zigbee chip debug text [used: Zigbee chip message]
  "M802A",   // send failed, no ACK (a few at start-up is normal)
  "M8071",   // stack event naming a neighbour (meter or hub) + code; harmless, meaning unknown
  "M8073",   // rejoin / scan attempt (every 1-2 s while the network is missing)
};
