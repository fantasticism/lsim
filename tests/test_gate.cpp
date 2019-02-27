#include "catch.hpp"
#include "circuit.h"
#include "gate.h"

inline void simulate_until_pin_change(Circuit *circuit, pin_t pin) {
    do {
        circuit->simulation_tick();
    } while (!circuit->value_changed(pin));
}

TEST_CASE("AND-gate functions as expected", "[gate]") {

    auto circuit = std::make_unique<Circuit>();
    REQUIRE(circuit);

    auto in_0 = circuit->create_component<Constant>(1, VALUE_FALSE);
    REQUIRE(in_0);
    auto in_1 = circuit->create_component<Constant>(1, VALUE_TRUE);
    REQUIRE(in_1);

    auto and_gate = circuit->create_component<AndGate>(2);
    REQUIRE(and_gate);

    auto out = circuit->create_component<Connector>(1);
    REQUIRE(out);

    circuit->simulation_init();
    circuit->connect_pins(and_gate->pin(2), out->pin(0));

    SECTION("all inputs are false") {
        circuit->connect_pins(in_0->pin(0), and_gate->pin(0));
        circuit->connect_pins(in_0->pin(0), and_gate->pin(1));

        simulate_until_pin_change(circuit.get(), out->pin(0));
        REQUIRE(circuit->read_value(out->pin(0)) == VALUE_FALSE);
    }

    SECTION("all inputs are true") {
        circuit->connect_pins(in_1->pin(0), and_gate->pin(0));
        circuit->connect_pins(in_1->pin(0), and_gate->pin(1));

        simulate_until_pin_change(circuit.get(), out->pin(0));
        REQUIRE(circuit->read_value(out->pin(0)) == VALUE_TRUE);
    }

    SECTION("first input is true, second is false") {
        circuit->connect_pins(in_1->pin(0), and_gate->pin(0));
        circuit->connect_pins(in_0->pin(0), and_gate->pin(1));

        simulate_until_pin_change(circuit.get(), out->pin(0));
        REQUIRE(circuit->read_value(out->pin(0)) == VALUE_FALSE);
    }

    SECTION("first input is false, second is true") {
        circuit->connect_pins(in_0->pin(0), and_gate->pin(0));
        circuit->connect_pins(in_1->pin(0), and_gate->pin(1));

        simulate_until_pin_change(circuit.get(), out->pin(0));
        REQUIRE(circuit->read_value(out->pin(0)) == VALUE_FALSE);
    }

}