// Copyright 2025 <Tony V>
#pragma once

#include <cstddef>
#include <deque>
#include <vector>
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/number/number.h"
#include "esphome/components/switch/switch.h"

namespace esphome::charger {

// Offsets within a status packet (cmd byte 0x06, pkt[1]==0x06).
// Packet format: pkt[0]=length, pkt[1]=cmd, pkt[2..]=payload as float32 LE fields.
namespace proto {
constexpr uint8_t CMD_STATUS_RESPONSE = 0x06;
constexpr uint8_t CMD_STATUS_QUERY    = 0x06;
constexpr uint8_t CMD_PREFIX_QUERY    = 0x02;
constexpr uint8_t CMD_PREFIX_SET      = 0x06;
constexpr uint8_t CMD_PREFIX_ACK      = 0x03;

constexpr uint8_t OP_SET_VOLTAGE      = 0x07;
constexpr uint8_t OP_SET_CURRENT      = 0x08;

constexpr size_t STATUS_MIN_SIZE = 49;  // minimum packet size for cmd 0x06 response

// Float32 LE field offsets within the status packet (pkt[0]=len, pkt[1]=0x06)
constexpr size_t STATUS_IN_VOLTAGE    = 2;
constexpr size_t STATUS_IN_CURRENT    = 6;
constexpr size_t STATUS_FREQUENCY     = 10;
constexpr size_t STATUS_TEMP2         = 14;
constexpr size_t STATUS_TEMP1         = 18;
constexpr size_t STATUS_OUT_VOLTAGE   = 22;
constexpr size_t STATUS_OUT_CURRENT   = 26;
constexpr size_t STATUS_EFFICIENCY    = 34;
constexpr size_t STATUS_CHARGE_STATE  = 38;  // uint8: 0=charging, other=idle
constexpr size_t STATUS_CHARGE_AH     = 39;
constexpr size_t STATUS_CHARGE_WH     = 43;

// Config response (cmd 0x05), packet size >= 106
constexpr uint8_t CMD_CONFIG_RESPONSE = 0x05;
constexpr size_t CONFIG_MIN_SIZE      = 106;
constexpr size_t CONFIG_SET_VOLTAGE   = 2;
constexpr size_t CONFIG_SET_CURRENT   = 6;
}  // namespace proto

struct ChargerData {
  bool charging{};
  float in_voltage{};
  float in_current{};
  float frequency{};
  float temp1{};
  float temp2{};
  float out_voltage{};
  float out_current{};
  float efficiency{};
  float charge_ah{};
  float charge_wh{};
  float set_voltage{};
  float set_current{};
};

class ChargerComponent : public Component {
 public:
  const ChargerData &get_data() const { return data_; }

  /// Feed raw BLE notify bytes; reassembles packets and queues complete ones.
  void parse_ble_packet(const std::vector<uint8_t> &x);

  /// Parse one packet into data_ (no publish). Called from loop().
  void parse_packet(const uint8_t *data, size_t size);

  /// Returns the 3-byte status query packet: 02 06 06.
  std::vector<uint8_t> get_query_packet() const;

  /// Returns 7-byte set-voltage packet: 06 07 [float32 LE] [checksum].
  std::vector<uint8_t> get_set_voltage_packet(float voltage) const;

  /// Returns 7-byte set-current packet: 06 08 [float32 LE] [checksum].
  std::vector<uint8_t> get_set_current_packet(float current) const;

  /// Returns 7-byte toggle-on packet for the given opcode.
  std::vector<uint8_t> get_toggle_on_packet(uint8_t opcode) const;

  /// Returns 7-byte toggle-off packet for the given opcode.
  std::vector<uint8_t> get_toggle_off_packet(uint8_t opcode) const;

  void set_sensor_in_voltage(sensor::Sensor *s)    { sensor_in_voltage_ = s; }
  void set_sensor_in_current(sensor::Sensor *s)    { sensor_in_current_ = s; }
  void set_sensor_frequency(sensor::Sensor *s)     { sensor_frequency_ = s; }
  void set_sensor_temp1(sensor::Sensor *s)         { sensor_temp1_ = s; }
  void set_sensor_temp2(sensor::Sensor *s)         { sensor_temp2_ = s; }
  void set_sensor_out_voltage(sensor::Sensor *s)   { sensor_out_voltage_ = s; }
  void set_sensor_out_current(sensor::Sensor *s)   { sensor_out_current_ = s; }
  void set_sensor_out_power(sensor::Sensor *s)     { sensor_out_power_ = s; }
  void set_sensor_efficiency(sensor::Sensor *s)    { sensor_efficiency_ = s; }
  void set_sensor_charge_ah(sensor::Sensor *s)     { sensor_charge_ah_ = s; }
  void set_sensor_charge_wh(sensor::Sensor *s)     { sensor_charge_wh_ = s; }
  void set_sensor_set_voltage(sensor::Sensor *s)   { sensor_set_voltage_ = s; }
  void set_sensor_set_current(sensor::Sensor *s)   { sensor_set_current_ = s; }
  void set_binary_sensor_charging(binary_sensor::BinarySensor *s) { binary_sensor_charging_ = s; }
  void set_number_target_voltage(number::Number *n) { number_target_voltage_ = n; }
  void set_number_target_current(number::Number *n) { number_target_current_ = n; }

  void setup() override;
  void loop() override;
  void dump_config() override;

 protected:
  ChargerData data_;

  std::vector<uint8_t> ble_buffer_;
  static constexpr size_t BLE_BUFFER_MAX = 512;
  static constexpr size_t PACKET_QUEUE_MAX = 4;
  std::deque<std::vector<uint8_t>> packet_queue_;

  static constexpr uint32_t PUBLISH_INTERVAL_MS = 1000;
  uint32_t last_publish_ms_{0};

  sensor::Sensor *sensor_in_voltage_{nullptr};
  sensor::Sensor *sensor_in_current_{nullptr};
  sensor::Sensor *sensor_frequency_{nullptr};
  sensor::Sensor *sensor_temp1_{nullptr};
  sensor::Sensor *sensor_temp2_{nullptr};
  sensor::Sensor *sensor_out_voltage_{nullptr};
  sensor::Sensor *sensor_out_current_{nullptr};
  sensor::Sensor *sensor_out_power_{nullptr};
  sensor::Sensor *sensor_efficiency_{nullptr};
  sensor::Sensor *sensor_charge_ah_{nullptr};
  sensor::Sensor *sensor_charge_wh_{nullptr};
  sensor::Sensor *sensor_set_voltage_{nullptr};
  sensor::Sensor *sensor_set_current_{nullptr};
  binary_sensor::BinarySensor *binary_sensor_charging_{nullptr};
  number::Number *number_target_voltage_{nullptr};
  number::Number *number_target_current_{nullptr};

  void parse_status_packet(const uint8_t *pkt, size_t size);
  void parse_config_packet(const uint8_t *pkt, size_t size);
  void publish_state_from_data();

  std::vector<uint8_t> make_float_command(uint8_t opcode, float value) const;
  std::vector<uint8_t> make_toggle_command(uint8_t opcode, bool on) const;
};

}  // namespace esphome::charger
