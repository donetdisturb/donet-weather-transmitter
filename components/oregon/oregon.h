#pragma once

#include <vector>

#include "esphome/core/defines.h"  // for USE_OREGON_CC1101 (added by code-gen)
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/time/real_time_clock.h"

#include "Oregon_TM.h"

namespace esphome {
namespace oregon {

enum RadioType { RADIO_FS1000A = 0, RADIO_CC1101 = 1 };

// Model for an extra channel: RTGN318 (TH + own RTGR328N clock, channels 2-5, collision-free)
// or THGN132 (TH-only, non-clock, channels 2-3, for 3-channel stations).
enum ExtraModel { EXTRA_RTGN318 = 0, EXTRA_THGN132 = 1 };

// Emulates Oregon v2.1 sensors toward an Oregon base. setup() does the boot init
// (radio + the three Oregon_TM transmitters, flag 0x0B held); loop() runs the
// scheduler, gated to a 250 ms tick to preserve the hand-tuned cadence:
//   - TH (RTGN318) every th_interval (~53 s ch1), anchored BEFORE SendPacket
//   - Clock (RTGR328N) DECOUPLED: only every clock_interval, th_clock_gap after a TH
//   - UV (UVR128) on its own ~73 s cadence
class OregonComponent : public Component {
 public:
  void set_radio(RadioType r) { this->radio_ = r; }
  void set_tx_pin(int pin) { this->tx_pin_ = pin; }
  void set_time(time::RealTimeClock *t) { this->time_ = t; }
  void set_th_interval(uint32_t ms) { this->th_interval_ = ms; }
  void set_clock_interval(uint32_t ms) { this->clock_interval_ = ms; }
  void set_th_clock_gap(uint32_t ms) { this->gap_ = ms; }
  void set_uv_interval(uint32_t ms) { this->uv_interval_ = ms; }
  void set_uv_boot(uint8_t uv) { this->uv_boot_ = uv; }
  void set_channel(uint8_t ch) { this->channel_ = ch; }
  void set_cc1101_pins(int sck, int miso, int mosi, int csn) {
    this->cc1101_sck_ = sck; this->cc1101_miso_ = miso;
    this->cc1101_mosi_ = mosi; this->cc1101_csn_ = csn;
  }
  void set_temp_sensor(sensor::Sensor *s) { this->temp_ = s; }
  void set_hum_sensor(sensor::Sensor *s) { this->hum_ = s; }
  void set_uv_sensor(sensor::Sensor *s) { this->uvs_ = s; }
  // Additional channels 2-5: declared in YAML order. Model is chosen per channel
  // (default RTGN318+clock); THGN132 = non-clock TH-only. interval 0 = library default.
  void add_extra_channel(uint8_t ch, ExtraModel model, uint32_t interval);
  void set_extra_temp(uint8_t ch, sensor::Sensor *s);
  void set_extra_hum(uint8_t ch, sensor::Sensor *s);
  // Optional diagnostics (ch1), published at each ch1 TH / clock send.
  void set_last_temp_diag(sensor::Sensor *s) { this->last_temp_diag_ = s; }
  void set_last_hum_diag(sensor::Sensor *s) { this->last_hum_diag_ = s; }
  void set_th_count_diag(sensor::Sensor *s) { this->th_count_diag_ = s; }
  void set_clock_count_diag(sensor::Sensor *s) { this->clock_count_diag_ = s; }
  void set_last_clock_diag(text_sensor::TextSensor *s) { this->last_clock_diag_ = s; }
  void set_tx_status_diag(text_sensor::TextSensor *s) { this->tx_status_diag_ = s; }

  void setup() override;
  void loop() override;
  void dump_config() override;
  // LATE so time/sensor components are constructed first.
  float get_setup_priority() const override { return setup_priority::LATE; }

 protected:
  void init_radio_();
  // Each returns true if it transmitted a packet this tick. loop() sends at most ONE packet
  // per 250 ms tick (ch1 first), so a crowded moment spreads over ticks instead of blocking.
  void send_uv_();             // UV cadence block
  bool run_extra_channels_();  // extra ch2-5 (self-paced, anchored; RTGN318 carry a clock)
  bool run_scheduler_();       // ch1 TH + decoupled clock state machine

  // One emulated extra sensor (ch2-5). RTGN318 -> clk != nullptr (TH + own clock, so the
  // base never starves a clock-sync channel); THGN132 -> clk == nullptr (TH-only, non-clock).
  // Each is self-paced and anchored before SendPacket.
  struct ExtraChannel {
    uint8_t channel{0};
    ExtraModel model{EXTRA_RTGN318};
    uint32_t interval{0};  // 0 = keep the library's per-channel/model send_time
    sensor::Sensor *temp{nullptr};
    sensor::Sensor *hum{nullptr};
    Oregon_TM *th{nullptr};
    Oregon_TM *clk{nullptr};  // non-null for RTGN318 channels (TH + own clock)
    uint32_t last_pair{0};
    uint32_t last_clock{0};
    bool clk_pending{false};  // a clock is queued to send the tick AFTER this channel's TH
  };
  ExtraChannel *find_extra_(uint8_t ch);

  // ---- config ----
  RadioType radio_{RADIO_FS1000A};
  int tx_pin_{4};
  uint8_t channel_{1};
  time::RealTimeClock *time_{nullptr};
  uint32_t th_interval_{53000};
  uint32_t clock_interval_{600000};
  uint32_t gap_{5000};
  uint32_t uv_interval_{73000};
  uint8_t uv_boot_{5};
  int cc1101_sck_{18}, cc1101_miso_{19}, cc1101_mosi_{23}, cc1101_csn_{5};
  sensor::Sensor *temp_{nullptr}, *hum_{nullptr}, *uvs_{nullptr};
  std::vector<ExtraChannel> extra_;  // extra ch2-5 (empty unless declared)

  // ---- optional diagnostics (ch1; null unless declared in YAML) ----
  sensor::Sensor *last_temp_diag_{nullptr};
  sensor::Sensor *last_hum_diag_{nullptr};
  sensor::Sensor *th_count_diag_{nullptr};
  sensor::Sensor *clock_count_diag_{nullptr};
  text_sensor::TextSensor *last_clock_diag_{nullptr};
  text_sensor::TextSensor *tx_status_diag_{nullptr};
  uint32_t th_tx_count_{0};
  uint32_t clock_tx_count_{0};

  // ---- runtime state ----
  Oregon_TM *th_{nullptr};
  Oregon_TM *clock_{nullptr};
  Oregon_TM *uv_{nullptr};
  bool ready_{false};
  int sched_state_{0};
  uint32_t last_pair_{0};
  uint32_t last_pair_uv_{0};
  uint32_t th_sent_{0};
  uint32_t last_clock_{0};
  uint32_t last_tick_{0};
};

}  // namespace oregon
}  // namespace esphome
