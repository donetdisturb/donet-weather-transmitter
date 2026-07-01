# Donet Weather Transmitter

> **TL;DR** — An ESP32 (ESPHome) that emulates Oregon Scientific–compatible 433.92 MHz sensors to keep an
> old weather station alive, fed by Home Assistant data.

## Quick start
1. Copy `components/oregon/` into `/config/esphome/components/oregon/`.
2. `cp secrets.yaml.example secrets.yaml` → set Wi-Fi, the API/OTA keys, and your HA entity_ids.
3. Pick the radio and flash: `esphome run oregon-esp32-cc1101.yaml` (or `oregon-esp32-fs1000a.yaml`).
   On a NodeMCU/ESP8266 instead of an ESP32, use `oregon-esp8266-fs1000a.yaml` (budget build, no BLE).
4. Put the Oregon base into sensor-search mode (power-cycle it): it locks on in ~80 s, clock within ~10 min.

> **CC1101:** SCK→18 MISO→19 MOSI→23 CSN→5 GDO0→4, 3.3V. — **FS1000A:** DATA→GPIO5, 5V. Antenna ~17 cm.
> — **ESP8266 + FS1000A:** DATA→D1(GPIO5), VCC→VU(5V) or 3V3 (not VIN), GND→GND.

---

An ESP32 (or ESP8266) + ESPHome project that emulates Oregon Scientific wireless sensors, so a
modern data source (a SwitchBot meter, a Home Assistant value, anything) can keep an old Oregon
weather station alive and updating — instead of throwing it away.

On an ESP32 the firmware plays two roles at once (an ESP8266 does the RF half only — it has no BLE):

| Role | What it does |
|------|--------------|
| BLE proxy | Receives SwitchBot meter beacons over Bluetooth and forwards them to Home Assistant (`bluetooth_proxy`). |
| RF emulator | Reads values back from Home Assistant and re-transmits them on 433.92 MHz, emulating an Oregon-compatible temp/humidity sensor, the radio clock/date (RTGR328N) and a UV-index sensor (UVR128). Up to four extra temp/humidity channels are available as an optional feature — see below. |

```
SwitchBot  ──BLE──►  ESP32 (proxy)  ──WiFi──►  Home Assistant
                                                     │
Home Assistant  ──API──►  ESP32  ──433.92 MHz──►  Oregon base station
        (temp / humidity / clock / UV)            (e.g. BAR989HG)
```

> The source can be anything HA exposes — SwitchBot is just the example used here. Point the
> `!secret` entity_ids at whatever sensors you like.

---

## Why this exists

More than twenty years ago, my father bought this weather station. For two decades it has hung on
his wall as a small daily ritual — a glance at the temperature, the humidity, the time, every single
morning.

Then its outdoor sensors began to die, the way twenty-year-old electronics do. And here was the trap:
replacements were either nowhere to be found or priced as if they were carved from gold. The
"obvious" fix — just buy a brand-new station — was the one thing I refused to do. For an older person,
replacing a device you've trusted for half a lifetime isn't a convenience; it's an *upheaval*. New
buttons, new layout, new everything — all to read the same number you've always read, in the same spot
on the same wall.

So instead of throwing away a perfectly good display that he loves, I taught a tiny microcontroller to
speak its language. Now modern sensors (or any Home Assistant value) feed his old station over the
air, and the display he's known for twenty years simply *keeps working* — same wall, same glance, same
morning ritual.

This project was born from two stubborn convictions: that good hardware should never become e-waste,
and that an elderly man deserves to be spared the small trauma of change. If it keeps one more familiar
object alive, on one more wall, then it has done its job. 💛

---

## Disclaimer

> I am not a programmer. However, I firmly believe in repurposing old hardware in non-production
> environments because I stand strongly against technological e-waste. Please note that I was
> assisted by an AI in compiling and refining this project; therefore, everything here should be
> taken with caution and double-checked. The following information is provided for educational
> purposes only. I assume no responsibility for any hardware malfunctions, data loss, natural
> disasters, or any other damages that may occur from using this information. Use at your own risk.

> **Trademarks & affiliation.** *Oregon Scientific*, *SwitchBot*, *Home Assistant*, *ESPHome* and all
> product names, model numbers and logos are trademarks of their respective owners, used here only
> nominatively to describe interoperability. This is an independent, non-commercial hobby project: it
> is not affiliated with, authorized, sponsored or endorsed by Oregon Scientific or any other
> trademark holder. No manufacturer firmware, artwork or proprietary code is included — only an
> independently reverse-engineered, interoperable implementation of a publicly transmitted RF protocol.

> **Radio regulations.** Transmitting on 433.92 MHz is your responsibility. This band is license-free
> for short-range devices in many regions (e.g. the EU SRD band, subject to duty-cycle limits — the
> short bursts here stay well within them), but rules differ by country. Comply with your local RF
> regulations and power/duty-cycle limits.

---

## Features

- **Production default = one TH channel + clock + UV** → the base shows temperature, humidity, the
  radio clock and UV.
- **Optional extra sensors** — up to 4 more temp/humidity channels (`extra_channels:`), each an
  independent emulated Oregon sensor with its own channel, ID and transmit period. Off by default
  (shipped as a commented example); enable a channel by uncommenting its block + its source sensor.
- **Radio-controlled clock / date** (RTGR328N packet) — keeps the base station's clock display alive.
- **UV index** (UVR128) — including a reverse-engineered post-amble CRC (see below); one of the few
  public, complete emulations of an Oregon UV sensor end-to-end.
- **Two radio front-ends**: cheap FS1000A, or a CC1101 transceiver for range — selected by a single
  `radio:` line; the two builds are otherwise identical.
- **Runs on ESP32 or ESP8266**: the same component is platform-agnostic Arduino code. A NodeMCU
  (ESP8266) + FS1000A is a viable budget build (`oregon-esp8266-fs1000a.yaml`) — it just drops the
  BLE proxy (the ESP8266 has no Bluetooth), so HA must be fed from a separate bluetooth proxy.
  Validated under the full 5-channel stress test despite the single core.
- **Clean ESPHome external component** (`components/oregon/`): all the TX logic lives there, so the
  YAML is fully declarative — no inline lambdas, no copying loose `.cpp/.h` files.
- **BLE→HA bridge** (bluetooth_proxy) in the same firmware.
- **Timing that holds**: a 250 ms scheduler with pre-send cadence anchoring that survives the base's
  narrow ±1 s reception window (the *gap* fix), and a one-send-per-tick policy so a transmit never
  blocks. Validated over 12 h+ and under a full 5-channel + multi-clock stress test.

---

## Repository layout

```
.
├── oregon-esp32-cc1101.yaml    # Production — ESP32 + CC1101 transceiver (better range)
├── oregon-esp32-fs1000a.yaml   # Production — ESP32 + FS1000A module (cheapest)
├── oregon-esp8266-fs1000a.yaml # Production — ESP8266/NodeMCU + FS1000A (budget, no BLE)
├── components/
│   └── oregon/           # the external component: TX logic + vendored Oregon_TM library
├── secrets.yaml.example
├── LICENSE
└── README.md
```

The two ESP32 YAMLs are the same device (node name `oregon-all-in-one`) and differ only in their
radio layer — one `radio:` line (plus the TX pin and, for CC1101, the SPI pins). All the transmit
logic lives in `components/oregon/`, so the YAML is declarative. Pick one file.

* **`oregon-esp32-fs1000a.yaml`** — cheapest path. A bare FS1000A ASK/OOK module. Fine within a room or two.
* **`oregon-esp32-cc1101.yaml`** — a CC1101 sub-GHz transceiver driven in async/raw OOK mode. Crystal-PLL
  frequency accuracy and a controllable power amplifier give noticeably better range through walls.
* **`oregon-esp8266-fs1000a.yaml`** — the budget build on a NodeMCU (ESP8266) instead of an ESP32.
  Same component, distinct node name (`oregon-esp8266`); it drops the BLE proxy (no Bluetooth on the
  ESP8266), so feed Home Assistant from a separate bluetooth proxy on your network.

---

## The emulated sensors (channel model)

Channel 1 is always the master: temp/humidity (`RTGN318`) + the radio clock (`RTGR328N`). Extra
sensors (channels 2-5) are configured under `extra_channels:` and pick a `model:`:

> **`rtgn318` (default)** — temp/hum + its own RTGR328N clock. Channel codes are distinct
> (0x20/0x30/0x40/0x50), so channels never collide on air; and because every channel carries a clock,
> the base can never "starve" a clock-sync channel (see *The clock-channel hijack*). Supports
> channels 2-5 — the default for a 5-channel base like the BAR989HG.
>
> **`thgn132`** — plain (non-clock) temp/hum, channels 2-3 only. For 3-channel base stations, or
> when you want clock-sync guaranteed on ch1 alone.

| Channel | Default model | Type code | Period | Clock | Notes |
|--------:|---------------|:---------:|:------:|:-----:|-------|
| **1** | RTGN318 | `0xDCC3` | ~53 s | RTGR328N | Master — temp + hum + clock (always on) |
| 2–5 | RTGN318 | `0xDCC3` | ~59/61/67/71 s | each its own | Optional extra sensors (`extra_channels:`) |
| 2–3 | THGN132 *(opt.)* | `0x1D20` | ~41/43 s | — | Non-clock alternative for 3-channel bases |
| UV | UVR128 | `0xEC70` | ~73 s | — | UV index 0–25 (always on) |

Per-channel periods are distinct primes (53/59/61/67/71/73 s) so near-simultaneous transmissions
are rare (see *Scheduler & timing*). A config-time guard rejects model mixes that would collide on
air (notably `thgn132`-ch3 and `rtgn318`-ch4 both map to channel code 0x40).

> **What the shipped files contain:** the production configuration — channel 1 (temp + humidity +
> clock) and the UV sensor active; the base displays temp, humidity, clock and UV. The
> `extra_channels:` block (and its Home Assistant source sensors) ship commented out as an
> example. To enable an extra sensor, uncomment its `extra_channels` entry and its matching
> source block in `sensor:` (a matched pair), then add the channel's keys to `secrets.yaml`.

---

## The Oregon protocol in brief

- **Modulation:** OOK/ASK at 433.92 MHz, Manchester-coded. Protocol v2.1 here (each data bit is
  sent doubled, and the whole frame is transmitted twice).
- **Frame:** preamble + sync nibble `0xA` + payload `[type ×4 nibbles][channel][id ×2][flags]
  [data…]` + checksum + (for some models) a post-amble CRC.
- **Checksum:** 8-bit sum of the data nibbles, stored with nibbles swapped.
- **`RTGN318` (0xDCC3)** is the temp/hum packet of the RTGR328N clock-station family; **`RTGR328N`
  (0x8CE3)** is its clock/date packet (seconds…year, BCD — an encoding confirmed byte-for-byte
  against real captured Oregon clock frames in the rtl_433 corpus). **`THGN132` (0x1D20)** is a plain
  thermo-hygrometer (no clock association). **`UVR128` (0xEC70)** carries a single UV index.
- **Flag nibble `0x0B`** is held permanently on every TH/clock packet — empirically load-bearing
  on the BAR989HG (see *The gap*).

The transmission library, `Oregon_TM`, is derived from invandy's OREGON_NR (see Credits). Our
additions: the RTGR328N clock packet, the UVR128 UV packet + its CRC, ESP32 timing hardening
(`IRAM_ATTR`), and exposure of the timing internals to the ESPHome scheduler.

---

## The UV sensor (UVR128) — a reverse-engineering story

Oregon UV sensors report the standard WHO/WMO UV Index (0–2 low … 11+ extreme) as an integer —
the same scale used in public forecasts, so a decimal HA value just needs rounding to the nearest
integer before transmission.

The hard part was the frame's trailing byte. The working 8-byte frame is:

```
EC 70 16 90 [UV] [b5] [checksum] [CRC]
            └ byte4 = UV index (BCD-inverted)
                 └ byte5 = "unknown"/rolling field (free; covered by the CRC)
                      └ byte6 = checksum (swapped sum of nibbles 0-5)
                           └ byte7 = post-amble CRC
```

`byte7` is a CRC-8-CCITT, polynomial `0x07`, fed each nibble MSB-first over nibbles 0–11
(bytes 0–5), stored LS-nibble-first (swapped), with a sensor-specific init value of `0xCB`.
That init was found by sweeping 0–255 against real captures and verifying the generator reproduces
the reference frames byte-for-byte.

Method (reusable for other sensors): with no rtl_433/Python available, we wrote a PowerShell
OOK/Manchester demodulator for the real `.cu8` IQ captures in the `rtl_433_tests` corpus —
magnitude → threshold → half-bit slicing → Manchester pair-decode → decimate ×2 (the v2.1
bit-doubling step) → nibbles. Then brute-forced the CRC init, always verifying by
full-frame reconstruction. Confirmed on hardware: the BAR989HG displays an arbitrary UV index
driven from Home Assistant.

---

## Hardware & wiring

You need an ESP32 (or ESP8266) dev board, a 433.92 MHz transmitter, and an Oregon base station.
Tested against an Oregon BAR989HG. Other Oregon bases may behave differently.

### Build A — FS1000A (`oregon-esp32-fs1000a.yaml`)
| FS1000A | ESP32 |
|---------|-------|
| DATA    | GPIO5 |
| VCC     | 5V (stronger output than 3.3V) |
| GND     | GND |

Antenna: a ~17 cm straight wire on the module's ANT pad.

### Build B — CC1101 (`oregon-esp32-cc1101.yaml`)
| CC1101 | ESP32 |
|--------|-------|
| SCK    | GPIO18 |
| MISO   | GPIO19 |
| MOSI   | GPIO23 |
| CSN    | GPIO5  |
| GDO0   | GPIO4  (OOK modulation input) |
| VCC    | **3.3V** (not 5V) |
| GND    | GND |

Antenna: ~17 cm wire (or a proper 433 MHz antenna) on the ANT pad. `GDO2` is unused.

### Build C — ESP8266 / NodeMCU v3 + FS1000A (`oregon-esp8266-fs1000a.yaml`)
The budget build. A NodeMCU (LoLin v3 / ESP-12E) replaces the ESP32; no Bluetooth, so HA is fed by a
separate bluetooth proxy.

| FS1000A | NodeMCU v3 |
|---------|------------|
| DATA    | D1 (GPIO5) |
| VCC     | **VU** (USB 5V) for full range, or **3V3** (works, reduced range) |
| GND     | GND |

> **Do not power the FS1000A from `VIN`.** On the LoLin v3 `VIN` is a power-*input* and delivers no
> usable 5V from USB — the module stays under-powered and the base hears nothing. Use `VU` or `3V3`.
> Avoid the strapping pins (GPIO0/2/15) for DATA. Antenna: ~17 cm wire.

> If the build fails with *"section .text will not fit in region iram1"*, the `IRAM_ATTR` TX
> routines overflow the ESP8266's small IRAM — drop `IRAM_ATTR` on ESP8266 (they move to flash).
> (Not needed in our builds, but worth knowing.)

---

## Setup

1. **The component.** Copy the whole `components/oregon/` folder into your ESPHome config dir
   (`/config/esphome/components/oregon/`). It carries the TX logic and the vendored `Oregon_TM`
   library — no more copying loose `.cpp/.h` files. The YAML's `external_components:` finds it.

2. **Secrets.** Copy the example and fill in your values:
   ```bash
   cp secrets.yaml.example secrets.yaml
   ```
   Set your Wi-Fi, the API/OTA keys, and point the `oregon_all_in_one__sensor-temp-1` /
   `..._sensor-hum-1` keys at real Home Assistant entity_ids (and `..._sensor-uv` at a UV-index
   entity, if you have one). Those three are all the production config needs. `secrets.yaml` is
   git-ignored.

3. *(Optional)* **Extra sensors.** To add channels 2-5, uncomment their `extra_channels` entries and
   the matching `sensor:` source blocks (in matched pairs) and add the `..._sensor-temp/hum-2…5`
   keys. To exercise them without real hardware, point those secrets at any changing HA entities
   (e.g. a `template:` snippet generating fake temp/hum).

4. **Flash.** Pick the file for your radio:
   ```bash
   esphome run oregon-esp32-cc1101.yaml      # or oregon-esp32-fs1000a.yaml
   ```

5. **Pair on the base.** Put the Oregon base into sensor-search mode (power-cycle it) while the
   board is transmitting. It locks onto the channels and starts showing values. The clock syncs
   within ~10 minutes. **Re-pairing tip:** if you power-cycle *only the ESP*, its phase restarts
   and may fall outside the base's learned window — just put the base back into search mode.

---

## The "gap" — why the timing is what it is

The original engineering story. Early on, the base would receive ~6–27 packets fine, then freeze
(temp/humidity stop updating while the ESP keeps transmitting), then revive minutes later. We called
it *the gap*.

It was not the clock packet, and not the data — it was timing. Oregon bases don't listen
continuously; they open a narrow reception window phase-locked to the sensor's expected schedule
(≈53 s for an RTGN318 on channel 1, tolerance ±1 s). Our transmit period had drifted to 54.0 s
— right on the +1 s edge of that window. Each cycle added +1 s of phase, and after ~20-odd cycles we
walked clean out of the window → freeze; drifted back in → revive.

The fix (in the YAML scheduler, not the library):
* run the scheduler at 250 ms granularity instead of 1 s,
* target an exact 53 000 ms period,
* anchor the cadence timestamp before the (~400 ms blocking) transmit call, not after.

Net real period ≈ 53.2 s, centered in the window. Stable over 12 h+ with no freeze.

The flag nibble `0x0B` is the other empirical quirk: held permanently, because with flag `0` the
BAR989HG accepts one packet and then ignores all updates.

---

## The clock-channel hijack

When we added a second channel, the clock stopped syncing — and the base's clock-sync channel
indicator had jumped to "2". The cause: the base auto-selects which sensor *channel* it syncs its
RF clock from, and it picks a *clock-family* model. `RTGN318` is exactly that (it's the RTGR328N's
TH packet), so a second `RTGN318` on channel 2 made the base chase the clock on a channel where we
sent only temp/hum — so it never synced.

Two ways to avoid it, both supported by `extra_channels`:
* **`thgn132` slaves** — a plain (non-clock-family) v2.1 thermo-hygrometer; with THGN132 on ch2+ the
  base keeps clock-sync on ch1. Limited to channels 2-3.
* **`rtgn318` slaves (the default)** — each extra channel carries *its own* clock, so whichever
  channel the base picks for sync, a valid clock is always there. A stress test (ch1 + ch4 + ch5 all
  carrying their own clock) confirmed the base picks *one* channel semi-arbitrarily (it chose ch4),
  and sync holds fine — and since every clock is NTP-derived they all agree. This is what makes a
  clean 5-channel setup possible.

---

## Scheduler & timing

The component's `loop()` runs on a 250 ms tick. Each transmitter is self-paced at its own period
and anchors its timestamp before transmitting, so a blocking ~400 ms send never accumulates drift
— every sensor re-centers on the next cycle. To keep any single tick short, the loop sends at most
one packet per tick (ch1 has priority, so its ±1 s window is always protected); when several
sensors are due together they spread across consecutive ticks instead of one long blocking burst.
Per-channel periods are distinct primes, so coincidences are rare anyway.

The clock is decoupled and sent only every `clock_interval` (default 10 min), a few seconds after
a TH packet. Sending it every cycle on the same ID makes the base stop updating temp/hum after ~6
packets; spacing it leaves clean TH-only windows. A simulated 5-channel + multi-clock load over
~44 min showed zero out-of-window events; ch1 stayed at 53.0–53.2 s throughout, and the
one-send-per-tick policy keeps even boot (when every sensor is first due) from blocking.

---

## Known limitations & quirks

- **THGN132 caps at 3 channels.** Its channel nibble is one-hot (`0x10`/`0x20`/`0x40`), so it can't
  go past ch3 — which is why the default extra-sensor model is `RTGN318` (linear channels 1-5, each
  carrying its own clock). Use `model: thgn132` only on a 3-channel base. There is no v2.1, non-clock,
  5-channel temp/hum model (`THGR810` is protocol v3.0, which this base doesn't use), so 5 clean
  channels means accepting that channels 2-5 each transmit their own (NTP-derived) clock.
- **Clock weekday nibble is cosmetic (and ignored).** The day-of-week sits in the low nibble of the
  clock packet's month byte, but the BAR989HG — like the rtl_433 reference decoder, which leaves it
  commented out — ignores it and recomputes the weekday from the date. Confirmed on hardware: the
  displayed day is correct regardless of what that nibble holds, so the `day_of_week % 7` value we
  send is inconsequential. Time, date and weekday all display correctly.
- **Timezone & DST are host-side, never over-RF.** The clock packet carries only local-time digits —
  no GMT offset — and the packet's separate "DST/timezone" flag nibble is ignored by the base
  (verified by setting it and observing no change). The ESP32 transmits fully-resolved local time
  from SNTP via the POSIX `timezone:` string (DST switches automatically), so leave the base's
  manual ZONE offset at 0 to avoid a double offset.
- **No real battery.** The "low battery" flag is forced to OK (the ESP is mains-powered). It could
  be repurposed as a health indicator (e.g. source-sensor battery passthrough, or a stale-data
  alarm) — a possible future feature.
- **v2.1 only / BAR989HG-tested.** Other Oregon bases may use different windows, flags, or model
  support.

---

## Credits & License

The RF heavy lifting — the `Oregon_TM` transmission library — is derived from the
[OREGON_NR library by Sergey Zawislak (invandy)](https://github.com/invandy/Oregon_NR), used and
modified under its MIT license. Our additions: the RTGR328N clock/date packet, the UVR128 UV
packet with its reverse-engineered CRC, multi-channel emulation with the THGN132 clock-channel fix,
ESP32 timing hardening (`IRAM_ATTR`, buffer zeroing), and exposure of timing internals to ESPHome.

Reverse-engineering relied on the IQ capture corpus in
[merbanan/rtl_433_tests](https://github.com/merbanan/rtl_433_tests) (used only as a cross-check; no
captures are redistributed here, and no rtl_433 decoder code was copied — the demodulator and CRC
recovery are an independent clean-room implementation).

The CC1101 build pulls in the
[SmartRC-CC1101-Driver-Lib by LSatan](https://github.com/LSatan/SmartRC-CC1101-Driver-Lib) at build
time (referenced by PlatformIO, not bundled or redistributed in this repo); it is used under its
own license.

This project is released under the [MIT License](LICENSE).
