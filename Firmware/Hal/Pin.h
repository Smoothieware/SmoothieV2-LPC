#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <bitset>

#include "lpc43_pinconfig.h"
#include "lpc43_gpio.h"

#define putreg8(v,a)   (*(volatile uint8_t *)(a) = (v))
#define getreg8(a)     (*(volatile uint8_t *)(a))

class Pin
{
public:
    Pin();
    Pin(const char *s);
    virtual ~Pin();

    enum TYPE_T {AS_INPUT, AS_OUTPUT};
    Pin(const char *s, Pin::TYPE_T);


    Pin* from_string(std::string value);
    std::string to_string() const;

    bool connected() const
    {
        return this->valid;
    }

    Pin* as_output();
    Pin* as_input();

    // we need to do this inline without calling lpc43_gpio_read due to ISR being in SRAM not FLASH
    inline bool get() const
    {
        if (!this->valid) return false;
        //return this->inverting ^ lpc43_gpio_read(gpiocfg);
        uint8_t v = getreg8(LPC43_GPIO_B(((gpiocfg & GPIO_PORT_MASK) >> GPIO_PORT_SHIFT), ((gpiocfg & GPIO_PIN_MASK) >> GPIO_PIN_SHIFT))) & GPIO_B;
        return this->inverting ^ (v != 0);
    }

    // we need to do this inline without calling lpc43_gpio_write due to ISR being in SRAM not FLASH
    inline void set(bool value)
    {
        if (!this->valid) return;
        //lpc43_gpio_write(gpiocfg, this->inverting ^ value);
        uint8_t v= (this->inverting ^ value) ? 1 : 0;
        putreg8(v, LPC43_GPIO_B(((gpiocfg & GPIO_PORT_MASK) >> GPIO_PORT_SHIFT), ((gpiocfg & GPIO_PIN_MASK) >> GPIO_PIN_SHIFT)));
    }

    inline uint16_t get_gpiocfg() const { return gpiocfg; }

    bool is_inverting() const { return inverting; }
    void set_inverting(bool f) { inverting = f; }

    // mbed::InterruptIn *interrupt_pin();


private:
    static bool set_allocated(uint8_t, uint8_t, bool set= true);

    //Added pinName
    uint16_t gpiocfg;

    struct {
        bool inverting: 1;
        bool valid: 1;
        bool adc_only: 1;   //true if adc only pin
        int adc_channel: 8;   //adc channel
    };
};
