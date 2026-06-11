// Copyright 2025 <Tony V>
#include <cstring>
#include <vector>
#include "esphome/core/log.h"
#include "charger.h"

namespace esphome::charger {

static const char *const TAG = "charger";

using namespace proto;

static inline float read_f32_le(const uint8_t *p) {
  float f;
  memcpy(&f, p, 4);
  return f;
}

// Checksum: sum of bytes starting from index 1 (mod 256).
static uint8_t calc_checksum(const uint8_t *data, size_t len) {
  uint8_t s = 0;
  for (size_t i = 1; i < len; i++)
    s += data[i];
  return s;
}

void ChargerComponent::setup() {}

void ChargerComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "HW Smart Charger component");
}

void ChargerComponent::loop() {
  if (packet_queue_.empty())
    return;
  std::vector<uint8_t> pkt = std::move(packet_queue_.front());
  packet_queue_.pop_front();
  parse_packet(pkt.data(), pkt.size());

  uint32_t now = millis();
  if (now - last_publish_ms_ >= PUBLISH_INTERVAL_MS) {
    last_publish_ms_ = now;
    publish_state_from_data();
  }
}

/// Reassembles BLE notify chunks into packets. Packet length = buf[0]+1.
void ChargerComponent::parse_ble_packet(const std::vector<uint8_t> &x) {
  if (ble_buffer_.size() + x.size() > BLE_BUFFER_MAX)
    ble_buffer_.clear();
  ble_buffer_.insert(ble_buffer_.end(), x.begin(), x.end());

  while (ble_buffer_.size() >= 2) {
    size_t pkt_len = (size_t)ble_buffer_[0] + 1;
    if (ble_buffer_.size() < pkt_len)
      break;
    if (packet_queue_.size() < PACKET_QUEUE_MAX)
      packet_queue_.push_back(std::vector<uint8_t>(ble_buffer_.begin(), ble_buffer_.begin() + pkt_len));
    ble_buffer_.erase(ble_buffer_.begin(), ble_buffer_.begin() + pkt_len);
  }
}

void ChargerComponent::parse_packet(const uint8_t *data, size_t size) {
  if (size < 2)
    return;
  uint8_t cmd = data[1];
  if (cmd == CMD_STATUS_RESPONSE && size >= STATUS_MIN_SIZE)
    parse_status_packet(data, size);
  else if (cmd == CMD_CONFIG_RESPONSE && size >= CONFIG_MIN_SIZE)
    parse_config_packet(data, size);
}

void ChargerComponent::parse_status_packet(const uint8_t *pkt, size_t size) {
  (void)size;
  data_.in_voltage  = read_f32_le(pkt + STATUS_IN_VOLTAGE);
  data_.in_current  = read_f32_le(pkt + STATUS_IN_CURRENT);
  data_.frequency   = read_f32_le(pkt + STATUS_FREQUENCY);
  data_.temp2       = read_f32_le(pkt + STATUS_TEMP2);
  data_.temp1       = read_f32_le(pkt + STATUS_TEMP1);
  data_.out_voltage = read_f32_le(pkt + STATUS_OUT_VOLTAGE);
  data_.out_current = read_f32_le(pkt + STATUS_OUT_CURRENT);
  data_.efficiency  = read_f32_le(pkt + STATUS_EFFICIENCY);
  data_.charging    = (pkt[STATUS_CHARGE_STATE] == 0);
  data_.charge_ah   = read_f32_le(pkt + STATUS_CHARGE_AH);
  data_.charge_wh   = read_f32_le(pkt + STATUS_CHARGE_WH);
}

void ChargerComponent::parse_config_packet(const uint8_t *pkt, size_t size) {
  (void)size;
  data_.set_voltage = read_f32_le(pkt + CONFIG_SET_VOLTAGE);
  data_.set_current = read_f32_le(pkt + CONFIG_SET_CURRENT);
}

void ChargerComponent::publish_state_from_data() {
  if (sensor_out_voltage_ != nullptr)
    sensor_out_voltage_->publish_state(data_.out_voltage);
  if (sensor_out_current_ != nullptr)
    sensor_out_current_->publish_state(data_.out_current);
  if (sensor_out_power_ != nullptr)
    sensor_out_power_->publish_state(data_.out_voltage * data_.out_current);
  if (sensor_in_voltage_ != nullptr)
    sensor_in_voltage_->publish_state(data_.in_voltage);
  if (sensor_in_current_ != nullptr)
    sensor_in_current_->publish_state(data_.in_current);
  if (sensor_frequency_ != nullptr)
    sensor_frequency_->publish_state(data_.frequency);
  if (sensor_temp1_ != nullptr)
    sensor_temp1_->publish_state(data_.temp1);
  if (sensor_temp2_ != nullptr)
    sensor_temp2_->publish_state(data_.temp2);
  if (sensor_efficiency_ != nullptr)
    sensor_efficiency_->publish_state(data_.efficiency);
  if (sensor_charge_ah_ != nullptr)
    sensor_charge_ah_->publish_state(data_.charge_ah);
  if (sensor_charge_wh_ != nullptr)
    sensor_charge_wh_->publish_state(data_.charge_wh);
  if (binary_sensor_charging_ != nullptr)
    binary_sensor_charging_->publish_state(data_.charging);
  if (sensor_set_voltage_ != nullptr)
    sensor_set_voltage_->publish_state(data_.set_voltage);
  if (sensor_set_current_ != nullptr)
    sensor_set_current_->publish_state(data_.set_current);
  if (number_target_voltage_ != nullptr)
    number_target_voltage_->publish_state(data_.set_voltage);
  if (number_target_current_ != nullptr)
    number_target_current_->publish_state(data_.set_current);
}

std::vector<uint8_t> ChargerComponent::get_query_packet() const {
  return {0x02, CMD_STATUS_QUERY, CMD_STATUS_QUERY};
}

std::vector<uint8_t> ChargerComponent::make_float_command(uint8_t opcode, float value) const {
  uint8_t buf[7];
  buf[0] = CMD_PREFIX_SET;
  buf[1] = opcode;
  memcpy(buf + 2, &value, 4);
  buf[6] = calc_checksum(buf, 6);
  return std::vector<uint8_t>(buf, buf + 7);
}

std::vector<uint8_t> ChargerComponent::make_toggle_command(uint8_t opcode, bool on) const {
  uint8_t buf[7] = {CMD_PREFIX_SET, opcode, (uint8_t)(on ? 1 : 0), 0x00, 0x00, 0x00, 0x00};
  buf[6] = calc_checksum(buf, 6);
  return std::vector<uint8_t>(buf, buf + 7);
}

std::vector<uint8_t> ChargerComponent::get_set_voltage_packet(float voltage) const {
  return make_float_command(OP_SET_VOLTAGE, voltage);
}

std::vector<uint8_t> ChargerComponent::get_set_current_packet(float current) const {
  return make_float_command(OP_SET_CURRENT, current);
}

std::vector<uint8_t> ChargerComponent::get_toggle_on_packet(uint8_t opcode) const {
  return make_toggle_command(opcode, true);
}

std::vector<uint8_t> ChargerComponent::get_toggle_off_packet(uint8_t opcode) const {
  return make_toggle_command(opcode, false);
}

}  // namespace esphome::charger
