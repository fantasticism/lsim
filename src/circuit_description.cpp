// circuit_description.cpp - Johan Smet - BSD-3-Clause (see LICENSE)
//
// describe the composition of a logic circuit

#include "circuit_description.h"

#include <cassert>

namespace lsim {

///////////////////////////////////////////////////////////////////////////////
//
// Component
//

Component::Component(uint32_t id, ComponentType type, size_t inputs, size_t outputs, size_t controls) :
        m_id(id),
        m_type(type),
        m_inputs(inputs),
        m_outputs(outputs),
        m_controls(controls),
        m_position(0,0),
        m_angle(0) {
}

pin_id_t Component::pin_id(size_t index) const {
    assert(index < m_inputs + m_outputs + m_controls);
    return (static_cast<pin_id_t>(m_id) << 32) | (index & 0xffffffff);
}

pin_id_t Component::input_pin_id(size_t index) const {
    assert(index < m_inputs);
    return pin_id(index);
}

pin_id_t Component::output_pin_id(size_t index) const {
    assert(index < m_outputs);
    return pin_id(m_inputs + index);
}

pin_id_t Component::control_pin_id(size_t index) const {
    assert(index < m_controls);
    return pin_id(m_inputs + m_outputs + index);
}

void Component::add_property(Property::uptr_t &&prop) {
    m_properties[prop->key()] = std::move(prop);
}

Property *Component::property(const char *key) {
    auto result = m_properties.find(key);
    if (result != m_properties.end()) {
        return result->second.get();
    } else {
        return nullptr;
    }
}

std::string Component::property_value(const char *key, const char *def_value) {
    auto result = property(key);
    return (result != nullptr) ? result->value_as_string() : def_value;
}

int64_t Component::property_value(const char *key, int64_t def_value) {
    auto result = property(key);
    return (result != nullptr) ? result->value_as_integer() : def_value;
}

bool Component::property_value(const char *key, bool def_value) {
    auto result = property(key);
    return (result != nullptr) ? result->value_as_boolean() : def_value;
}

void Component::set_position(const Point &pos) {
    m_position = pos;
}

void Component::set_angle(int angle) {
    m_angle = angle;
}

///////////////////////////////////////////////////////////////////////////////
//
// Wire
//

Wire::Wire(uint32_t id) : m_id(id) {

}

void Wire::add_pin(pin_id_t pin) {
    m_pins.push_back(pin);
}

///////////////////////////////////////////////////////////////////////////////
//
// CircuitDescription
//

CircuitDescription::CircuitDescription(const char *name) :
        m_name(name),
        m_component_id(0),
        m_wire_id(0) {
}

void CircuitDescription::change_name(const char *name) {
    assert(name);
    m_name = name;
}

Component *CircuitDescription::create_component(ComponentType type, size_t input_pins, size_t output_pins, size_t control_pins) {
    auto component = std::make_unique<Component>(m_component_id++, type, input_pins, output_pins, control_pins);
    auto result = component.get();
    m_components[result->id()] = std::move(component);

    if (type == COMPONENT_CONNECTOR_IN || type == COMPONENT_CONNECTOR_OUT) {
        result->add_property(make_property("name", (std::string("c#") + std::to_string(result->id())).c_str()));
        result->add_property(make_property("tri_state", false));
    } else if (type == COMPONENT_CONSTANT) {
        result->add_property(make_property("value", static_cast<int64_t>(VALUE_FALSE)));
    } else if (type == COMPONENT_PULL_RESISTOR) {
        result->add_property(make_property("pull_to", static_cast<int64_t>(VALUE_FALSE)));
    } else if (type == COMPONENT_SUB_CIRCUIT) {
        result->add_property(make_property("circuit", "unknown"));
    }

    return result;
}

Component *CircuitDescription::component_by_id(uint32_t id) {
    auto found = m_components.find(id);
    if (found != m_components.end()) {
        return found->second.get();
    } else {
        return nullptr;
    }
}

Wire *CircuitDescription::create_wire() {
    auto wire = std::make_unique<Wire>(m_wire_id++);
    auto result = wire.get();
    m_wires[result->id()] = std::move(wire);
    return result;
}

Wire *CircuitDescription::connect(pin_id_t pin_a, pin_id_t pin_b) {
    auto wire = create_wire();
    wire->add_pin(pin_a);
    wire->add_pin(pin_b);
    return wire;
}

Component *CircuitDescription::add_connector_in(const char *name, size_t data_bits, bool tri_state) {
    auto result = create_component(COMPONENT_CONNECTOR_IN, 0, data_bits, 0);
    result->property("name")->value(name);
    result->property("tri_state")->value(tri_state);
    return result;
}

Component *CircuitDescription::add_connector_out(const char *name, size_t data_bits, bool tri_state) {
    auto result = create_component(COMPONENT_CONNECTOR_OUT, data_bits, 0, 0);
    result->property("name")->value(name);
    result->property("tri_state")->value(tri_state);
    return result;
}

Component *CircuitDescription::add_constant(Value value) {
    auto result = create_component(COMPONENT_CONSTANT, 0, 1, 0);
    result->property("value")->value(static_cast<int64_t>(value));
    return result;
}

Component *CircuitDescription::add_pull_resistor(Value pull_to) {
    auto result = create_component(COMPONENT_PULL_RESISTOR, 0, 1, 0);
    result->property("pull_to")->value(static_cast<int64_t>(pull_to));
    return result;
}

Component *CircuitDescription::add_buffer(size_t data_bits) {
    assert(data_bits >= 1);
    return create_component(COMPONENT_BUFFER, data_bits, data_bits, 0);
}

Component *CircuitDescription::add_tristate_buffer(size_t data_bits) {
    assert(data_bits >= 1);
   return create_component(COMPONENT_TRISTATE_BUFFER, data_bits, data_bits, 1);
}

Component *CircuitDescription::add_and_gate(size_t num_inputs) {
    assert(num_inputs >= 2);
    return create_component(COMPONENT_AND_GATE, num_inputs, 1, 0);
}

Component *CircuitDescription::add_or_gate(size_t num_inputs) {
    assert(num_inputs >= 2);
    return create_component(COMPONENT_OR_GATE, num_inputs, 1, 0);
}

Component *CircuitDescription::add_not_gate() {
    return create_component(COMPONENT_NOT_GATE, 1, 1, 0);
}

Component *CircuitDescription::add_nand_gate(size_t num_inputs) {
    assert(num_inputs >= 2);
    return create_component(COMPONENT_NAND_GATE, num_inputs, 1, 0);
}

Component *CircuitDescription::add_nor_gate(size_t num_inputs) {
    assert(num_inputs >= 2);
    return create_component(COMPONENT_NOR_GATE, num_inputs, 1, 0);
}

Component *CircuitDescription::add_xor_gate() {
    return create_component(COMPONENT_XOR_GATE, 2, 1, 0);
}

Component *CircuitDescription::add_xnor_gate() {
    return create_component(COMPONENT_XNOR_GATE, 2, 1, 0);
}

Component *CircuitDescription::add_sub_circuit(const char *circuit) {
    return nullptr;    
}

} // namespace lsim