// basic.h - Johan Smet - BSD-3-Clause (see LICENSE)
//
// Basic component parts

#ifndef LSIM_BASIC_H
#define LSIM_BASIC_H

#include <cstdint>
#include <vector>
#include <array>
#include <string>

class Circuit;

typedef uint32_t pin_t;
typedef uint32_t node_t;

const pin_t PIN_UNDEFINED = static_cast<pin_t>(-1);

enum Value {
    VALUE_FALSE         = 0,
    VALUE_TRUE          = 1,
    VALUE_UNDEFINED     = 2
};

inline Value negate_value(Value input) {
    if (input == VALUE_TRUE) {
        return VALUE_FALSE; 
    } else if (input == VALUE_FALSE) {
        return VALUE_TRUE;
    } else {
        return VALUE_UNDEFINED;
    }
}

class Component {
public:
    Component(Circuit *circuit, size_t pin_count);

    pin_t pin(uint32_t index);
    size_t num_pins() const {return m_pins.size();}

    virtual void tick();
    virtual void process() = 0;

private:
    bool is_dirty() const;

protected:
    Circuit *m_circuit;
    std::vector<pin_t>  m_pins;
};

// connector - I/O between circuits
class Connector : public Component {
public:
    Connector(Circuit *circuit, const char *name, size_t data_bits);

    virtual void tick();
    virtual void process();

    void change_data(uint64_t data);

private: 
    uint64_t    m_data;
    bool        m_changed;
    std::string m_name;
};


#if 0

class Delay {
public:
    Delay(size_t delay);

    Value push_value(Value new_value);
    Value push_last();

private:
    // member variables
    std::deque<Value>  m_values;
};

#endif


#endif // LSIM_BASIC_H
