#include "catch.hpp"
#include "load_logisim.h"
#include "circuit.h"

#include <cstring>

const char *logisim_test_data = R"FILE(
<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<project source="2.15.0" version="1.0">
This file is intended to be loaded by Logisim-evolution (https://github.com/reds-heig/logisim-evolution).
<lib desc="#Wiring" name="0"/>
  <lib desc="#Gates" name="1"/>
  <lib desc="#Plexers" name="2">
    <tool name="Multiplexer">
      <a name="enable" val="false"/>
    </tool>
    <tool name="Demultiplexer">
      <a name="enable" val="false"/>
    </tool>
  </lib>
  <lib desc="#Arithmetic" name="3"/>
  <lib desc="#Memory" name="4">
    <tool name="ROM">
      <a name="contents">addr/data: 8 8
0
</a>
    </tool>
  </lib>
  <lib desc="#I/O" name="5"/>
  <lib desc="#TTL" name="6"/>
  <main name="test"/>
  <options>
    <a name="gateUndefined" val="ignore"/>
    <a name="simlimit" val="1000"/>
    <a name="simrand" val="0"/>
    <a name="tickmain" val="half_period"/>
  </options>
  <mappings>
    <tool lib="9" map="Button2" name="Menu Tool"/>
    <tool lib="9" map="Button3" name="Menu Tool"/>
    <tool lib="9" map="Ctrl Button1" name="Menu Tool"/>
  </mappings>
  <toolbar>
    <tool lib="9" name="Poke Tool"/>
    <tool lib="9" name="Edit Tool"/>
    <tool lib="9" name="Text Tool">
      <a name="text" val=""/>
      <a name="font" val="SansSerif plain 12"/>
      <a name="halign" val="center"/>
      <a name="valign" val="base"/>
    </tool>
    <sep/>
    <tool lib="0" name="Pin"/>
    <tool lib="0" name="Pin">
      <a name="facing" val="west"/>
      <a name="output" val="true"/>
    </tool>
    <tool lib="1" name="NOT Gate"/>
    <tool lib="1" name="AND Gate"/>
    <tool lib="1" name="OR Gate"/>
  </toolbar>
  <circuit name="test">
    <a name="circuit" val="test"/>
    <a name="clabel" val=""/>
    <a name="clabelup" val="east"/>
    <a name="clabelfont" val="SansSerif bold 16"/>
    <a name="circuitnamedbox" val="true"/>
    <a name="circuitnamedboxfixedsize" val="true"/>
    <a name="circuitvhdlpath" val=""/>
    <wire from="(190,170)" to="(220,170)"/>
    <wire from="(240,170)" to="(270,170)"/>
    <wire from="(300,170)" to="(320,170)"/>
    <comp lib="1" loc="(240,170)" name="Buffer"/>
    <comp lib="0" loc="(190,170)" name="Pin">
      <a name="label" val="In"/>
    </comp>
    <comp lib="1" loc="(300,170)" name="NOT Gate"/>
    <comp lib="0" loc="(320,170)" name="Pin">
      <a name="facing" val="west"/>
      <a name="output" val="true"/>
      <a name="label" val="Out"/>
    </comp>
  </circuit>
</project>
)FILE";

TEST_CASE ("Small Logisim Circuit", "[logisim]") {

    auto circuit = std::make_unique<Circuit>();
    REQUIRE(circuit);

    REQUIRE(load_logisim(circuit.get(), logisim_test_data, std::strlen(logisim_test_data)));

    auto in = circuit->component_by_name("In");
    REQUIRE(in);

    auto out = circuit->component_by_name("Out");
    REQUIRE(out);

    dynamic_cast<Connector *>(in)->change_data(VALUE_TRUE);
    circuit->simulation_until_pin_change(out->pin(0));
    REQUIRE(circuit->read_value(out->pin(0)) == VALUE_FALSE);
}