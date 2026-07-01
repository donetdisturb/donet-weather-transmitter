"""Oregon Scientific v2.1 TX external component for ESPHome.

Emulates Oregon weather sensors (RTGN318 temp/humidity + RTGR328N clock + UVR128 UV)
toward an Oregon base station, on either an FS1000A OOK module or a CC1101 transceiver.

The component owns the whole TX scheduler: the ~53.2s TH cadence (anchored before
SendPacket), the decoupled clock and the ~73s UV cadence. The YAML stays declarative,
and the FS1000A and CC1101 builds differ only by `radio:`.

Timing is empirical on the BAR989HG base (validated in production).
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, text_sensor, time
from esphome.const import CONF_ID, CONF_CHANNEL, CONF_TEMPERATURE, CONF_HUMIDITY

CODEOWNERS = ["@donetdisturb"]
DEPENDENCIES = ["time"]
# Pulled in only so the optional diagnostic sub-entities below can be created.
AUTO_LOAD = ["sensor", "text_sensor"]

oregon_ns = cg.esphome_ns.namespace("oregon")
OregonComponent = oregon_ns.class_("OregonComponent", cg.Component)

RadioType = oregon_ns.enum("RadioType")
RADIOS = {
    "fs1000a": RadioType.RADIO_FS1000A,
    "cc1101": RadioType.RADIO_CC1101,
}

# Model for an extra channel. rtgn318 (default) = TH + its own RTGR328N clock, channel codes
# 0x20..0x50 (channels 2-5), no on-air collisions -> the clean default for a 5-channel base.
# thgn132 = TH-only, non-clock, channels 2-3 only -> for 3-channel stations / clock-safe slaves.
ExtraModel = oregon_ns.enum("ExtraModel")
EXTRA_MODELS = {
    "rtgn318": ExtraModel.EXTRA_RTGN318,
    "thgn132": ExtraModel.EXTRA_THGN132,
}

CONF_RADIO = "radio"
CONF_TX_PIN = "tx_pin"
CONF_TIME_ID = "time_id"
CONF_TH_INTERVAL = "th_interval"
CONF_CLOCK_INTERVAL = "clock_interval"
CONF_TH_CLOCK_GAP = "th_clock_gap"
CONF_UV_INTERVAL = "uv_interval"
CONF_UV = "uv"
CONF_UV_BOOT = "uv_boot"

# Optional additional channels 2-5 (declarative list) — a real end-user feature: emulate up
# to 4 extra TH sensors. Each picks a `model:` (default rtgn318). `interval:` is optional;
# omit it to use the library's natural per-channel/model cadence.
CONF_EXTRA_CHANNELS = "extra_channels"
CONF_INTERVAL = "interval"
CONF_MODEL = "model"

# Optional diagnostic entities (ch1 only). Each is created only if its key is present in the
# YAML; the component publishes to it at each ch1 TH / clock send.
CONF_LAST_SENT_TEMPERATURE = "last_sent_temperature"
CONF_LAST_SENT_HUMIDITY = "last_sent_humidity"
CONF_TH_TX_COUNT = "th_tx_count"
CONF_CLOCK_TX_COUNT = "clock_tx_count"
CONF_LAST_SENT_CLOCK = "last_sent_clock"
CONF_TX_STATUS = "tx_status"

# CC1101 SPI pins (ESP32 VSPI defaults), only used when radio: cc1101
CONF_CC1101_SCK = "cc1101_sck"
CONF_CC1101_MISO = "cc1101_miso"
CONF_CC1101_MOSI = "cc1101_mosi"
CONF_CC1101_CSN = "cc1101_csn"

# On-air channel code (high nibble of byte 2) per model+channel, from Oregon_TM::setChannel.
# Used only to catch configs where two channels would collide on the same code (the base
# would merge them). THGN is the 1/2/4 family; RTGN is linear 1..5.
_THGN_CODE = {2: 0x20, 3: 0x40}
_RTGN_CODE = {2: 0x20, 3: 0x30, 4: 0x40, 5: 0x50}


def _validate_extra_channel(conf):
    if conf[CONF_MODEL] == "thgn132" and conf[CONF_CHANNEL] not in (2, 3):
        raise cv.Invalid(
            "model 'thgn132' supports only channels 2-3 (THGN is a 3-channel family); "
            "use 'rtgn318' for channels 4-5"
        )
    return conf


EXTRA_CHANNEL_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_CHANNEL): cv.int_range(min=2, max=5),
            cv.Optional(CONF_MODEL, default="rtgn318"): cv.one_of(*EXTRA_MODELS, lower=True),
            cv.Optional(CONF_INTERVAL): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_TEMPERATURE): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_HUMIDITY): cv.use_id(sensor.Sensor),
        }
    ),
    _validate_extra_channel,
)


def _validate_channels(value):
    seen = set()
    codes = {}  # on-air channel code -> (channel, model)
    for ch in value:
        n = ch[CONF_CHANNEL]
        if n in seen:
            raise cv.Invalid(f"Duplicate extra channel {n}")
        seen.add(n)
        code = (_THGN_CODE if ch[CONF_MODEL] == "thgn132" else _RTGN_CODE)[n]
        if code in codes:
            on, om = codes[code]
            raise cv.Invalid(
                f"channel {n} ({ch[CONF_MODEL]}) and channel {on} ({om}) both map to on-air "
                f"channel code 0x{code:02X} -> the base would merge their data. Keep one model "
                f"family, or avoid the thgn132-ch3 + rtgn318-ch4 combination."
            )
        codes[code] = (n, ch[CONF_MODEL])
    return value


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(OregonComponent),
        cv.Required(CONF_RADIO): cv.enum(RADIOS, lower=True),
        cv.Required(CONF_TX_PIN): cv.int_,
        cv.Required(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
        cv.Optional(CONF_CHANNEL, default=1): cv.int_range(min=1, max=5),
        cv.Optional(CONF_TH_INTERVAL, default="53s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_CLOCK_INTERVAL, default="10min"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_TH_CLOCK_GAP, default="5s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_UV_INTERVAL, default="73s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_UV_BOOT, default=5): cv.int_range(min=0, max=25),
        cv.Optional(CONF_TEMPERATURE): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_HUMIDITY): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_UV): cv.use_id(sensor.Sensor),
        # Additional channels 2-5 (list); per-channel model (default rtgn318) + collision guard.
        cv.Optional(CONF_EXTRA_CHANNELS): cv.All(
            cv.ensure_list(EXTRA_CHANNEL_SCHEMA), _validate_channels
        ),
        # Optional diagnostic entities (ch1).
        cv.Optional(CONF_LAST_SENT_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement="°C", accuracy_decimals=1, icon="mdi:thermometer"
        ),
        cv.Optional(CONF_LAST_SENT_HUMIDITY): sensor.sensor_schema(
            unit_of_measurement="%", accuracy_decimals=0, icon="mdi:water-percent"
        ),
        cv.Optional(CONF_TH_TX_COUNT): sensor.sensor_schema(
            accuracy_decimals=0, icon="mdi:counter"
        ),
        cv.Optional(CONF_CLOCK_TX_COUNT): sensor.sensor_schema(
            accuracy_decimals=0, icon="mdi:counter"
        ),
        cv.Optional(CONF_LAST_SENT_CLOCK): text_sensor.text_sensor_schema(
            icon="mdi:clock-outline"
        ),
        cv.Optional(CONF_TX_STATUS): text_sensor.text_sensor_schema(
            icon="mdi:radio-tower"
        ),
        # CC1101 SPI pins (ignored for fs1000a)
        cv.Optional(CONF_CC1101_SCK, default=18): cv.int_,
        cv.Optional(CONF_CC1101_MISO, default=19): cv.int_,
        cv.Optional(CONF_CC1101_MOSI, default=23): cv.int_,
        cv.Optional(CONF_CC1101_CSN, default=5): cv.int_,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_radio(config[CONF_RADIO]))
    cg.add(var.set_tx_pin(config[CONF_TX_PIN]))
    cg.add(var.set_time(await cg.get_variable(config[CONF_TIME_ID])))
    cg.add(var.set_channel(config[CONF_CHANNEL]))
    cg.add(var.set_th_interval(config[CONF_TH_INTERVAL]))
    cg.add(var.set_clock_interval(config[CONF_CLOCK_INTERVAL]))
    cg.add(var.set_th_clock_gap(config[CONF_TH_CLOCK_GAP]))
    cg.add(var.set_uv_interval(config[CONF_UV_INTERVAL]))
    cg.add(var.set_uv_boot(config[CONF_UV_BOOT]))

    cg.add(var.set_cc1101_pins(
        config[CONF_CC1101_SCK], config[CONF_CC1101_MISO],
        config[CONF_CC1101_MOSI], config[CONF_CC1101_CSN],
    ))

    if CONF_TEMPERATURE in config:
        cg.add(var.set_temp_sensor(await cg.get_variable(config[CONF_TEMPERATURE])))
    if CONF_HUMIDITY in config:
        cg.add(var.set_hum_sensor(await cg.get_variable(config[CONF_HUMIDITY])))
    if CONF_UV in config:
        cg.add(var.set_uv_sensor(await cg.get_variable(config[CONF_UV])))

    for ch in config.get(CONF_EXTRA_CHANNELS, []):
        n = ch[CONF_CHANNEL]
        # interval 0 -> C++ keeps the library's natural per-channel/model send_time.
        interval = ch.get(CONF_INTERVAL, 0)
        cg.add(var.add_extra_channel(n, EXTRA_MODELS[ch[CONF_MODEL]], interval))
        if CONF_TEMPERATURE in ch:
            cg.add(var.set_extra_temp(n, await cg.get_variable(ch[CONF_TEMPERATURE])))
        if CONF_HUMIDITY in ch:
            cg.add(var.set_extra_hum(n, await cg.get_variable(ch[CONF_HUMIDITY])))

    # Optional diagnostic entities (ch1)
    if CONF_LAST_SENT_TEMPERATURE in config:
        cg.add(var.set_last_temp_diag(await sensor.new_sensor(config[CONF_LAST_SENT_TEMPERATURE])))
    if CONF_LAST_SENT_HUMIDITY in config:
        cg.add(var.set_last_hum_diag(await sensor.new_sensor(config[CONF_LAST_SENT_HUMIDITY])))
    if CONF_TH_TX_COUNT in config:
        cg.add(var.set_th_count_diag(await sensor.new_sensor(config[CONF_TH_TX_COUNT])))
    if CONF_CLOCK_TX_COUNT in config:
        cg.add(var.set_clock_count_diag(await sensor.new_sensor(config[CONF_CLOCK_TX_COUNT])))
    if CONF_LAST_SENT_CLOCK in config:
        cg.add(var.set_last_clock_diag(await text_sensor.new_text_sensor(config[CONF_LAST_SENT_CLOCK])))
    if CONF_TX_STATUS in config:
        cg.add(var.set_tx_status_diag(await text_sensor.new_text_sensor(config[CONF_TX_STATUS])))

    # The CC1101 radio driver is only pulled in (and its code compiled) when needed.
    if config[CONF_RADIO] == "cc1101":
        cg.add_define("USE_OREGON_CC1101")
        cg.add_library("SPI", None)
        # If the registry name fails to resolve, use the git URL instead:
        #   "https://github.com/LSatan/SmartRC-CC1101-Driver-Lib.git"
        cg.add_library("SmartRC-CC1101-Driver-Lib", None)
