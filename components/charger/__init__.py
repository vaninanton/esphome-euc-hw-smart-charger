# Copyright 2025 <Tony V>
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor, number, sensor
from esphome.const import CONF_ID

AUTO_LOAD = ["binary_sensor", "number", "sensor"]

charger_ns = cg.esphome_ns.namespace("charger")
ChargerComponent = charger_ns.class_("ChargerComponent", cg.Component)


def _sensor(name, icon=None, device_class=None, unit=None, decimals=2, state_class="measurement"):
    d = {"name": name, "accuracy_decimals": decimals}
    if icon:
        d["icon"] = icon
    if device_class:
        d["device_class"] = device_class
    if unit:
        d["unit_of_measurement"] = unit
    if state_class:
        d["state_class"] = state_class
    return d


ENTITY_REGISTRY = [
    # (yaml_key, type, setter, defaults)
    ("charging",     "binary", "set_binary_sensor_charging",
     {"name": "Charging", "icon": "mdi:battery-charging", "device_class": "battery_charging"}),
    ("out_voltage",  "sensor", "set_sensor_out_voltage",
     _sensor("Out Voltage",  "mdi:flash",           "voltage",     "V",   2)),
    ("out_current",  "sensor", "set_sensor_out_current",
     _sensor("Out Current",  "mdi:current-dc",      "current",     "A",   2)),
    ("out_power",    "sensor", "set_sensor_out_power",
     _sensor("Out Power",    "mdi:lightning-bolt",  "power",       "W",   1)),
    ("in_voltage",   "sensor", "set_sensor_in_voltage",
     _sensor("In Voltage",   "mdi:power-plug",      "voltage",     "V",   1)),
    ("in_current",   "sensor", "set_sensor_in_current",
     _sensor("In Current",   "mdi:current-ac",      "current",     "A",   2)),
    ("frequency",    "sensor", "set_sensor_frequency",
     _sensor("Frequency",    "mdi:sine-wave",       "frequency",   "Hz",  1)),
    ("temp1",        "sensor", "set_sensor_temp1",
     _sensor("Temp 1",       None,                  "temperature", "°C",  1)),
    ("temp2",        "sensor", "set_sensor_temp2",
     _sensor("Temp 2",       None,                  "temperature", "°C",  1)),
    ("efficiency",   "sensor", "set_sensor_efficiency",
     _sensor("Efficiency",   "mdi:percent",         None,          "%",   1)),
    ("charge_ah",    "sensor", "set_sensor_charge_ah",
     _sensor("Charge Ah",    "mdi:battery-charging", None,         "Ah",  3, "total_increasing")),
    ("charge_wh",    "sensor", "set_sensor_charge_wh",
     _sensor("Charge Wh",    "mdi:battery-charging-high", "energy", "Wh", 2, "total_increasing")),
    ("set_voltage",  "sensor", "set_sensor_set_voltage",
     _sensor("Set Voltage",  "mdi:tune",            "voltage",     "V",   1)),
    ("set_current",  "sensor", "set_sensor_set_current",
     _sensor("Set Current",  "mdi:tune",            "current",     "A",   1)),
]

SENSOR_KEYS = [r[0] for r in ENTITY_REGISTRY if r[1] == "sensor"]
BINARY_KEYS = [r[0] for r in ENTITY_REGISTRY if r[1] == "binary"]
ALL_KEYS    = [r[0] for r in ENTITY_REGISTRY]
DEFAULTS    = {r[0]: r[3] for r in ENTITY_REGISTRY}
SETTER      = {r[0]: r[2] for r in ENTITY_REGISTRY}

CONF_SORTING_GROUP_ID = "sorting_group_id"

WEB_SERVER_DEFAULTS = {
    "charging":    (CONF_SORTING_GROUP_ID, 0),
    "out_voltage": (CONF_SORTING_GROUP_ID, 1),
    "out_current": (CONF_SORTING_GROUP_ID, 2),
    "out_power":   (CONF_SORTING_GROUP_ID, 3),
    "in_voltage":  (CONF_SORTING_GROUP_ID, 10),
    "in_current":  (CONF_SORTING_GROUP_ID, 11),
    "frequency":   (CONF_SORTING_GROUP_ID, 12),
    "temp1":       (CONF_SORTING_GROUP_ID, 20),
    "temp2":       (CONF_SORTING_GROUP_ID, 21),
    "efficiency":  (CONF_SORTING_GROUP_ID, 30),
    "charge_ah":   (CONF_SORTING_GROUP_ID, 31),
    "charge_wh":   (CONF_SORTING_GROUP_ID, 32),
    "set_voltage": (CONF_SORTING_GROUP_ID, 40),
    "set_current": (CONF_SORTING_GROUP_ID, 41),
}


def _fill_missing(config):
    result = dict(config)
    for key in ALL_KEYS:
        if key not in result:
            result[key] = {}
    return result


def _expand_prefix(config):
    prefix = config.get("id_prefix")
    device_id = config.get("device_id")
    result = dict(config)
    for key in ALL_KEYS:
        entry = dict(result.get(key) or {})
        if prefix:
            entry.setdefault(CONF_ID, f"{prefix}_{key}")
        if device_id is not None:
            entry["device_id"] = device_id
        result[key] = entry
    return result


def _inject_web_server(config):
    result = dict(config)
    for key, (group_attr, weight) in WEB_SERVER_DEFAULTS.items():
        group_id = result.get(group_attr)
        if not group_id:
            continue
        entry = result.get(key)
        if not isinstance(entry, dict) or "web_server" in entry:
            continue
        result[key] = {**entry, "web_server": {"sorting_group_id": group_id, "sorting_weight": weight}}
    return result


def _merge(key, defaults):
    def validator(value):
        out = dict(defaults)
        if value:
            out.update(value)
        if CONF_ID not in out:
            out[CONF_ID] = key
        return out
    return validator


SINGLE_SCHEMA = cv.All(
    _fill_missing,
    _expand_prefix,
    _inject_web_server,
    cv.Schema({
        cv.GenerateID(): cv.declare_id(ChargerComponent),
        cv.Optional("id_prefix"): cv.string,
        cv.Optional("device_id"): cv.string,
        cv.Optional(CONF_SORTING_GROUP_ID): cv.string,
        cv.Optional("target_voltage_id"): cv.use_id(number.Number),
        cv.Optional("target_current_id"): cv.use_id(number.Number),
        **{
            cv.Optional(k): cv.All(
                _merge(k, DEFAULTS[k]),
                sensor.sensor_schema().extend(cv.COMPONENT_SCHEMA),
            )
            for k in SENSOR_KEYS
        },
        **{
            cv.Optional(k): cv.All(
                _merge(k, DEFAULTS[k]),
                binary_sensor.binary_sensor_schema().extend(cv.COMPONENT_SCHEMA),
            )
            for k in BINARY_KEYS
        },
    }).extend(cv.COMPONENT_SCHEMA),
)

CONFIG_SCHEMA = cv.ensure_list(SINGLE_SCHEMA)


async def _to_code_one(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    for key in SENSOR_KEYS:
        sens = await sensor.new_sensor(config[key])
        cg.add(getattr(var, SETTER[key])(sens))

    for key in BINARY_KEYS:
        sens = await binary_sensor.new_binary_sensor(config[key])
        cg.add(getattr(var, SETTER[key])(sens))

    if "target_voltage_id" in config:
        num = await cg.get_variable(config["target_voltage_id"])
        cg.add(var.set_number_target_voltage(num))
    if "target_current_id" in config:
        num = await cg.get_variable(config["target_current_id"])
        cg.add(var.set_number_target_current(num))


async def to_code(config):
    for one in config:
        await _to_code_one(one)
