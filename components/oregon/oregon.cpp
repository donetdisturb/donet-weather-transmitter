#include "oregon.h"
#include "esphome/core/log.h"
#include <cmath>
#include <cstdio>

#ifdef USE_OREGON_CC1101
#include <ELECHOUSE_CC1101_SRC_DRV.h>
#endif

namespace esphome {
namespace oregon {

static const char *const TAG = "oregon";

void OregonComponent::init_radio_() {
#ifdef USE_OREGON_CC1101
  if (this->radio_ == RADIO_CC1101) {
    // 433.92 MHz, OOK, async/raw (GDO0 = modulation input). Order matters: SPI pins
    // BEFORE Init(). After SetTx() the chip keys the carrier on the GDO0 level, so
    // Oregon_TM toggling GDO0 keys the OOK carrier on/off (same as the FS1000A data pin).
    ELECHOUSE_cc1101.setSpiPin(this->cc1101_sck_, this->cc1101_miso_,
                               this->cc1101_mosi_, this->cc1101_csn_);
    ELECHOUSE_cc1101.Init();
    ELECHOUSE_cc1101.setGDO0(this->tx_pin_);
    ELECHOUSE_cc1101.setCCMode(0);      // 0 = async serial (raw OOK via GDO0)
    ELECHOUSE_cc1101.setModulation(2);  // 2 = ASK/OOK
    ELECHOUSE_cc1101.setMHZ(433.92);
    ELECHOUSE_cc1101.setPA(12);         // +12 dBm
    ELECHOUSE_cc1101.SetTx();
    if (ELECHOUSE_cc1101.getCC1101()) {
      ESP_LOGI(TAG, "CC1101 OK: 433.92MHz OOK async, GDO0=GPIO%d, PA=12dBm", this->tx_pin_);
    } else {
      ESP_LOGE(TAG, "CC1101 NOT detected on SPI! Check wiring (SCK/MISO/MOSI/CSN/3V3).");
    }
    return;
  }
#endif
  // FS1000A: nothing to set up — the Oregon_TM constructor already did pinMode(tx_pin).
  ESP_LOGI(TAG, "FS1000A OOK on GPIO%d (bit-banged)", this->tx_pin_);
}

void OregonComponent::setup() {
  this->init_radio_();

  // ── Boot init: the ch1 transmitters — TH + clock + UV ──
  this->th_ = new Oregon_TM(this->tx_pin_, 26);
  this->clock_ = new Oregon_TM(this->tx_pin_, 26);
  this->uv_ = new Oregon_TM(this->tx_pin_, 26);

  // ── TH (RTGN318) ──
  this->th_->setType(RTGN318);
  this->th_->setChannel(this->channel_);
  // setChannel set send_time from the library (53000 for ch1); make the YAML knob
  // authoritative. Default th_interval is 53s, so it matches the library value exactly.
  this->th_->send_time = this->th_interval_;
  this->th_->setBatteryFlag(0);
  // Flag nibble 0x0B, HELD permanently (load-bearing on this base; flag=0 makes it
  // accept one packet then ignore updates).
  this->th_->SendBuffer[3] |= 0x0B;
  this->th_->buffer_size = 19;
  this->th_->setTemperature(20.0);
  this->th_->setHumidity(50);
  this->th_->setComfort(20.0, 50);
  this->th_->time_marker_send = 0xFFFFFFFF;

  // ── Clock (RTGR328N) ──
  this->clock_->setType(RTGR328N);
  this->clock_->setChannel(this->channel_);
  this->clock_->setBatteryFlag(0);
  this->clock_->SendBuffer[3] |= 0x0B;
  this->clock_->buffer_size = 25;
  this->clock_->time_marker_send = 0xFFFFFFFF;

  // ── UV (UVR128) ──  setChannel(0) sets send_time = 73000 (channel fixed internally)
  this->uv_->setType(UVR128);
  this->uv_->setChannel(0);
  this->uv_->send_time = this->uv_interval_;  // default 73s == library value
  this->uv_->setUV(this->uv_boot_);
  this->uv_->SendBuffer[5] = 0x00;  // "unknown" field — free, covered by the CRC
  this->uv_->buffer_size = 16;
  this->uv_->time_marker_send = 0xFFFFFFFF;

  // ── Additional channels 2-5 (empty unless declared) ──
  // Model is per channel (default RTGN318):
  //   RTGN318 -> TH + its OWN RTGR328N clock. Channel codes 0x20..0x50 are all distinct, so
  //              channels never collide on air; and because every RTGN channel carries a
  //              clock, the base never starves a clock-sync channel (no hijack/clock-stop).
  //   THGN132 -> TH-only, non-clock (channels 2-3 only). For 3-channel base stations.
  // Each transmitter mirrors the ch1 setup (flag 0x0B held, buffer_size 19, defaults).
  for (auto &ec : this->extra_) {
    bool is_thgn = (ec.model == EXTRA_THGN132);
    auto *t = new Oregon_TM(this->tx_pin_, 26);
    t->setType(is_thgn ? THGN132 : RTGN318);
    t->setChannel(ec.channel);
    if (ec.interval != 0)
      t->send_time = ec.interval;  // 0 = keep the library's per-channel/model default
    t->setBatteryFlag(0);
    t->SendBuffer[3] |= 0x0B;
    t->buffer_size = 19;
    t->setTemperature(20.0);
    t->setHumidity(50);
    t->setComfort(20.0, 50);
    t->time_marker_send = 0xFFFFFFFF;
    ec.th = t;

    if (!is_thgn) {  // RTGN318 carries its own clock on the same channel/ID
      auto *c = new Oregon_TM(this->tx_pin_, 26);
      c->setType(RTGR328N);
      c->setChannel(ec.channel);
      c->setBatteryFlag(0);
      c->SendBuffer[3] |= 0x0B;
      c->buffer_size = 25;
      c->time_marker_send = 0xFFFFFFFF;
      ec.clk = c;
    }
    ESP_LOGI(TAG, "Extra ch%d: %s ~%us%s", ec.channel,
             is_thgn ? "THGN132 (TH-only)" : "RTGN318 (TH+clock)",
             (unsigned) (t->send_time / 1000), ec.clk != nullptr ? " [clock-bearing]" : "");
  }

  this->ready_ = true;
  ESP_LOGI(TAG, "Init OK | ch%d RTGN318+clk ~%us | UV(boot=%d) ~%us | clock every %us, flag=0x0B",
           this->channel_, (unsigned) (this->th_interval_ / 1000), this->uv_boot_,
           (unsigned) (this->uv_interval_ / 1000), (unsigned) (this->clock_interval_ / 1000));
}

void OregonComponent::send_uv_() {
  // UV index driven by HA; the boot/fallback value is sent until the first HA push.
  if (this->uvs_ != nullptr && !std::isnan(this->uvs_->state)) {
    float v = this->uvs_->state;
    if (v > -1.0f && v < 25.0f)
      this->uv_->setUV((uint8_t) lroundf(v));
  }
  this->last_pair_uv_ = millis();
  this->uv_->SendPacket();
  uint8_t *b = this->uv_->SendBuffer;
  int uvi = (b[4] & 0x0f) * 10 + (b[4] >> 4);  // same decode as rtl_433
  ESP_LOGI(TAG, "UV TX (idx=%d, ~%us) RAW: %02X %02X %02X %02X %02X %02X %02X %02X",
           uvi, (unsigned) (this->uv_->send_time / 1000),
           b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7]);
}

OregonComponent::ExtraChannel *OregonComponent::find_extra_(uint8_t ch) {
  for (auto &ec : this->extra_)
    if (ec.channel == ch)
      return &ec;
  return nullptr;
}

void OregonComponent::add_extra_channel(uint8_t ch, ExtraModel model, uint32_t interval) {
  ExtraChannel ec;
  ec.channel = ch;
  ec.model = model;
  ec.interval = interval;
  this->extra_.push_back(ec);
}

void OregonComponent::set_extra_temp(uint8_t ch, sensor::Sensor *s) {
  auto *ec = this->find_extra_(ch);
  if (ec != nullptr)
    ec->temp = s;
}

void OregonComponent::set_extra_hum(uint8_t ch, sensor::Sensor *s) {
  auto *ec = this->find_extra_(ch);
  if (ec != nullptr)
    ec->hum = s;
}

bool OregonComponent::run_extra_channels_() {
  // One packet per call (loop() already enforces one send per tick). Scan channels in order;
  // for each, a clock QUEUED by its previous TH goes out first, then a due TH. Each TH is
  // self-paced and anchored BEFORE the send, so deferring it by a tick never adds drift.
  for (auto &ec : this->extra_) {
    if (ec.th == nullptr)
      continue;

    // A clock queued by this channel's last TH (RTGN318 channels): send it the next tick.
    if (ec.clk_pending) {
      ec.clk_pending = false;
      auto n = this->time_->now();
      if (ec.clk != nullptr && n.is_valid()) {
        ec.clk->setClock(n.hour, n.minute, n.second,
                         n.day_of_month, n.month, n.year, n.day_of_week % 7);
        ec.clk->SendPacket();
        ec.last_clock = millis();
        ESP_LOGI(TAG, "  CLK%d TX (ch%d)", ec.channel, ec.channel);
        return true;
      }
      // time not valid: drop this clock; last_clock untouched so the next TH re-queues it.
    }

    uint32_t now = millis();
    if (ec.last_pair != 0 && (now - ec.last_pair) < ec.th->send_time)
      continue;
    ec.last_pair = millis();  // anchor before the blocking send

    // Refresh TH fields from HA right before the send (range-guarded, like ch1).
    float t = (ec.temp != nullptr) ? ec.temp->state : NAN;
    float h = (ec.hum != nullptr) ? ec.hum->state : NAN;
    bool t_ok = !std::isnan(t) && t > -50.0f && t < 70.0f;
    bool h_ok = !std::isnan(h) && h >= 0.0f && h <= 100.0f;
    if (t_ok) ec.th->setTemperature(t);
    if (h_ok) ec.th->setHumidity((uint8_t) h);
    if (t_ok && h_ok) ec.th->setComfort(t, (uint8_t) h);
    ec.th->SendPacket();
    ESP_LOGI(TAG, "TH%d TX (ch%d, ~%us) T=%.1f H=%d", ec.channel,
             (ec.th->SendBuffer[2] & 0xF0) >> 4, (unsigned) (ec.th->send_time / 1000),
             t_ok ? t : NAN, h_ok ? (int) h : -1);

    // Queue this channel's clock for the NEXT tick (RTGN318 channels), decoupled by interval.
    if (ec.clk != nullptr) {
      bool clk_due = (ec.last_clock == 0) ||
                     ((millis() - ec.last_clock) >= this->clock_interval_);
      if (clk_due)
        ec.clk_pending = true;
    }
    return true;
  }
  return false;
}

bool OregonComponent::run_scheduler_() {
  const uint32_t TX_INTERVAL = this->th_->send_time;  // 53000 for ch1
  const uint32_t TH_CLOCK_GAP = this->gap_;           // 5s between TH and Clock
  uint32_t now = millis();

  // ── State 0: IDLE ──
  if (this->sched_state_ == 0) {
    bool time_for_th = (this->last_pair_ == 0) || ((now - this->last_pair_) >= TX_INTERVAL);
    if (!time_for_th)
      return false;

    // Anchor the cadence to the INTENDED instant BEFORE transmitting: the ~400ms
    // blocking SendPacket no longer adds to the period (real period ~53.0s + 250ms
    // jitter, not 54.0s sitting on the +/-1s window edge).
    this->last_pair_ = millis();

    // Refresh TH fields from HA right before the send (range-guarded).
    float t = (this->temp_ != nullptr) ? this->temp_->state : NAN;
    float h = (this->hum_ != nullptr) ? this->hum_->state : NAN;
    bool t_ok = !std::isnan(t) && t > -50.0f && t < 70.0f;
    bool h_ok = !std::isnan(h) && h >= 0.0f && h <= 100.0f;
    if (t_ok) this->th_->setTemperature(t);
    if (h_ok) this->th_->setHumidity((uint8_t) h);
    if (t_ok && h_ok) this->th_->setComfort(t, (uint8_t) h);

    this->th_->SendPacket();
    this->th_sent_ = millis();  // real TH send instant (for the clock gap)

    // Diagnostics (ch1).
    if (this->last_temp_diag_ != nullptr && t_ok) this->last_temp_diag_->publish_state(t);
    if (this->last_hum_diag_ != nullptr && h_ok) this->last_hum_diag_->publish_state(h);
    this->th_tx_count_++;
    if (this->th_count_diag_ != nullptr)
      this->th_count_diag_->publish_state((float) this->th_tx_count_);
    if (this->tx_status_diag_ != nullptr) this->tx_status_diag_->publish_state("TH OK");

    // Clock is DECOUPLED: only every clock_interval. An every-cycle clock accumulates
    // on the same ID and makes the BAR989HG stop updating temp/hum after ~6 packets.
    bool clock_due = (this->last_clock_ == 0) ||
                     ((millis() - this->last_clock_) >= this->clock_interval_);
    if (clock_due) {
      this->sched_state_ = 1;  // send the Clock TH_CLOCK_GAP after this TH
      ESP_LOGI(TAG, "TH TX | Clock in %us", (unsigned) (TH_CLOCK_GAP / 1000));
    } else {
      this->sched_state_ = 0;  // TH-only this cycle
      ESP_LOGI(TAG, "TH TX | TH-only");
    }
    return true;
  }

  // ── State 1: TH_SENT ──
  if (this->sched_state_ == 1) {
    bool gap_ok = (now - this->th_sent_) >= TH_CLOCK_GAP;
    if (!gap_ok)
      return false;

    auto sntp_now = this->time_->now();
    if (!sntp_now.is_valid()) {
      // Never transmit 00:00 / 00-00-00 (an impossible date the base may treat as bad
      // data and block the sensor). Don't touch last_clock_ so we retry next TH cycle.
      this->sched_state_ = 0;
      ESP_LOGW(TAG, "Clock skipped: time not valid yet (will retry next TH)");
      return false;
    }
    uint8_t dow = sntp_now.day_of_week % 7;
    this->clock_->setClock(sntp_now.hour, sntp_now.minute, sntp_now.second,
                           sntp_now.day_of_month, sntp_now.month, sntp_now.year, dow);
    this->clock_->SendPacket();
    this->last_clock_ = millis();  // space the next Clock by clock_interval
    this->sched_state_ = 0;        // TH cadence is anchored in state 0

    // Diagnostics (ch1).
    this->clock_tx_count_++;
    if (this->clock_count_diag_ != nullptr)
      this->clock_count_diag_->publish_state((float) this->clock_tx_count_);
    if (this->last_clock_diag_ != nullptr) {
      char buf[20];
      snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d", sntp_now.year,
               sntp_now.month, sntp_now.day_of_month, sntp_now.hour, sntp_now.minute,
               sntp_now.second);
      this->last_clock_diag_->publish_state(buf);
    }
    if (this->tx_status_diag_ != nullptr) this->tx_status_diag_->publish_state("TH+Clock OK");

    ESP_LOGI(TAG, "Clock TX %04d-%02d-%02d %02d:%02d:%02d | Next pair in ~%us",
             sntp_now.year, sntp_now.month, sntp_now.day_of_month,
             sntp_now.hour, sntp_now.minute, sntp_now.second,
             (unsigned) (TX_INTERVAL / 1000));
    return true;
  }

  return false;
}

void OregonComponent::loop() {
  if (!this->ready_)
    return;
  uint32_t now = millis();
  if (now - this->last_tick_ < 250)  // preserve the original 250ms scheduler granularity
    return;
  this->last_tick_ = now;

  // ONE SendPacket per tick. Priority order matters:
  //   1) ch1 (TH / clock) — the production channel; it must land in the base's +/-1s window,
  //      so it always gets the tick when due (never deferred behind another send).
  //   2) UV.
  //   3) extra channels 2-5 (and their queued clocks).
  // Stopping at the first send means a crowded moment — or boot, when everything is due at
  // once — spreads across ticks (max ~one send of blocking) instead of one long pass.
  if (this->run_scheduler_())
    return;

  if ((this->last_pair_uv_ == 0) ||
      ((millis() - this->last_pair_uv_) >= this->uv_->send_time)) {
    this->send_uv_();
    return;
  }

  this->run_extra_channels_();
}

void OregonComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Oregon TX:");
  ESP_LOGCONFIG(TAG, "  Radio: %s (GPIO%d)",
                this->radio_ == RADIO_CC1101 ? "CC1101" : "FS1000A", this->tx_pin_);
  ESP_LOGCONFIG(TAG, "  Channel: %d", this->channel_);
  ESP_LOGCONFIG(TAG, "  TH interval: %us | Clock interval: %us | TH->Clock gap: %us | UV: %us",
                (unsigned) (this->th_interval_ / 1000), (unsigned) (this->clock_interval_ / 1000),
                (unsigned) (this->gap_ / 1000), (unsigned) (this->uv_interval_ / 1000));
  for (auto &ec : this->extra_) {
    ESP_LOGCONFIG(TAG, "  Extra ch%d: %s ~%us%s", ec.channel,
                  ec.model == EXTRA_THGN132 ? "THGN132 (TH-only)" : "RTGN318 (TH+clock)",
                  (unsigned) ((ec.th != nullptr ? ec.th->send_time : ec.interval) / 1000),
                  ec.clk != nullptr ? " [clock-bearing]" : "");
  }
}

}  // namespace oregon
}  // namespace esphome
