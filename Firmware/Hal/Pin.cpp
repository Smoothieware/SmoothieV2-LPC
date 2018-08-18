#include "Pin.h"
#include "StringUtils.h"

Pin::Pin()
{
    this->inverting = false;
    this->valid = false;
}

Pin::Pin(const char *s)
{
    this->inverting = false;
    this->valid = false;
    from_string(s);
}

Pin::Pin(const char *s, TYPE_T t)
{
    this->inverting = false;
    this->valid = false;
    if(from_string(s) != nullptr) {
        switch(t) {
            case AS_INPUT: as_input(); break;
            case AS_OUTPUT: as_output(); break;
        }
    }
}

Pin::~Pin()
{
    // TODO trouble is we copy pins so this would deallocate a used pin, see Robot actuator pins
    // deallocate it in the bitset, but leaves th ephysical port as it was
    // if(valid) {
    //     uint16_t port = gpiocfg >> GPIO_PORT_SHIFT;
    //     uint16_t pin = gpiocfg & GPIO_PIN_MASK;
    //     set_allocated(port, pin, false);
    // }
}

// bitset to indicate a port/pin has been configured
#include <bitset>
static std::bitset<256> allocated_pins;
bool Pin::set_allocated(uint8_t port, uint8_t pin, bool set)
{
    uint8_t n = (port * NUM_GPIO_PINS) + pin;

    if(!set) {
        // deallocate it
        allocated_pins.reset(n);
        return true;
    }

    if(!allocated_pins[n]) {
        // if not set yet then set it
        allocated_pins.set(n);
        return true;
    }

    // indicate it was already set
    return false;
}

// look up table to convert GPIO port/pin into a PINCONF
static const uint32_t port_pin_lut[NUM_GPIO_PORTS][NUM_GPIO_PINS] = {
    {
        PINCONF_GPIO0p0, PINCONF_GPIO0p1, PINCONF_GPIO0p2,  PINCONF_GPIO0p3,  PINCONF_GPIO0p4,  PINCONF_GPIO0p5,  PINCONF_GPIO0p6, PINCONF_GPIO0p7,
        PINCONF_GPIO0p8, PINCONF_GPIO0p9, PINCONF_GPIO0p10, PINCONF_GPIO0p11, PINCONF_GPIO0p12, PINCONF_GPIO0p13, PINCONF_GPIO0p14, PINCONF_GPIO0p15,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    }, //port 0

    {
        PINCONF_GPIO1p0,  PINCONF_GPIO1p1,  PINCONF_GPIO1p2,  PINCONF_GPIO1p3,  PINCONF_GPIO1p4,  PINCONF_GPIO1p5,  PINCONF_GPIO1p6,  PINCONF_GPIO1p7,
        PINCONF_GPIO1p8,  PINCONF_GPIO1p9,  PINCONF_GPIO1p10, PINCONF_GPIO1p11, PINCONF_GPIO1p12, PINCONF_GPIO1p13, PINCONF_GPIO1p14, PINCONF_GPIO1p15,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    }, //port 1

    {
        PINCONF_GPIO2p0,  PINCONF_GPIO2p1,  PINCONF_GPIO2p2,  PINCONF_GPIO2p3,  PINCONF_GPIO2p4,  PINCONF_GPIO2p5,  PINCONF_GPIO2p6,  PINCONF_GPIO2p7,
        PINCONF_GPIO2p8,  PINCONF_GPIO2p9,  PINCONF_GPIO2p10, PINCONF_GPIO2p11, PINCONF_GPIO2p12, PINCONF_GPIO2p13, PINCONF_GPIO2p14, PINCONF_GPIO2p15,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    }, //port 2

    {
        PINCONF_GPIO3p0,  PINCONF_GPIO3p1,  PINCONF_GPIO3p2,  PINCONF_GPIO3p3,  PINCONF_GPIO3p4,  PINCONF_GPIO3p5,  PINCONF_GPIO3p6,  PINCONF_GPIO3p7,
        PINCONF_GPIO3p8,  PINCONF_GPIO3p9,  PINCONF_GPIO3p10, PINCONF_GPIO3p11, PINCONF_GPIO3p12, PINCONF_GPIO3p13, PINCONF_GPIO3p14, PINCONF_GPIO3p15,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    }, //port 3

    {
        PINCONF_GPIO4p0,  PINCONF_GPIO4p1,  PINCONF_GPIO4p2,  PINCONF_GPIO4p3,  PINCONF_GPIO4p4,  PINCONF_GPIO4p5,  PINCONF_GPIO4p6,  PINCONF_GPIO4p7,
        PINCONF_GPIO4p8,  PINCONF_GPIO4p9,  PINCONF_GPIO4p10, PINCONF_GPIO4p11, PINCONF_GPIO4p12, PINCONF_GPIO4p13, PINCONF_GPIO4p14, PINCONF_GPIO4p15,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    }, //port 4

    {
        PINCONF_GPIO5p0,  PINCONF_GPIO5p1,  PINCONF_GPIO5p2,  PINCONF_GPIO5p3,  PINCONF_GPIO5p4,  PINCONF_GPIO5p5,  PINCONF_GPIO5p6,  PINCONF_GPIO5p7,
        PINCONF_GPIO5p8,  PINCONF_GPIO5p9,  PINCONF_GPIO5p10, PINCONF_GPIO5p11, PINCONF_GPIO5p12, PINCONF_GPIO5p13, PINCONF_GPIO5p14, PINCONF_GPIO5p15,
        PINCONF_GPIO5p16, PINCONF_GPIO5p17, PINCONF_GPIO5p18, PINCONF_GPIO5p19, PINCONF_GPIO5p20, PINCONF_GPIO5p21, PINCONF_GPIO5p22, PINCONF_GPIO5p23,
        PINCONF_GPIO5p24, PINCONF_GPIO5p25, PINCONF_GPIO5p26,
        0, 0, 0, 0, 0
    }, //port 5

    {
        PINCONF_GPIO6p0, PINCONF_GPIO6p1,  PINCONF_GPIO6p2,  PINCONF_GPIO6p3,  PINCONF_GPIO6p4,  PINCONF_GPIO6p5,  PINCONF_GPIO6p6,  PINCONF_GPIO6p7,
        PINCONF_GPIO6p8,  PINCONF_GPIO6p9,  PINCONF_GPIO6p10, PINCONF_GPIO6p11, PINCONF_GPIO6p12, PINCONF_GPIO6p13, PINCONF_GPIO6p14, PINCONF_GPIO6p15,
        PINCONF_GPIO6p16, PINCONF_GPIO6p17, PINCONF_GPIO6p18, PINCONF_GPIO6p19, PINCONF_GPIO6p20, PINCONF_GPIO6p21, PINCONF_GPIO6p22, PINCONF_GPIO6p23,
        PINCONF_GPIO6p24, PINCONF_GPIO6p25, PINCONF_GPIO6p26, PINCONF_GPIO6p27, PINCONF_GPIO6p28, PINCONF_GPIO6p29, PINCONF_GPIO6p30,
        0
    }, //port 6

    {
        PINCONF_GPIO7p0, PINCONF_GPIO7p1,  PINCONF_GPIO7p2,  PINCONF_GPIO7p3,  PINCONF_GPIO7p4,  PINCONF_GPIO7p5,  PINCONF_GPIO7p6,  PINCONF_GPIO7p7,
        PINCONF_GPIO7p8,  PINCONF_GPIO7p9,  PINCONF_GPIO7p10, PINCONF_GPIO7p11, PINCONF_GPIO7p12, PINCONF_GPIO7p13, PINCONF_GPIO7p14, PINCONF_GPIO7p15,
        PINCONF_GPIO7p16, PINCONF_GPIO7p17, PINCONF_GPIO7p18, PINCONF_GPIO7p19, PINCONF_GPIO7p20, PINCONF_GPIO7p21, PINCONF_GPIO7p22, PINCONF_GPIO7p23,
        PINCONF_GPIO7p24, PINCONF_GPIO7p25,
        0, 0, 0, 0, 0, 0
    } //port 7
};

// given the physical port and pin (P2.7) finds the GPIO port and pin (GPIO0[7])
static bool lookup_pin(uint16_t port, uint16_t pin, uint16_t& gpioport, uint16_t& gpiopin)
{
    for (int i = 0; i < NUM_GPIO_PORTS; ++i) {
        for (int j = 0; j < NUM_GPIO_PINS; ++j) {
            uint32_t v = port_pin_lut[i][j];
            if(v == 0) continue;
            if( ((v & PINCONF_PINS_MASK) >> PINCONF_PINS_SHIFT) == port && ((v & PINCONF_PIN_MASK) >> PINCONF_PIN_SHIFT) == pin ) {
                gpioport = i;
                gpiopin = j;
                return true;
            }
        }
    }

    return false;
}

// Make a new pin object from a string
// Pins are defined for the LPC43xx as GPIO names GPIOp[n] or gpiop_n where p is the GPIO port and n is the pin or as pin names eg P1_6 or P1.6
Pin* Pin::from_string(std::string value)
{
    valid = false;
    inverting = false;

    if(value == "nc") return nullptr;

    uint16_t port = 0;
    uint16_t pin = 0;
    size_t pos = 0;
    if(stringutils::toUpper(value.substr(0, 4)) == "GPIO") {
        // grab first integer as GPIO port.
        port = strtol(value.substr(4).c_str(), nullptr, 10);
        pos = value.find_first_of("[_", 4);
        if(pos == std::string::npos) return nullptr;

        // grab pin number
        pin = strtol(value.substr(pos + 1).c_str(), nullptr, 10);

    } else if(stringutils::toUpper(value.substr(0, 1)) == "P") {
        uint16_t x = strtol(value.substr(1).c_str(), nullptr, 16);
        pos = value.find_first_of("._", 1);
        if(pos == std::string::npos) return nullptr;
        uint16_t y = strtol(value.substr(pos + 1).c_str(), nullptr, 10);

        // Pin name convert to GPIO
        if(!lookup_pin(x, y, port, pin)) return nullptr;

    } else {
        return nullptr;
    }

    if(port >= NUM_GPIO_PORTS || pin >= NUM_GPIO_PINS) return nullptr;

    if(!set_allocated(port, pin)) {
        printf("WARNING: GPIO%d[%d] has already been allocated\n", port, pin);
    }

    // convert port and pin to a GPIO and setup as a GPIO
    uint32_t gpio = port_pin_lut[port][pin];
    if(gpio == 0) return nullptr; // not a valid pin

    // now check for modifiers:-
    // ! = invert pin
    // o = set pin to open drain
    // ^ = set pin to pull up
    // v = set pin to pull down
    // - = set pin to no pull up or down
    for(char c : value.substr(pos + 1)) {
        switch(c) {
            case '!':
                this->inverting = true;
                break;
            case 'o':
                gpio |= PINCONF_FLOAT;
                break;
            case '^':
                gpio |= PINCONF_PULLUP;
                break;
            case 'v':
                gpio |= PINCONF_PULLDOWN;
                break;
            case '-':
                break;
        }
    }
    lpc43_pin_config(gpio); //configures pin for GPIO
    gpiocfg = (port << GPIO_PORT_SHIFT) | (pin << GPIO_PIN_SHIFT);

    this->valid = true;
    return this;
}

std::string Pin::to_string() const
{
    if(valid) {
        uint16_t port = gpiocfg >> GPIO_PORT_SHIFT;
        uint16_t pin = gpiocfg & GPIO_PIN_MASK;

        std::string s("gpio");
        s.append(std::to_string(port)).append("_").append(std::to_string(pin));

        uint32_t v = port_pin_lut[port][pin];
        port = ((v & PINCONF_PINS_MASK) >> PINCONF_PINS_SHIFT);
        pin = ((v & PINCONF_PIN_MASK) >> PINCONF_PIN_SHIFT);
        const char *digits = "0123456789abcdef";
        s.append("(p");
        s.push_back(digits[port]);
        s.push_back('_');
        s.append(std::to_string(pin)).append(")");
        return s;

    } else {
        return "invalid";
    }
}

Pin* Pin::as_output()
{
    if(valid) {
        lpc43_gpio_config(gpiocfg | GPIO_MODE_OUTPUT);
        return this;
    }

    return nullptr;
}

Pin* Pin::as_input()
{
    if(valid) {
        lpc43_gpio_config(gpiocfg | GPIO_MODE_INPUT);
        return this;
    }

    return nullptr;
}

#if 0
mbed::InterruptIn* Pin::interrupt_pin()
{
    if(!this->valid) return nullptr;
    /*
        // set as input
        as_input();

        if (port_number == 0 || port_number == 2) {
            PinName pinname = port_pin((PortName)port_number, pin);
            return new mbed::InterruptIn(pinname);

        }else{
            this->valid= false;
            return nullptr;
        }
    */
    return nullptr;
}
#endif
