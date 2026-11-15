// Geo Trio II J8 decoder - NXP JN5169 serial frames -> Zigbee Smart Energy data.
// Frame: 0x01 .. 0x03; bytes 0x00-0x07 escaped as 0x02,(b^0x08); length + XOR checksum checked.
#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cmath>
struct TrioAttr { uint8_t endpoint; uint16_t cluster; uint16_t attr; int64_t raw; };

struct TrioFrame {
  bool ok = false;            // well-formed frame
  uint16_t msg_type = 0;      // e.g. 0x8002 = incoming Zigbee data
  uint8_t profile_pos = 0;    // where the 0x0109 profile was found (expected 9; 0 = not found)
  int16_t zcl_off = -1;       // ZCL header position relative to the profile (expected 19; -1 = not found)
  bool zcl = false;           // a Smart Energy (0x0109) ZCL payload was found
  uint8_t endpoint = 0;
  uint16_t cluster = 0;
  uint8_t frame_ctrl = 0;
  uint8_t cmd = 0;
  bool cluster_specific = false;
  std::vector<uint8_t> payload;   // ZCL payload after the command byte
  std::vector<TrioAttr> attrs;    // decoded attributes (global read-response / report)
  std::vector<uint8_t> unescaped;
};

static inline int trio_type_size(uint8_t t) {
  switch (t) {
    case 0x08: case 0x10: case 0x18: case 0x20: case 0x28: case 0x30: return 1;
    case 0x09: case 0x19: case 0x21: case 0x29: case 0x31: return 2;
    case 0x0a: case 0x1a: case 0x22: case 0x2a: return 3;
    case 0x0b: case 0x1b: case 0x23: case 0x2b: case 0xe2: return 4;
    case 0x0c: case 0x1c: case 0x24: case 0x2c: return 5;
    case 0x0d: case 0x1d: case 0x25: case 0x2d: return 6;
    case 0x0e: case 0x1e: case 0x26: case 0x2e: return 7;
    case 0x0f: case 0x1f: case 0x27: case 0x2f: return 8;
    default: return 0;
  }
}
static inline bool trio_type_signed(uint8_t t) { return t >= 0x28 && t <= 0x2f; }

static inline uint32_t trio_u32(const std::vector<uint8_t> &b, size_t i) {
  return uint32_t(b[i]) | (uint32_t(b[i+1]) << 8) | (uint32_t(b[i+2]) << 16) | (uint32_t(b[i+3]) << 24);
}

static inline TrioFrame trio_decode(const std::vector<uint8_t> &raw) {
  TrioFrame f;
  if (raw.size() < 4 || raw.front() != 0x01 || raw.back() != 0x03) return f;
  auto &u = f.unescaped; u.reserve(raw.size());
  bool esc = false;
  for (size_t i = 1; i + 1 < raw.size(); i++) {
    uint8_t b = raw[i];
    if (esc) { u.push_back(b ^ 0x08); esc = false; }
    else if (b == 0x02) esc = true;
    else u.push_back(b);
  }
  // NXP frame: type(2) length(2) checksum(1) data(normally length+2, see below). Checksum = XOR of all the
  // other bytes (type, length, data; after unescaping, start/end markers excluded).
  // Verified on 6,797 captured frames; rejects frames damaged by dropped or merged bytes.
  if (u.size() < 7) return f;
  uint16_t len = (uint16_t(u[2]) << 8) | u[3];
  // The stated length is 2 short of the real data: frames are normally len + 7 bytes, and the last 2 bytes
  // are still message data (in 0x8002 the ZCL length runs to the very end; checked on every kind, Nov 2026).
  // One exception seen: a 0x8000 Status frame at IHD start-up was exactly len + 5 with a valid checksum, so
  // len + 5 is accepted for any type except meter data (0x8002).
  uint16_t mtype = (uint16_t(u[0]) << 8) | u[1];
  bool len_ok = u.size() == size_t(len) + 7 || (u.size() == size_t(len) + 5 && mtype != 0x8002);
  if (!len_ok) return f;
  uint8_t x = 0;
  for (size_t i = 0; i < u.size(); i++) if (i != 4) x ^= u[i];
  if (x != u[4]) return f;
  f.ok = true;
  f.msg_type = (uint16_t(u[0]) << 8) | u[1];
  if (f.msg_type != 0x8002) return f;
  // Meter data (0x8002) header: type(2) length(2) checksum(1), byte 5 = running sequence number (shared by a
  // request and its reply frames), byte 6 = varies with the contents, bytes 7-8 = 00 AB, then the profile
  // 01 09 at byte 9 (fixed; verified on live data 9-15 Nov 2026). If byte 9 isn't 01 09, fall back to a scan
  // from byte 7 (so bytes 5-6 can't be mistaken for it); the YAML reports that as a frame mismatch.
  size_t p = 0; bool found = false;
  if (u.size() > 16 && u[9] == 0x01 && u[10] == 0x09) { p = 9; found = true; }
  else
    for (size_t i = 7; i + 7 < u.size(); i++)
      if (u[i] == 0x01 && u[i + 1] == 0x09) { p = i; found = true; break; }
  if (!found) return f;
  f.profile_pos = uint8_t(p > 255 ? 255 : p);
  f.cluster = (uint16_t(u[p + 2]) << 8) | u[p + 3];
  f.endpoint = u[p + 4];
  // ZCL header: frame control, seq, command; preceded by a 2-byte big-endian length of the whole ZCL
  // message (header + payload), which always ends exactly at the frame end. Fixed at p+19 with the length
  // at p+17/p+18 (verified 10-15 Nov 2026), checked by zcl_ok (frame control + exact length); if that
  // fails, fall back to a scan with the same check, and the YAML reports it as a frame mismatch.
  auto zcl_ok = [&](size_t i) -> bool {
    if (i < 2 || i + 3 > u.size()) return false;
    uint8_t fc = u[i];
    // No manufacturer code (bit 2) and reserved bits 5-7 clear; bit 4 (disable default response) may be
    // either. Direction (bit 3) and frame type (bits 0-1) are NOT checked: the fixed position and the
    // exact length check below do the work.
    if ((fc & 0xF4) != 0x00 && (fc & 0xF4) != 0x10) return false;
    uint16_t len = (uint16_t(u[i - 2]) << 8) | u[i - 1];
    // The stated length always ends exactly at the frame end (every kind, checked 14-15 Nov 2026).
    return len >= 3 && i + len == u.size();
  };
  size_t z = 0; bool zf = false;
  if (zcl_ok(p + 19)) { z = p + 19; zf = true; }
  else
    for (size_t i = p + 10; i + 3 <= u.size(); i++)
      if (zcl_ok(i)) { z = i; zf = true; break; }
  if (!zf) return f;
  f.zcl_off = int16_t(z - p);     // 19 normally; anything else = found by the fallback scan (YAML reports a frame mismatch)
  f.zcl = true;
  f.frame_ctrl = u[z];
  f.cluster_specific = (u[z] & 0x03) == 0x01;
  f.cmd = u[z + 2];
  f.payload.assign(u.begin() + z + 3, u.end());   // = the stated ZCL length (zcl_ok requires an exact match)
  if (f.cluster_specific || (f.cmd != 0x01 && f.cmd != 0x0a)) return f;
  // Global read-attributes response (0x01, has status byte) or report (0x0a)
  auto &pl = f.payload; size_t i = 0;
  while (i + 3 <= pl.size()) {
    uint16_t attr = uint16_t(pl[i]) | (uint16_t(pl[i + 1]) << 8); i += 2;
    if (f.cmd == 0x01) { uint8_t st = pl[i++]; if (st != 0x00) continue; }
    if (i >= pl.size()) break;
    uint8_t type = pl[i++];
    int sz = trio_type_size(type);
    if (sz == 0 || i + sz > pl.size()) break;
    uint64_t v = 0;
    for (int k = sz - 1; k >= 0; k--) v = (v << 8) | pl[i + k];
    i += sz;
    // ZCL "invalid / not available" markers: unsigned ints and enums with every bit set (e.g. uint48
    // 0xFFFFFFFFFFFF), signed ints at their minimum (e.g. int24 0x800000). Skip them, so a meter that
    // sends "no value" can never publish ~2.8e11 kWh or -8.4 MW into HA.
    {
      const uint64_t all = (sz >= 8) ? ~0ULL : ((1ULL << (sz * 8)) - 1);
      bool unsigned_like = (type >= 0x20 && type <= 0x27) || type == 0x30 || type == 0x31;
      bool invalid = (unsigned_like && v == all) ||
                     (trio_type_signed(type) && v == (1ULL << (sz * 8 - 1)));
      if (invalid) continue;
    }
    int64_t sv = int64_t(v);
    if (trio_type_signed(type) && sz < 8 && ((v >> (sz * 8 - 1)) & 1)) sv = int64_t(v | (~0ULL << (sz * 8)));
    f.attrs.push_back({f.endpoint, f.cluster, attr, sv});
  }
  return f;
}

// ---- Zigbee chip text (message type 0x8028) -----------------------------------------------
// The Trio's Zigbee chip (JN5169) sends its own log lines as ASCII from byte 7, e.g. "Start Main: Stack ptr ..."
// (chip boot) or "Initiating rejoin as MAC restored". Some fail the checksum (reason unknown), so the YAML
// checks for them BEFORE counting a frame as damaged. Works on f.unescaped, which is filled even when !f.ok.
static inline bool trio_is_chip_text(const TrioFrame &f) {
  return f.unescaped.size() > 7 && f.unescaped[0] == 0x80 && f.unescaped[1] == 0x28;
}
static inline std::string trio_chip_text(const TrioFrame &f) {
  std::string s;
  for (size_t i = 7; i < f.unescaped.size() && s.size() < 250; i++) {
    uint8_t c = f.unescaped[i];
    if (c >= 0x20 && c < 0x7F) s += char(c);          // printable only
  }
  size_t a = s.find_first_not_of(" "), b = s.find_last_not_of(" ");
  return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}

// ---- Messaging cluster (0x0703): supplier messages for the display ------------------------
// Display Message (cmd 0x00): MessageID(4) MessageControl(1) StartTime(4, UTC secs since 2000, 0 = now)
// DurationInMinutes(2) Message(length byte + text) [ExtendedMessageControl(1)].
// Returns true and fills text/start if this is a Display Message.
static inline bool trio_display_message(const TrioFrame &f, std::string &text, uint32_t &start) {
  if (!f.zcl || f.cluster != 0x0703 || !f.cluster_specific || f.cmd != 0x00) return false;
  const auto &p = f.payload;
  if (p.size() < 12) return false;
  start = trio_u32(p, 5);
  size_t n = p[11];
  if (12 + n > p.size()) n = p.size() - 12;   // short frame: take what is there
  text.clear();
  for (size_t i = 12; i < 12 + n; i++) {
    uint8_t c = p[i];
    if (c >= 0x20 && c < 0x7F) text += char(c);
  }
  return true;
}
// Cancel Message (cmd 0x01 = Cancel Message, 0x02 = Cancel All Messages in later SE versions)
static inline bool trio_cancel_message(const TrioFrame &f) {
  return f.zcl && f.cluster == 0x0703 && f.cluster_specific && (f.cmd == 0x01 || f.cmd == 0x02);
}

// ---- Text attribute (octet / character string) from a read-attributes response or report ----
// trio_decode() only keeps numbers, so strings are read here. E.g. Device Management (0x0708)
// attr 0x0101 ProviderName (supplier name), 0x0102 ProviderContactDetails (phone number).
static inline bool trio_string_attr(const TrioFrame &f, uint16_t cluster, uint16_t want, std::string &out) {
  if (!f.zcl || f.cluster_specific || f.cluster != cluster || (f.cmd != 0x01 && f.cmd != 0x0a)) return false;
  const auto &pl = f.payload; size_t i = 0;
  while (i + 3 <= pl.size()) {
    uint16_t attr = uint16_t(pl[i]) | (uint16_t(pl[i + 1]) << 8); i += 2;
    if (f.cmd == 0x01) { uint8_t st = pl[i++]; if (st != 0x00) continue; }   // unsupported: no type/value
    if (i >= pl.size()) return false;
    uint8_t type = pl[i++];
    bool text = type == 0x41 || type == 0x42;
    size_t sz;
    if (text) { if (i >= pl.size()) return false; sz = 1 + size_t(pl[i]); }   // length byte + bytes
    else { int s = trio_type_size(type); if (s == 0) return false; sz = size_t(s); }
    if (i + sz > pl.size()) return false;
    if (attr == want && text) {
      out.clear();
      for (size_t k = 1; k < sz; k++) { char c = char(pl[i + k]); if (c >= 0x20 && c < 0x7f) out += c; }
      return true;
    }
    i += sz;
  }
  return false;
}

// ---- Publish Billing Period (Price cluster cmd 0x09) ------------------------------------
// ProviderID(4) IssuerEventID(4) BillingPeriodStartTime(4, UTC secs since 2000, 0 = now)
// BillingPeriodDuration(3) BillingPeriodDurationType(1: low nibble = timebase 0 min, 1 days,
// 2 weeks, 3 months, 4 years) TariffType(1). Returns false if not this command / too short.
static inline bool trio_billing_period(const TrioFrame &f, uint32_t &start, uint32_t &duration, uint8_t &dtype) {
  if (!f.zcl || f.cluster != 0x0700 || !f.cluster_specific || f.cmd != 0x09 || f.payload.size() < 16) return false;
  const auto &p = f.payload;
  start = trio_u32(p, 8);
  duration = uint32_t(p[12]) | (uint32_t(p[13]) << 8) | (uint32_t(p[14]) << 16);
  dtype = p[15];
  return true;
}

// ---- Price cluster (0x0700) commands --------------------------------------------------
// Anchored on the currency code (0x033A = 826 = GBP), which sits at a fixed position
// relative to the fields we need in both Publish Price and Publish Tariff Information.
static inline int trio_find_gbp(const std::vector<uint8_t> &p, size_t from) {
  for (size_t i = from; i + 1 < p.size(); i++) if (p[i] == 0x3a && p[i + 1] == 0x03) return int(i);
  return -1;
}
static inline double trio_pow10(int n) { double d = 1; while (n-- > 0) d *= 10; return d; }

// Publish Price (cmd 0x00): returns unit price in GBP/kWh, or -1.
static inline double trio_publish_price(const TrioFrame &f) {
  if (!f.zcl || f.cluster != 0x0700 || !f.cluster_specific || f.cmd != 0x00) return -1;
  int g = trio_find_gbp(f.payload, 9);
  if (g < 0 || size_t(g) + 2 + 1 + 1 + 4 + 2 + 4 > f.payload.size()) return -1;
  int digits = f.payload[g + 2] >> 4;
  uint32_t price = trio_u32(f.payload, g + 2 + 1 + 1 + 4 + 2);
  if (price == 0 || price == 0xFFFFFFFF) return -1;
  return price / trio_pow10(digits);
}
// Publish Tariff Information (cmd 0x04): returns standing charge in GBP/day, or -1.
static inline double trio_standing_charge(const TrioFrame &f) {
  if (!f.zcl || f.cluster != 0x0700 || !f.cluster_specific || f.cmd != 0x04) return -1;
  int g = trio_find_gbp(f.payload, 16);
  if (g < 0 || size_t(g) + 2 + 1 + 4 > f.payload.size()) return -1;
  int digits = f.payload[g + 2] >> 4;
  uint32_t sc = trio_u32(f.payload, g + 3);
  if (sc == 0xFFFFFFFF) return -1;
  return sc / trio_pow10(digits);
}
// Start time (ZCL UTC seconds since 2000-01-01; 0 = "now") of a Price cluster tariff command, or 0 if none.
// Positions confirmed on live frames 14 Nov 2026:
//   Publish Price (00): 4 bytes after currency(2) + trailing digit(1) + tiers(1)  (elec: re-issued daily, gas: months ago)
//   Publish Tariff Info (04): payload[12]                                          (elec: 0, gas: tariff start date)
//   Conversion Factor (02) / Calorific Value (03): payload[4]                      (both 0)
// 0xFFFFFFFF marks an empty / unused slot (seen as an empty "future tariff" on another user's meter).
static inline uint32_t trio_price_start_time(const TrioFrame &f) {
  if (!f.zcl || f.cluster != 0x0700 || !f.cluster_specific) return 0;
  const auto &p = f.payload;
  if (f.cmd == 0x00) {
    int g = trio_find_gbp(p, 9);
    return (g >= 0 && size_t(g) + 8 <= p.size()) ? trio_u32(p, g + 4) : 0;
  }
  if (f.cmd == 0x04) return p.size() >= 16 ? trio_u32(p, 12) : 0;
  if (f.cmd == 0x02 || f.cmd == 0x03) return p.size() >= 8 ? trio_u32(p, 4) : 0;
  return 0;
}
// Currency position worked out from the ZCL field layout (as upstream PR #4 does) instead of searching for 3A 03.
// Publish Price: ProviderID(4), RateLabel(len byte + text), IssuerEventID(4), CurrentTime(4), UnitOfMeasure(1), Currency.
// Publish Tariff Info: ProviderID(4), IssuerEventID(4), IssuerTariffID(4), StartTime(4), TariffType(1),
//                      TariffLabel(len byte + text), TiersInUse(1), BlockThresholdsInUse(1), UnitOfMeasure(1), Currency.
// Monitoring only for now: the YAML compares it with trio_currency_pos_search() and reports any difference as a
// frame mismatch (none since 9 Nov 2026); plan: switch the decoder to it once proven across a supplier change.
// Returns -1 if not this command or too short.
static inline int trio_currency_pos_fixed(const TrioFrame &f) {
  if (!f.zcl || f.cluster != 0x0700 || !f.cluster_specific) return -1;
  const auto &p = f.payload;
  if (f.cmd == 0x00 && p.size() > 5) {
    size_t o = 5 + p[4];
    return (o + 23 <= p.size()) ? int(o + 9) : -1;
  }
  if (f.cmd == 0x04 && p.size() > 18) {
    size_t o = 18 + p[17];
    return (o + 10 <= p.size()) ? int(o + 3) : -1;
  }
  return -1;
}
// The same position as the current decoder finds it (GBP search), for the comparison.
static inline int trio_currency_pos_search(const TrioFrame &f) {
  if (!f.zcl || f.cluster != 0x0700 || !f.cluster_specific) return -1;
  if (f.cmd == 0x00) return trio_find_gbp(f.payload, 9);
  if (f.cmd == 0x04) return trio_find_gbp(f.payload, 16);
  return -1;
}
// Publish Price: price tier = low nibble of the byte after the currency (high nibble = decimal places);
// the next byte holds number of price tiers (high nibble) and register tier (low nibble). Returns -1 if not found.
static inline int trio_price_tier(const TrioFrame &f, int *num_tiers = nullptr) {
  if (!f.zcl || f.cluster != 0x0700 || !f.cluster_specific || f.cmd != 0x00) return -1;
  int g = trio_find_gbp(f.payload, 9);
  if (g < 0 || size_t(g) + 4 > f.payload.size()) return -1;
  if (num_tiers) *num_tiers = f.payload[g + 3] >> 4;
  return f.payload[g + 2] & 0x0F;
}
// Publish Conversion Factor (cmd 0x02) / Publish Calorific Value (cmd 0x03)
static inline double trio_conversion_factor(const TrioFrame &f) {
  if (!f.zcl || f.cluster != 0x0700 || !f.cluster_specific || f.cmd != 0x02 || f.payload.size() < 13) return -1;
  return trio_u32(f.payload, 8) / trio_pow10(f.payload[12] >> 4);
}
static inline double trio_calorific_value(const TrioFrame &f) {
  if (!f.zcl || f.cluster != 0x0700 || !f.cluster_specific || f.cmd != 0x03 || f.payload.size() < 14) return -1;
  if (f.payload[12] != 0x01) return -1;   // only MJ/m3 understood
  return trio_u32(f.payload, 8) / trio_pow10(f.payload[13] >> 4);
}

// ---- Metering Status (0x0702 attr 0x0200, bitmap8) as text --------------------------------
// ZCL Smart Energy flag meanings; bits 3, 4 and 7 differ between electricity and gas meters.
static inline std::string trio_meter_status_text(uint8_t bits, bool gas) {
  static const char *const elec_names[8] = {"Check meter", "Low battery", "Tamper detected", "Power failure",
                                            "Power quality", "Leak detected", "Service disconnect open", "Bit 7"};
  static const char *const gas_names[8]  = {"Check meter", "Low battery", "Tamper detected", "Bit 3",
                                            "Low pressure", "Leak detected", "Service disconnect", "Reverse flow"};
  if (bits == 0) return "OK";
  std::string s;
  for (int b = 0; b < 8; b++)
    if (bits & (1 << b)) { if (!s.empty()) s += ", "; s += (gas ? gas_names : elec_names)[b]; }
  return s;
}

// ---- Sanity filter for meter totals (kWh / m3) ------------------------------------------
// Accept a reading that rises by less than max_jump. A small dip (< stale) is an old frame.
// Anything else is rejected, unless the same new level keeps coming (each within tol of the
// one before): after 3 in a row it is accepted, so a data gap or meter swap can't block
// readings until the next reboot, while a single bad frame still can't get through.
struct TrioMeterFilter { double last = 0, cand = 0; int n = 0; };
enum TrioCheck { TRIO_ACCEPT, TRIO_STALE, TRIO_REJECT, TRIO_ACCEPT_AFTER_GAP };
static inline TrioCheck trio_meter_check(TrioMeterFilter &m, double v, double max_jump, double stale, double tol) {
  if (m.last == 0 || (v >= m.last && v - m.last < max_jump)) { m.last = v; m.n = 0; return TRIO_ACCEPT; }
  if (v < m.last && m.last - v < stale) return TRIO_STALE;
  if (m.n > 0 && std::fabs(v - m.cand) < tol) m.n++; else m.n = 1;
  m.cand = v;
  if (m.n >= 3) { m.last = v; m.n = 0; return TRIO_ACCEPT_AFTER_GAP; }
  return TRIO_REJECT;
}

// ---- Running cost total for the HA Energy dashboard (state_class total, never decreases) ----
// Uses the meter's own figures, standing charge included:
//  - every rise in "cost today" is added (this includes the gas meter adding its standing charge ~01:00);
//  - at the daily reset (a drop) the new day's opening value is added (elec opens at its standing charge, gas at 0);
//  - small dips are stale frames and ignored.
// Fallback: if no reset is seen by 03:35, the previous day had zero import, so "cost today" never dropped
// (e.g. 0.45 -> 0.45). The new day's standing charge is then added from the tariff.
// today      = year*1000 + day_of_year now (0 if the clock isn't set)
// today_soon = the same, 15 min from now, so a reset a few minutes before our midnight counts for the new day
// mins       = minutes since midnight (-1 if the clock isn't set)
// Returns the previous day's final "cost today" when a new day is registered (the reset or the 03:35 fallback),
// for the "cost yesterday" sensor; otherwise -1. Works whatever the day's use, including zero-import days.
static inline double trio_cost_total(double cost, double standing, double &prev, double &total,
                                     int &reset_day, int today, int today_soon, int mins) {
  double yesterday = -1;
  bool near_midnight = mins >= 0 && (mins >= 23 * 60 + 50 || mins < 15);
  if (prev < 0) { prev = cost; reset_day = today; return -1; }   // first reading ever: baseline only
  if (cost >= prev) { total += cost - prev; prev = cost; }
  else if (prev - cost > 0.1 || (near_midnight && prev - cost > 0.005)) {   // daily reset
    yesterday = prev;
    total += cost; prev = cost;
    if (today_soon > 0) reset_day = today_soon;
  }
  // else: small dip = stale frame, ignore
  if (today > 0) {
    if (reset_day <= 0) reset_day = today;                       // clock wasn't set at the baseline
    else if (reset_day != today && mins >= 3 * 60 + 35) {        // no reset seen: "cost today" never dropped
      yesterday = prev;
      total += standing; reset_day = today;
    }
  }
  return yesterday;
}

// ---- "Have we seen this kind of message before?" ---------------------------------------
// Signature strings for anything not in the known list get reported to HA.
static inline std::string trio_hex(const std::vector<uint8_t> &b, size_t max_bytes = 120) {
  std::string s; char t[4];
  for (size_t i = 0; i < b.size() && i < max_bytes; i++) { snprintf(t, sizeof t, "%02X", b[i]); s += t; }
  if (b.size() > max_bytes) s += "...";
  return s;
}
