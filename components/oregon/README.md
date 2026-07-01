# `oregon` — ESPHome external component

Turns an ESP32 (or ESP8266) + a 433.92 MHz transmitter (CC1101 or FS1000A) into an
emulator of Oregon Scientific weather sensors. It speaks the Oregon v2.1 protocol toward a
compatible base station (e.g. BAR989HG):

- ch1 = temperature/humidity (~53.2s) + clock (every 10min) + UV index (~73s)
- optionally ch2–5 = up to four more temperature/humidity sensors, each on its own channel

The TX logic — the drift-free ~53.2s cadence (anchored before each send), the decoupled clock,
the UV cadence, the one-send-per-tick scheduler — lives entirely in the component, so the YAML
stays fully declarative. The CC1101 and FS1000A builds collapse into one config that differs
only by `radio:`.

Validated in production on a BAR989HG base (ESP32, both radios) and stress-tested on ESP8266
(5 channels, multi-hour uptime).

## Files

| File | Role |
|------|------|
| `__init__.py` | YAML config schema + code-gen (the Python part) |
| `oregon.h` / `oregon.cpp` | `OregonComponent` — `setup()` = boot init, `loop()` = scheduler |
| `Oregon_TM.{h,cpp}` | the TX library (derived from invandy's OREGON_NR), vendored here |

## Usage

Drop the whole `oregon/` folder into a `components/` directory next to your YAML, then:

```yaml
external_components:
  - source:
      type: local
      path: components       # folder that CONTAINS the oregon/ dir
    components: [oregon]

time:
  - platform: sntp
    id: sntp_time

sensor:
  - platform: homeassistant
    entity_id: !secret oregon_all_in_one__sensor-temp-1
    id: ha_temperature
    internal: true
  - platform: homeassistant
    entity_id: !secret oregon_all_in_one__sensor-hum-1
    id: ha_humidity
    internal: true
  - platform: homeassistant
    entity_id: !secret oregon_all_in_one__sensor-uv
    id: ha_uv
    internal: true

oregon:
  id: oregon_tx
  radio: cc1101          # or: fs1000a
  tx_pin: 4              # GDO0 (cc1101) / data pin (fs1000a)
  time_id: sntp_time
  channel: 1
  th_interval: 53s       # default — matches the library's ch1 send_time exactly
  clock_interval: 10min
  th_clock_gap: 5s
  uv_interval: 73s
  uv_boot: 5             # UV index sent until HA pushes the real value
  temperature: ha_temperature
  humidity: ha_humidity
  uv: ha_uv
  # CC1101 SPI pins (ignored for fs1000a; ESP32 VSPI defaults shown):
  cc1101_sck: 18
  cc1101_miso: 19
  cc1101_mosi: 23
  cc1101_csn: 5
```

For an `fs1000a` build, just set `radio: fs1000a` and `tx_pin: 5` (no SPI pins needed).

Ready-to-use configs for all three hardware variants live at the repo root:
`oregon-esp32-cc1101.yaml`, `oregon-esp32-fs1000a.yaml`, `oregon-esp8266-fs1000a.yaml`.

## Diagnostic entities (ch1, optional)

Optional keys, each creating a sensor only if declared (ch1 only):

```yaml
oregon:
  # ...
  last_sent_temperature: { name: "Last Sent Temperature" }   # sensor °C
  last_sent_humidity:    { name: "Last Sent Humidity" }      # sensor %
  th_tx_count:           { name: "Temperature TX Count" }    # sensor
  clock_tx_count:        { name: "Clock TX Count" }          # sensor
  last_sent_clock:       { name: "Last Sent Clock" }          # text_sensor
  tx_status:             { name: "TX Status" }                # text_sensor ("TH OK" / "TH+Clock OK")
```

The generic `wifi_info` (IP/SSID) and `restart` button are orthogonal — keep them in the YAML.

## Additional sensors 2–5 (`extra_channels:`)

Emulate up to 4 extra TH sensors, each on its own channel. Pick a `model:` per channel
(default `rtgn318`):

| model | clock | channels | on-air channel code | notes |
|---|---|---|---|---|
| `rtgn318` (default) | yes (own RTGR328N) | 2–5 | 0x20 / 0x30 / 0x40 / 0x50 | distinct codes ⇒ no collision; every channel carries a clock so the base never starves a clock-sync channel. The clean default for a 5-channel base (e.g. BAR989HG). |
| `thgn132` | no (TH-only) | 2–3 only | 0x20 / 0x40 | THGN is a 3-channel family (codes 1/2/**4**). For 3-channel stations / clock-safe slaves. |

```yaml
oregon:
  # ...ch1 config...
  extra_channels:
    - channel: 2                 # model omitted -> rtgn318 (+ own clock)
      temperature: ha_temperature_2
      humidity: ha_humidity_2
    - channel: 3
      model: thgn132             # opt-in: TH-only, for a 3-channel base
      temperature: ha_temperature_3
      humidity: ha_humidity_3
    - channel: 4
      interval: 67s              # optional; omit to use the library's natural cadence
      temperature: ha_temperature_4
      humidity: ha_humidity_4
```

Each channel is self-paced and anchored before its send (the same drift-free cadence fix as
ch1). Collision guard: the config validator rejects two channels that map to the same
on-air code — notably `thgn132`-ch3 and `rtgn318`-ch4 (both 0x40) — so a misconfig can't
silently merge two sensors' data on the base.

One SendPacket per tick: `loop()` transmits at most one packet per 250 ms tick, ch1
first. So when several channels (or boot, when everything is due at once) coincide, the sends
spread across ticks instead of one long blocking pass — no `took a long time` warnings, and
boot never blocks more than a single send. Cadences are unchanged (a one-tick deferral is
absorbed by the anchor-before-send), and ch1 is never deferred (it has priority).

## Quick test

1. Point a YAML at this component (`external_components` block above).
2. `esphome config <file>.yaml` — validates the Python schema.
3. `esphome compile <file>.yaml` — builds (where the CC1101 lib / Arduino types are checked).
4. Flash and watch the base: TH every ~53.2s, clock every 10min, UV ~73s, and the temp/hum
   readings keep updating.
