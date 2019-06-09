#include "main_gui.h"
#include "imgui_ex.h"

#include "component_ui.h"
#include "component_std.h"

#include "lsim_context.h"
#include "circuit_instance.h"
#include "load_logisim.h"
#include "colors.h"
#include "serialize.h"
#include "file_selector.h"

#include <list>

namespace {

using namespace lsim;
using namespace lsim::gui;

static UICircuit::uptr_t ui_circuit = nullptr;
static std::unique_ptr<CircuitInstance> circuit_instance = nullptr;
static std::list<UICircuit::uptr_t> sub_circuit_drill_downs;

static std::string ui_filename = "";
static int selected_circuit_idx = 0;
static const char *value_labels[] = {"False", "True", "Undefined", "Error"};

static void change_active_circuit(LSimContext *context, CircuitDescription *active_circuit) {
	if (ui_circuit != nullptr && ui_circuit->circuit_desc() == active_circuit) {
		return;
	}

	if (active_circuit) {
		ui_circuit = UICircuitBuilder::create_circuit(active_circuit);
		selected_circuit_idx = context->user_library()->circuit_idx(active_circuit);
	}
}

static void close_circuit_library(LSimContext *lsim_context) {
	sub_circuit_drill_downs.clear();
	circuit_instance = nullptr;
	ui_circuit = nullptr;
	lsim_context->user_library()->clear_circuits();
}

static void load_circuit_library(LSimContext *lsim_context, const std::string &filename) {

	if (ui_circuit != nullptr) {
		close_circuit_library(lsim_context);
	}

	ui_filename = filename;
	auto ext = ui_filename.substr(ui_filename.find_last_of('.') + 1);

	if (ext == "circ") {
		// auto circuit = load_logisim(lsim_context, circuit_file);
		// ui_filename.replace(ui_filename.begin() + ui_filename.find_last_of('.'), ui_filename.end(), ".lsim");
	} else {
		deserialize_library(lsim_context, lsim_context->user_library(), filename.c_str());
		if (lsim_context->user_library()->num_circuits() <= 0) {
			lsim_context->user_library()->create_circuit("main", lsim_context);
			lsim_context->user_library()->change_main_circuit("main");
		}
	}

	// select main circuit
	auto main_circuit = lsim_context->user_library()->main_circuit();
	selected_circuit_idx = lsim_context->user_library()->circuit_idx(main_circuit);
	change_active_circuit(lsim_context, main_circuit);
}

static void save_circuit_library(LSimContext *lsim_context) {
	if (ui_filename.empty()) {
		ImGui::OpenPopup("Library name");
	} else {
		serialize_library(lsim_context, lsim_context->user_library(), ui_filename.c_str());
	}
}

static void ui_popup_library_save_name(LSimContext *lsim_context) {
	static char buffer[512] = "";

	if (ImGui::BeginPopupModal("Library name")) {
		ImGui::InputText("Filename", buffer, sizeof(buffer) / sizeof(buffer[0]));
		if (ImGui::Button("Ok")) {
			ui_filename = buffer;
			buffer[0] = '\0';
			save_circuit_library(lsim_context);
			ImGui::CloseCurrentPopup();
		}
		if (ImGui::Button("Cancel")) {
			buffer[0] = '\0';
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

static void init_input_connectors(CircuitDescription *desc, CircuitInstance *inst) {
	auto connector_ids = desc->component_ids_of_type(COMPONENT_CONNECTOR_IN);

	for (const auto &id : connector_ids) {
		auto connector = desc->component_by_id(id);
		auto pin = connector->output_pin_id(0);
		if (!connector->property_value("tri_state", false)) {
			for (size_t idx = 0; idx < connector->num_outputs(); ++idx) {
				inst->write_pin(pin++, VALUE_FALSE);
			}
		}
	}
}

static void ui_circuit_management(LSimContext *context) {

	auto lib = context->user_library();

	// move up
	if (ImGui::Button("Up")) {
		if (selected_circuit_idx < lib->num_circuits() && selected_circuit_idx > 0) {
			lib->swap_circuits(selected_circuit_idx, selected_circuit_idx - 1);
			selected_circuit_idx -= 1;
		}
	}

	// move down
	ImGui::SameLine();
	if (ImGui::Button("Down")) {
		if (selected_circuit_idx < lib->num_circuits() - 1) {
			lib->swap_circuits(selected_circuit_idx, selected_circuit_idx + 1);
			selected_circuit_idx += 1;
		}
	}

	// add a new circuit to the library
	ImGui::SameLine();
	if (ImGui::Button("Add")) {
		auto new_name = std::string("circuit#") + std::to_string(lib->num_circuits()+1);
		auto circuit  = lib->create_circuit(new_name.c_str(), context);
		if (!lib->main_circuit()) {
			lib->change_main_circuit(circuit->name().c_str());
		}
		change_active_circuit(context, circuit);
	}

	// remove circuit from library if allowed
	if (lib->num_circuits() > 1 && lib->main_circuit() != lib->circuit_by_idx(selected_circuit_idx)) {
		ImGui::SameLine();
		if (ImGui::Button("Delete")) {
			lib->delete_circuit(lib->circuit_by_idx(selected_circuit_idx));
			change_active_circuit(context, lib->main_circuit());
		}
	}

	// list of circuits in the user library
	ImGui::Separator();

	for (size_t i = 0; i < lib->num_circuits(); ++i) {
		auto circuit = lib->circuit_by_idx(i);
		if (ImGui::Selectable(circuit->name().c_str(), selected_circuit_idx == i)) {
			change_active_circuit(context, circuit);
		}
	}

	ImGui::Separator();
}

static void ui_component_pallette(LSimContext *context) {

	auto add_component_button = [](uint32_t component, const char *caption, Component * (create_func)(CircuitDescription *circuit)) {
		Point pos = ImGui::GetCursorScreenPos();
		auto draw_list = ImGui::GetWindowDrawList();

		ImGui::PushID(caption);
		if (ImGui::Button("", {40, 40})) {
			if (!ui_circuit->is_simulating()) {
				auto component = create_func(ui_circuit->circuit_desc());
				component->set_position({-200, -200});
				ui_circuit->ui_create_component(component);
			}
		}
		auto icon = ComponentIcon::cached(component);
		if (icon) {
			icon->draw(pos + Point(20,20), {34, 34}, draw_list, 1, COLOR_COMPONENT_BORDER);
		}
		ImGuiEx::Text(pos + Point(50,20), caption, ImGuiEx::TAH_LEFT, ImGuiEx::TAV_CENTER);
		ImGui::SetCursorScreenPos(pos + Point(0,42));
		ImGui::PopID();
	};

	ImGui::Spacing();
	if (ImGui::TreeNodeEx("Gates", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_NoTreePushOnOpen)) {
		ImGui::BeginGroup();
		ImGui::Indent();
		add_component_button(COMPONENT_AND_GATE, "AND", [](CircuitDescription *circuit) {return circuit->add_and_gate(2);});
		add_component_button(COMPONENT_OR_GATE, "OR", [](CircuitDescription *circuit) {return circuit->add_or_gate(2);});
		add_component_button(COMPONENT_NOT_GATE, "NOT", [](CircuitDescription *circuit) {return circuit->add_not_gate();});
		add_component_button(COMPONENT_NAND_GATE, "NAND", [](CircuitDescription *circuit) {return circuit->add_nand_gate(2);});
		add_component_button(COMPONENT_NOR_GATE, "NOR", [](CircuitDescription *circuit) {return circuit->add_nor_gate(2);});
		add_component_button(COMPONENT_XOR_GATE, "XOR", [](CircuitDescription *circuit) {return circuit->add_xor_gate();});
		add_component_button(COMPONENT_XNOR_GATE, "XNOR", [](CircuitDescription *circuit) {return circuit->add_xnor_gate();});
		add_component_button(COMPONENT_BUFFER, "Buffer", [](CircuitDescription *circuit) {return circuit->add_buffer(1);});
		add_component_button(COMPONENT_TRISTATE_BUFFER, "TriState Buffer", [](CircuitDescription *circuit) {return circuit->add_tristate_buffer(1);});
		ImGui::EndGroup();
	}

	ImGui::Spacing();
	if (ImGui::TreeNodeEx("Various", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_NoTreePushOnOpen)) {
		ImGui::BeginGroup();
		ImGui::Indent();
		add_component_button(COMPONENT_CONNECTOR_IN, "Input", [](CircuitDescription *circuit) {return circuit->add_connector_in("in", 1);});
		add_component_button(COMPONENT_CONNECTOR_OUT, "Output", [](CircuitDescription *circuit) {return circuit->add_connector_out("out", 1);});
		add_component_button(COMPONENT_CONSTANT, "Constant", [](CircuitDescription *circuit) {return circuit->add_constant(VALUE_TRUE);});
		add_component_button(COMPONENT_PULL_RESISTOR, "PullResistor", [](CircuitDescription *circuit) {return circuit->add_pull_resistor(VALUE_TRUE);});
		ImGui::EndGroup();
	}
}

static void ui_property_panel(LSimContext *context) {

	if (ui_circuit != nullptr && ui_circuit->selected_component()) {
		auto ui_comp = ui_circuit->selected_component();
		auto component = ui_comp->component();

		// helper functions
		auto text_property = [](const char *caption, Property *property) -> bool {
			std::string value = property->value_as_string();
			std::vector<char> buffer(value.begin(), value.end());
			buffer.resize(256);

			if (ImGui::InputText(caption, buffer.data(), buffer.size(), ImGuiInputTextFlags_EnterReturnsTrue)) {
				property->value(buffer.data());
				return true;
			} else {
				return false;
			}
		};

		auto boolean_property = [](const char *caption, Property *property) {
			bool value = property->value_as_boolean();
			if (ImGui::Checkbox(caption, &value)) {
				property->value(value);
			}
		};

		auto value_property = [](const char *caption, Property *property) -> Value {
			int cur_value = property->value_as_integer();
			if (ImGui::Combo(caption, &cur_value, value_labels, 4)) {
				property->value(static_cast<int64_t>(cur_value));
			}
			return static_cast<Value>(cur_value);
		};

		// orientation - present for all components
		int cur_orientation = component->angle() / 90;
		const char *orientations[] = {"East", "South", "West", "North"};
		if (ImGui::Combo("Orientation", &cur_orientation, orientations, sizeof(orientations) / sizeof(orientations[0]))) {
			component->set_angle(cur_orientation * 90);
			ui_comp->build_transform();
		}

		// component specific fields
		if (component->type() == COMPONENT_CONNECTOR_IN || component->type() == COMPONENT_CONNECTOR_OUT) {
			if (text_property("Name", component->property("name"))) {
				ui_circuit->circuit_desc()->rebuild_port_list();
			}
			boolean_property("TriState", component->property("tri_state"));

			int data_bits = component->num_inputs() + component->num_outputs();
			int org_bits = data_bits;
			if (ImGui::InputInt("Data Bits", &data_bits)) {
				if (data_bits <= 0) {
					data_bits = 1;
				}
				ui_circuit->circuit_desc()->change_port_pin_count(component->id(), data_bits);

				// update the position of the connector so the endpoints do not move
				auto dir = (component->angle() == 0 || component->angle() == 180) ? Point(0,10) : Point(10,0);
				component->set_position(component->position() + (dir * (data_bits - org_bits)));
				ui_comp->build_transform();

				UICircuitBuilder::rematerialize_component(ui_circuit.get(), ui_comp);
				ui_circuit->fix_component_connections(ui_comp);
			}
		}

		if (component->type() == COMPONENT_CONSTANT) {
			auto new_value = value_property("Value", component->property("value"));
		}

		if (component->type() == COMPONENT_PULL_RESISTOR) {
			auto new_value = value_property("Value", component->property("pull_to"));
		}

		if (component->type() == COMPONENT_BUFFER || component->type() == COMPONENT_TRISTATE_BUFFER) {
			int data_bits = component->num_inputs();

			if (ImGui::InputInt("Data Bits", &data_bits)) {
				if (data_bits <= 0) {
					data_bits = 1;
				}
				component->change_input_pins(data_bits);
				component->change_output_pins(data_bits);
				UICircuitBuilder::rematerialize_component(ui_circuit.get(), ui_comp);
			}
		}

		if (component->type() == COMPONENT_AND_GATE ||
		    component->type() == COMPONENT_OR_GATE ||
			component->type() == COMPONENT_NAND_GATE ||
			component->type() == COMPONENT_NOR_GATE) {
			int num_inputs = component->num_inputs();
			if (ImGui::SliderInt("Inputs", &num_inputs, 2, 8)) {
				component->change_input_pins(num_inputs);
				UICircuitBuilder::rematerialize_component(ui_circuit.get(), ui_comp);
			}
		}

	} else if (ui_circuit != nullptr) {
		// no component selected - edit circuit properties
		std::vector<char> name(ui_circuit->circuit_desc()->name().begin(), ui_circuit->circuit_desc()->name().end());
		name.resize(256);

		if (ImGui::InputText("Name", name.data(), name.size(), ImGuiInputTextFlags_EnterReturnsTrue)) {
			context->user_library()->rename_circuit(ui_circuit->circuit_desc(), name.data());
		}

		bool main_circuit = context->user_library()->main_circuit() == ui_circuit->circuit_desc();
		if (main_circuit) {
			ImGui::Text("This is the main circuit");
		} else {
			if (ImGui::Button("Set as main circuit")) {
				context->user_library()->change_main_circuit(ui_circuit->circuit_desc()->name().c_str());
			}
		}
	}
}

} // unnamed namespace

namespace lsim {

namespace gui {

void main_gui_setup(LSimContext *lsim_context, const char *circuit_file) {
	component_register_basic();
	component_register_extra();
	component_register_gates();

	// try to load the circuit specified on the command line
	if (circuit_file) {
		load_circuit_library(lsim_context, circuit_file);
	}
}

void main_gui(LSimContext *lsim_context)
{
	auto sim = lsim_context->sim();
	auto lib = lsim_context->user_library();

	static bool sim_running = false;
	static bool show_file_selector = false;

	///////////////////////////////////////////////////////////////////////////
	//
	// control window
	//

	ImGui::SetNextWindowPos({0,0}, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize({268, ImGui::GetIO().DisplaySize.y}, ImGuiCond_FirstUseEver);

	ImGui::Begin("Control");

		// Library management
		ui_popup_library_save_name(lsim_context);

		if (ImGui::Button("New")) {
			load_circuit_library(lsim_context, "");
		}
		ImGui::SameLine();
		if (ImGui::Button("Load")) {
			ui_file_selector_init("examples");
			show_file_selector = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("Save")) {
			save_circuit_library(lsim_context);
		}
		ImGui::SameLine();
		if (ImGui::Button("Add Library")) {
		}

		if (show_file_selector) {
			std::string selection;
			show_file_selector = ui_file_selector(&selection);
			if (!show_file_selector && !selection.empty()) {
				load_circuit_library(lsim_context, selection);
			}
		}

		// Circuit management
		ImGui::Spacing();

		if (ImGui::CollapsingHeader("Circuits", ImGuiTreeNodeFlags_DefaultOpen)) {
			ui_circuit_management(lsim_context);
		}

		// Component pallette
		ImGui::Spacing();
		if (ImGui::CollapsingHeader("Components", ImGuiTreeNodeFlags_DefaultOpen)) {
			ui_component_pallette(lsim_context);
		}

		// Properties
		ImGui::Spacing();
		if (ImGui::CollapsingHeader("Properties", ImGuiTreeNodeFlags_DefaultOpen)) {
			ui_property_panel(lsim_context);
		}

	ImGui::End();

	///////////////////////////////////////////////////////////////////////////
	//
	// main circuit window
	//

	ImGui::SetNextWindowPos({268, 0}, ImGuiSetCond_FirstUseEver);
	ImGui::SetNextWindowSize({ImGui::GetIO().DisplaySize.x-268, ImGui::GetIO().DisplaySize.y}, ImGuiSetCond_FirstUseEver);
	ImGui::Begin("Circuit", nullptr, ImGuiWindowFlags_NoScrollWithMouse);

		if (ImGui::RadioButton("Editor", !ui_circuit->is_simulating())) {
			circuit_instance = nullptr;
			ui_circuit->set_simulation_instance(nullptr);
		}
		ImGui::SameLine();

		if (ImGui::RadioButton("Simulation", ui_circuit->is_simulating())) {
			circuit_instance = ui_circuit->circuit_desc()->instantiate(lsim_context->sim());
			ui_circuit->set_simulation_instance(circuit_instance.get());
			init_input_connectors(ui_circuit->circuit_desc(), ui_circuit->circuit_inst());
			sim_running = true;
			sim->init();
		}

		if (ui_circuit->is_simulating()) {
			ImGui::SameLine();
			ImGui::Checkbox("Run simulation", &sim_running);
			ImGui::SameLine();
			if (ImGui::Button("Reset simulation")) {
				sim->init();
			}
			ImGui::SameLine();
			if (ImGui::Button("Step")) {
				sim->step();
			}
		}

		if (sim_running) {
			sim->step();
		}

		if (ui_circuit) {
			ui_circuit->draw();
		}

	ImGui::End();

	// display windows for drill-downs
	auto dd_it = sub_circuit_drill_downs.begin();
	while (dd_it != sub_circuit_drill_downs.end()) {

		auto drill_down = dd_it->get();
		bool keep_open = true;

		ImGui::SetNextWindowSize(drill_down->circuit_dimensions() + Point(50,50), ImGuiCond_Appearing);
		ImGui::Begin(drill_down->circuit_inst()->name(), &keep_open, ImGuiWindowFlags_NoScrollWithMouse);
			drill_down->draw();
		ImGui::End();

		if (!keep_open) {
			dd_it = sub_circuit_drill_downs.erase(dd_it);
		} else {
			++dd_it;
		}
	}
}

void main_gui_drill_down_sub_circuit(CircuitInstance *parent_inst, Component *comp) {
	auto nested_desc = comp->nested_circuit();
	auto nested_inst = parent_inst->component_by_id(comp->id())->nested_instance();

	auto sub_circuit = UICircuitBuilder::create_circuit(nested_desc);
	sub_circuit->set_simulation_instance(nested_inst, true);

	sub_circuit_drill_downs.push_front(std::move(sub_circuit));
}

} // namespace lsim

} // namespace gui