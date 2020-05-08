#include "KillButton.h"

#include "ConfigReader.h"
#include "SlowTicker.h"
#include "main.h"

#define kill_button_enable_key "enable"
#define kill_button_pin_key "pin"
#define toggle_key "toggle_enable"
#define unkill_key "unkill_enable"

REGISTER_MODULE(KillButton, KillButton::create)

bool KillButton::create(ConfigReader& cr)
{
    printf("DEBUG: configure kill button\n");
    KillButton *kill_button = new KillButton();
    if(!kill_button->configure(cr)) {
        printf("INFO: No kill button enabled\n");
        delete kill_button;
        kill_button = nullptr;
    }
    return true;
}

KillButton::KillButton() : Module("killbutton")
{
    this->state = IDLE;
    this->estop_still_pressed= false;
}

bool KillButton::configure(ConfigReader& cr)
{
    ConfigReader::section_map_t m;
    if(!cr.get_section("kill button", m)) return false;

    bool kill_enable = cr.get_bool(m,  kill_button_enable_key , false);
    if(!kill_enable) {
        return false;
    }

    this->unkill_enable = cr.get_bool(m,  unkill_key , true);
    this->toggle_enable = cr.get_bool(m,  toggle_key , false);

    this->kill_button.from_string( cr.get_string(m,  kill_button_pin_key , "nc"))->as_input();

    if(!this->kill_button.connected()) {
        return false;
    }

    SlowTicker::getInstance()->attach(5, std::bind(&KillButton::button_tick, this));

    return true;
}

// Check the state of the button and act accordingly using the following FSM
// Note this is the system timer so don't do anything slow in here
// If in toggle mode (locking estop) then button down will kill, and button up will unkill if unkill is enabled
// otherwise it will look for a 2 second press on the kill button to unkill if unkill is set
void KillButton::button_tick()
{
    bool killed = is_halted(); // in Module

    switch(state) {
        case IDLE:
            if(!this->kill_button.get()) state = KILL_BUTTON_DOWN;
            else if(unkill_enable && !toggle_enable && killed) state = KILLED_BUTTON_UP; // allow kill button to unkill if kill was created from some other source
            break;
        case KILL_BUTTON_DOWN:
            if(killed) state = KILLED_BUTTON_DOWN;
            break;
        case KILLED_BUTTON_DOWN:
            if (this->kill_button.get()) {
                state= KILLED_BUTTON_UP;
            } else if ((toggle_enable) && (!killed)) {
                // button is still pressed but the halted state was left
                // re-trigger the halted state
                state= KILL_BUTTON_DOWN;
                estop_still_pressed= true;
            }
            break;
        case KILLED_BUTTON_UP:
            if(!killed) state = IDLE;
            if(unkill_enable) {
                if(toggle_enable) state = UNKILL_FIRE; // if toggle is enabled and button is released then we unkill
                else if(!this->kill_button.get()) state = UNKILL_BUTTON_DOWN; // wait for button to be pressed to go into next state for timing start
            }
            break;
        case UNKILL_BUTTON_DOWN:
            unkill_timer = 0;
            state = UNKILL_TIMING_BUTTON_DOWN;
            break;
        case UNKILL_TIMING_BUTTON_DOWN:
            if(++unkill_timer > 5 * 2) state = UNKILL_FIRE;
            else if(this->kill_button.get()) unkill_timer = 0;
            if(!killed) state = IDLE;
            break;
        case UNKILL_FIRE:
            if(!killed) state = UNKILLED_BUTTON_DOWN;
            break;
        case UNKILLED_BUTTON_DOWN:
            if(this->kill_button.get()) state = IDLE;
            break;
    }

    if(state == KILL_BUTTON_DOWN) {
        if(!killed) {
            Module::broadcast_halt(true);
            print_to_all_consoles("ALARM: Kill button pressed - reset or M999 to continue\n");
            if(estop_still_pressed) {
                print_to_all_consoles("WARNING: ESTOP is still latched, unlatch ESTOP to clear HALT\n");
                estop_still_pressed= false;
            }else{
                print_to_all_consoles("ALARM: Kill button pressed - reset, $X or M999 to clear HALT\n");
            }
        }

    } else if(state == UNKILL_FIRE) {
        if(killed) {
            Module::broadcast_halt(false); // clears on_halt
            print_to_all_consoles("UnKill button pressed Halt cleared\n");
        }
    }
}
