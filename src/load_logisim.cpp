// load_logisim.cpp - Johan Smet - BSD-3-Clause (see LICENSE)
//
// Load circuit from a LogiSim circuit

#include <pugixml.hpp>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <cassert>

#include "circuit.h"
#include "gate.h"

#define DEF_REQUIRED_ATTR(res, node, attr) \
    auto attr_##attr = (node).attribute(#attr); \
    if (!attr_##attr) { \
        return false;   \
    }                   \
    std::string res = attr_##attr.value();

class LogisimParser {
public:
    LogisimParser(pugi::xml_document *xml_doc, Circuit *circuit) : 
            m_xml_doc(xml_doc), 
            m_circuit(circuit) {
    }

    bool parse_xml();

private:
    union Position {
        struct {
            int32_t m_x;
            int32_t m_y;
        };
        int64_t m_full;
    };

    enum LogisimDirection {
        LS_NORTH,
        LS_EAST,
        LS_SOUTH,
        LS_WEST
    };

    typedef std::vector<int64_t>        wire_node_t;
    typedef std::vector<wire_node_t>    wire_container_t;

private:
    bool parse_component(pugi::xml_node &comp_node);
    bool parse_wire(pugi::xml_node &wire_node);
    bool connect_components();
    bool parse_location(const std::string &loc_string, Position &pos);
    void add_pin_location(pin_t pin, const Position &loc);
    Position input_pin_location(Position base, size_t index, 
                                size_t num_inputs, size_t comp_size,
                                LogisimDirection direction = LS_EAST,
                                bool negate_output = false,
                                bool negate_input = false);
    wire_node_t *point_on_wire(Position position);

private: 
    pugi::xml_document *m_xml_doc;
    Circuit *m_circuit;
    std::unordered_map<int64_t, pin_t> m_pin_locs;
    wire_container_t m_wires;
};

bool LogisimParser::parse_xml() {

    auto project_node = m_xml_doc->child("project");
    if (!project_node) {
        return false;
    }

    auto circuit_node = project_node.child("circuit");
    if (!circuit_node) {
        return false;
    }

    /* iterate all components */
    for (auto comp : circuit_node.children("comp")) {
        parse_component(comp);
    }

    /* iterate all wires */
    for (auto wire : circuit_node.children("wire")) {
        parse_wire(wire);
    }

    /* connect components */
    connect_components();

    return true;
}

bool LogisimParser::parse_component(pugi::xml_node &comp_node) {
    DEF_REQUIRED_ATTR(comp_type, comp_node, name);
    DEF_REQUIRED_ATTR(comp_loc, comp_node, loc);

    // parse location
    Position location = {0, 0};
    
    if (!parse_location(comp_loc, location)) {
        return false;
    }

    size_t comp_size = 20;
    auto comp_facing = LS_EAST;
    std::string comp_label = "";

    for (auto prop : comp_node.children("a")) {
        DEF_REQUIRED_ATTR(prop_name, prop, name);
        DEF_REQUIRED_ATTR(prop_val, prop, val);

        if (prop_name == "label") {
            comp_label = prop_val;
        }
    }

    Component *component = nullptr;
    if (comp_type == "Buffer") {
        component = m_circuit->create_component<Buffer>(1);
        add_pin_location(component->pin(0), input_pin_location(location, 0, 1, comp_size, comp_facing));
        add_pin_location(component->pin(1), location);
    } else if (comp_type == "Pin") {
        component = m_circuit->create_component<Connector>(1);
        add_pin_location(component->pin(0), location);
    } else if (comp_type == "AND Gate") {
        component = m_circuit->create_component<AndGate>(2);
        auto num_inputs = component->num_pins() - 1;
        for (auto idx = 0u; idx < num_inputs; ++idx) {
           add_pin_location(component->pin(idx), input_pin_location(location, idx, num_inputs, comp_size, comp_facing, false));
        }
        add_pin_location(component->pin(component->num_pins() - 1), location);
    } else if (comp_type == "OR Gate") {
        component = m_circuit->create_component<OrGate>(2);
        auto num_inputs = component->num_pins() - 1;
        for (auto idx = 0u; idx < num_inputs; ++idx) {
           add_pin_location(component->pin(idx), input_pin_location(location, idx, num_inputs, comp_size, comp_facing, false));
        }
        add_pin_location(component->pin(component->num_pins() - 1), location);
    } else if (comp_type == "NOT Gate") {
        component = m_circuit->create_component<NotGate>();
        add_pin_location(component->pin(0), input_pin_location(location, 0, 1, comp_size, comp_facing, true));
        add_pin_location(component->pin(1), location);
    } else if (comp_type == "NAND Gate") {
        component = m_circuit->create_component<AndGate>(2);
        auto num_inputs = component->num_pins() - 1;
        for (auto idx = 0u; idx < num_inputs; ++idx) {
           add_pin_location(component->pin(idx), input_pin_location(location, idx, num_inputs, comp_size, comp_facing, true));
        }
        add_pin_location(component->pin(component->num_pins() - 1), location);
    } else if (comp_type == "NOR Gate") {
        component = m_circuit->create_component<OrGate>(2);
        auto num_inputs = component->num_pins() - 1;
        for (auto idx = 0u; idx < num_inputs; ++idx) {
           add_pin_location(component->pin(idx), input_pin_location(location, idx, num_inputs, comp_size, comp_facing, true));
        }
        add_pin_location(component->pin(component->num_pins() - 1), location);
    } else if (comp_type == "XOR Gate") {
        component = m_circuit->create_component<XorGate>();
        add_pin_location(component->pin(2), location);
    } else {
        return false;
    }

    if (!comp_label.empty()) {
        m_circuit->register_component_name(comp_label.c_str(), component);
    }

    return true;
}

bool LogisimParser::parse_wire(pugi::xml_node &wire_node) {
    DEF_REQUIRED_ATTR(wire_from, wire_node, from);
    DEF_REQUIRED_ATTR(wire_to, wire_node, to);

    Position w1, w2;
    if (!parse_location(wire_from, w1) || 
        !parse_location(wire_to, w2)) {
        return false;
    }

    // check if wire should be merged with existing wire
    auto node_1 = point_on_wire(w1);
    auto node_2 = point_on_wire(w2);

    if (!node_1 && !node_2) {
        wire_node_t new_node = {w1.m_full, w2.m_full};
        m_wires.push_back(new_node);
        return true;
    }

    if (node_1 && node_2) {
        // wire connects two existing nodes (merge node_2 into node_1)
        for (auto &w : *node_2) {
            node_1->push_back(w);
        }
        node_2->clear();
        m_wires.erase(std::find(std::begin(m_wires), std::end(m_wires), *node_2));
        return true;
    }

    if (node_1 && !node_2) {
        node_1->push_back(w2.m_full);
        return true;
    }

    if (!node_1 && node_2) {
        node_2->push_back(w1.m_full);
        return true;
    }

    return false;
}

bool LogisimParser::connect_components() {

    for (const auto &node : m_wires) {
        pin_t pin_1 = PIN_UNDEFINED;

        for (const auto &point : node) {
            auto pin = m_pin_locs.find(point);
            if (pin != std::end(m_pin_locs)) {
                if (pin_1 == PIN_UNDEFINED) {
                    pin_1 = pin->second;
                } else {
                    m_circuit->connect_pins(pin_1, pin->second);
                }
            }
        }
    }

    return true;
}

bool LogisimParser::parse_location(const std::string &loc_string, Position &pos) {
    if (loc_string.front() != '(' || loc_string.back() != ')') {
        return false;
    }

    auto comma = loc_string.find_first_of(",");
    if (comma == loc_string.npos) {
        return false;
    }

    pos.m_x = std::stoi(loc_string.substr(1, comma - 1));
    pos.m_y = std::stoi(loc_string.substr(comma + 1));

    return true;
}

void LogisimParser::add_pin_location(pin_t pin, const Position &loc) {
    m_pin_locs[loc.m_full] = pin;
}

LogisimParser::Position LogisimParser::input_pin_location( 
        Position base, size_t index, 
        size_t num_inputs, size_t comp_size,
        LogisimDirection direction,
        bool negate_output,
        bool negate_input) {

    int axis_length = comp_size + (negate_output ? 10 : 0);

    int skipStart;
    int skipDist;
    int skipLowerEven = 10;

	if (num_inputs <= 3) {
        if (comp_size < 40) {
            skipStart = -5;
            skipDist = 10;
            skipLowerEven = 10;
        } else if (comp_size < 60 || num_inputs <= 2) {
            skipStart = -10;
            skipDist = 20;
            skipLowerEven = 20;
        } else {
            skipStart = -15;
            skipDist = 30;
            skipLowerEven = 30;
        }
    } else if (num_inputs == 4 && comp_size >= 60) {
        skipStart = -5;
        skipDist = 20;
        skipLowerEven = 0;
    } else {
        skipStart = -5;
        skipDist = 10;
        skipLowerEven = 10;
    }

	int dy;
	if ((num_inputs & 1) == 1) {
        dy = skipStart * (num_inputs - 1) + skipDist * index;
    } else {
        dy = skipStart * num_inputs + skipDist * index;
        if (index >= num_inputs / 2)
            dy += skipLowerEven;
    }

	int dx = axis_length + (negate_input ? 10 : 0);

	if (direction == LS_NORTH) {
        return {base.m_x + dy, base.m_y + dx};
    } else if (direction == LS_SOUTH) {
        return {base.m_x + dy, base.m_y - dx};
    } else if (direction == LS_WEST) {
        return {base.m_x + dx, base.m_y + dy};
    } else {
        return {base.m_x - dx, base.m_y + dy};
    }
}

LogisimParser::wire_node_t *LogisimParser::point_on_wire(Position position) {

    for (auto &node : m_wires) {
        auto res = std::find(std::begin(node), std::end(node), position.m_full);
        if (res != std::end(node)) {
            return &node;
        }
    }

    return nullptr;
}

//
// interface
//

bool load_logisim(Circuit *circuit, const char *filename) {
    pugi::xml_document xml_doc;

    auto result = xml_doc.load_file(filename);
    if (!result) {
        return false;
    }

    auto parser = LogisimParser(&xml_doc, circuit);
    return parser.parse_xml();
}

bool load_logisim(Circuit *circuit, const char *data, size_t len) {
    pugi::xml_document xml_doc;

    auto result = xml_doc.load_buffer(data, len);
    if (!result) {
        return false;
    }

    auto parser = LogisimParser(&xml_doc, circuit);
    return parser.parse_xml();
}