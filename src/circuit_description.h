// circuit_description.h - Johan Smet - BSD-3-Clause (see LICENSE)
//
// describe the composition of a logic circuit

#ifndef LSIM_CIRCUIT_DESCRIPTION_H
#define LSIM_CIRCUIT_DESCRIPTION_H

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>

#include "component_description.h"
#include "wire_description.h"

namespace lsim {

class CircuitDescription {
public:
    typedef std::unique_ptr<CircuitDescription> uptr_t;
    typedef std::unordered_map<uint32_t, Wire::uptr_t> wire_lut_t;
public:
    CircuitDescription(const char *name, class LSimContext *context);
    CircuitDescription(const CircuitDescription &) = delete;

    class LSimContext *context() const {return m_context;}

    // name
    const std::string &name() const {return m_name;}
    void change_name(const char *name);

    // components
protected:
    Component *create_component(ComponentType type, size_t input_pins, size_t output_pins = 1, size_t control_pins = 0);
    Component *create_component(CircuitDescription *nested_circuit);
public:
    Component *component_by_id(uint32_t id);
    std::vector<uint32_t> component_ids() const;
    std::vector<uint32_t> component_ids_of_type(ComponentType type) const;
    void disconnect_component(uint32_t id);
    void remove_component(uint32_t id);

    // connections
    Wire *create_wire();
    Wire *connect(pin_id_t pin_a, pin_id_t pin_b);
    void disconnect_pin(pin_id_t pin);
    std::vector<uint32_t> wire_ids() const;
    Wire *wire_by_id(uint32_t id) const;
    const wire_lut_t &wires() const {return m_wires;}
    void remove_wire(uint32_t id);

    // ports
    void rebuild_port_list();
    void change_port_pin_count(uint32_t comp_id, size_t new_count);
    pin_id_t port_by_name(const char *name) const;
    pin_id_t port_by_index(bool input, size_t index) const;
    const std::string &port_name(bool input, size_t index) const;
    size_t num_input_ports() const {return m_input_ports.size();}
    size_t num_output_ports() const {return m_output_ports.size();}

    // specialized component creation functions
    Component *add_connector_in(const char *name, size_t data_bits, bool tri_state = false);
    Component *add_connector_out(const char *name, size_t data_bits, bool tri_state = false);
    Component *add_constant(Value value);
    Component *add_pull_resistor(Value pull_to);
    Component *add_buffer(size_t data_bits);
    Component *add_tristate_buffer(size_t data_bits);
    Component *add_and_gate(size_t num_inputs);
    Component *add_or_gate(size_t num_inputs);
    Component *add_not_gate();
    Component *add_nand_gate(size_t num_inputs);
    Component *add_nor_gate(size_t num_inputs);
    Component *add_xor_gate();
    Component *add_xnor_gate();
    Component *add_sub_circuit(const char *circuit);

    // instantiate into a simulator
    std::unique_ptr<class CircuitInstance> instantiate(class Simulator *sim);

private:
    typedef std::unordered_map<uint32_t, Component::uptr_t> component_lut_t;
    typedef std::vector<std::string> port_container_t;

private:
    class LSimContext *m_context;
    std::string     m_name;

    uint32_t        m_component_id;
    component_lut_t m_components;

    uint32_t        m_wire_id;
    wire_lut_t      m_wires;

    port_lut_t       m_ports_lut;
    port_container_t m_input_ports;
    port_container_t m_output_ports;

};

} // namespace lsim

#endif // LSIM_CIRCUIT_DESCRIPTION_H