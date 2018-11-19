#include "Extruder.h"

#include "Conveyor.h"
#include "Block.h"
#include "StepperMotor.h"
#include "ConfigReader.h"
#include "Robot.h"
#include "GCode.h"
#include "OutputStream.h"
#include "AxisDefns.h"
#include "Dispatcher.h"

#include <math.h>

#define filament_diameter_key           "filament_diameter"
#define x_offset_key                    "x_offset"
#define y_offset_key                    "y_offset"
#define z_offset_key                    "z_offset"
#define tool_id_key                     "tool_id"

#define retract_length_key              "retract_length"
#define retract_feedrate_key            "retract_feedrate"
#define retract_recover_length_key      "retract_recover_length"
#define retract_recover_feedrate_key    "retract_recover_feedrate"
#define retract_zlift_length_key        "retract_zlift_length"
#define retract_zlift_feedrate_key      "retract_zlift_feedrate"

#define PI 3.14159265358979F


/*
    As the actual motion is handled by the planner and the stepticker, this module just handles Extruder specific gcodes
    and settings.
    In a multi extruder setting it must be selected to be addressed. (using T0 T1 etc)
*/

Extruder::Extruder(const char *name) : Module("extruder", name)
{
    this->selected = false;
    this->retracted = false;
    this->volumetric_multiplier = 1.0F;
    this->extruder_multiplier = 1.0F;
    this->stepper_motor = nullptr;
    this->max_volumetric_rate = 0;
    this->g92e0_detected = false;
}

Extruder::~Extruder()
{
    delete stepper_motor;
}

bool Extruder::configure(ConfigReader& cr)
{
    ConfigReader::sub_section_map_t ssmap;
    if(!cr.get_sub_sections("extruder", ssmap)) {
        printf("configure-extruder: no extuder section found\n");
        return false;
    }

    int cnt = 0;
    for(auto& i : ssmap) {
        // foreach extruder
        std::string name = i.first;
        auto& m = i.second;
        if(cr.get_bool(m, "enable", false)) {
            Extruder *ex = new Extruder(name.c_str());
            if(ex->configure(cr, m)) {
                // make sure the first (or only) extruder is selected
                if(cnt == 0) ex->select();
                ++cnt;
            } else {
                delete ex;
            }
        }
    }

    if(cnt == 0) return false;

    if(cnt > 1) {
        printf("configure-extruder: NOTE: %d extruders configured and enabled\n", cnt);

    } else {
        // only one extruder so select it
        printf("configure-extruder: NOTE: One extruder configured and enabled\n");
    }

    return true;
}

// Get config
bool Extruder::configure(ConfigReader& cr, ConfigReader::section_map_t& m)
{
    // pins and speeds and acceleration are set in Robot for delta, epsilon etc

    // multi extruder setup
    this->tool_id             = cr.get_int(m, tool_id_key, 0); // set to T0 by default, must be set to > 0 for subsequent extruders
    this->tool_offset[X_AXIS] = cr.get_float(m, x_offset_key, 0);
    this->tool_offset[Y_AXIS] = cr.get_float(m, y_offset_key, 0);
    this->tool_offset[Z_AXIS] = cr.get_float(m, z_offset_key, 0);

    // settings
    this->filament_diameter        = cr.get_float(m, filament_diameter_key , 0);
    this->retract_length           = cr.get_float(m, retract_length_key, 3);
    this->retract_feedrate         = cr.get_float(m, retract_feedrate_key, 45);
    this->retract_recover_length   = cr.get_float(m, retract_recover_length_key, 0);
    this->retract_recover_feedrate = cr.get_float(m, retract_recover_feedrate_key, 8);
    this->retract_zlift_length     = cr.get_float(m, retract_zlift_length_key, 0);
    this->retract_zlift_feedrate   = cr.get_float(m, retract_zlift_feedrate_key, 100 * 60) / 60.0F; // mm/min

    if(filament_diameter > 0.01F) {
        this->volumetric_multiplier = 1.0F / (powf(this->filament_diameter / 2, 2) * PI);
    }

    // Stepper motor object for the extruder, get from robot it will be actuator index 3+tool_id
    if(Robot::getInstance()->actuators.size() <= (size_t)(A_AXIS+tool_id)) {
        printf("ERROR: Extruder motor has not been defined in Robot (delta and/or epsilon)\n");
        return false;
    }

    // we keep a copy for convenience
    stepper_motor = Robot::getInstance()->actuators[A_AXIS+tool_id];
    motor_id = stepper_motor->get_motor_id();
    if(motor_id < A_AXIS || motor_id == 255) {
        // error registering, maybe too many
        return false;
    }

    stepper_motor->set_selected(false); // not selected by default
    stepper_motor->set_extruder(true);  // indicates it is an extruder

    // register gcodes and mcodes
    using std::placeholders::_1;
    using std::placeholders::_2;

    Dispatcher::getInstance()->add_handler(Dispatcher::MCODE_HANDLER,   6, std::bind(&Extruder::handle_M6,    this, _1, _2));
    Dispatcher::getInstance()->add_handler(Dispatcher::MCODE_HANDLER,  92, std::bind(&Extruder::handle_mcode, this, _1, _2));
    Dispatcher::getInstance()->add_handler(Dispatcher::MCODE_HANDLER, 114, std::bind(&Extruder::handle_mcode, this, _1, _2));
    Dispatcher::getInstance()->add_handler(Dispatcher::MCODE_HANDLER, 200, std::bind(&Extruder::handle_mcode, this, _1, _2));
    Dispatcher::getInstance()->add_handler(Dispatcher::MCODE_HANDLER, 203, std::bind(&Extruder::handle_mcode, this, _1, _2));
    Dispatcher::getInstance()->add_handler(Dispatcher::MCODE_HANDLER, 204, std::bind(&Extruder::handle_mcode, this, _1, _2));
    Dispatcher::getInstance()->add_handler(Dispatcher::MCODE_HANDLER, 207, std::bind(&Extruder::handle_mcode, this, _1, _2));
    Dispatcher::getInstance()->add_handler(Dispatcher::MCODE_HANDLER, 208, std::bind(&Extruder::handle_mcode, this, _1, _2));
    Dispatcher::getInstance()->add_handler(Dispatcher::MCODE_HANDLER, 221, std::bind(&Extruder::handle_mcode, this, _1, _2));
    Dispatcher::getInstance()->add_handler(Dispatcher::MCODE_HANDLER, 500, std::bind(&Extruder::handle_mcode, this, _1, _2));

    Dispatcher::getInstance()->add_handler(Dispatcher::GCODE_HANDLER,   0, std::bind(&Extruder::handle_gcode, this, _1, _2));
    Dispatcher::getInstance()->add_handler(Dispatcher::GCODE_HANDLER,   1, std::bind(&Extruder::handle_gcode, this, _1, _2));
    Dispatcher::getInstance()->add_handler(Dispatcher::GCODE_HANDLER,  10, std::bind(&Extruder::handle_gcode, this, _1, _2));
    Dispatcher::getInstance()->add_handler(Dispatcher::GCODE_HANDLER,  11, std::bind(&Extruder::handle_gcode, this, _1, _2));
    Dispatcher::getInstance()->add_handler(Dispatcher::GCODE_HANDLER,  92, std::bind(&Extruder::handle_gcode, this, _1, _2));


    return true;
}

void Extruder::select()
{
    selected = true;
    stepper_motor->set_selected(true);
    // set the function pointer to return the current scaling
    Robot::getInstance()->get_e_scale_fnc = std::bind(&Extruder::get_e_scale, this);
}

void Extruder::deselect()
{
    selected = false;
    stepper_motor->set_selected(false);
}

// handle tool change
bool Extruder::handle_M6(GCode& gcode, OutputStream& os)
{
    // this replaces what toolmanager used to do
    if(gcode.has_t()) {
        if(gcode.get_int_arg('T') == this->tool_id) {
            // we have been selected
            if(!selected) {
                // We must wait for an empty queue before we can disable the current extruder
                Conveyor::getInstance()->wait_for_idle();
                this->select();

                // send tool offset to robot
                Robot::getInstance()->setToolOffset(this->tool_offset);
            }

        } else {
            if(selected) {
                // we have not been selected so deselect ourselves
                this->deselect();
            }
        }

    } else {
        // this is an error as there was no T parameter to tell us what tool was selected
        return false;
    }

    return true;
}

bool Extruder::request(const char *key, void *value)
{
    if(strcmp(key, "save_state") == 0) {
        save_position();
        this->saved_selected = this->selected;
        return true;
    }

    if(strcmp(key, "restore_state") == 0) {
        restore_position();
        this->selected = this->saved_selected;
        return true;
    }

    if(strcmp(key, "get_state") == 0 && this->selected) {
        // pointer to structure to return data to is provided
        pad_extruder_t *e = static_cast<pad_extruder_t *>(value);
        e->steps_per_mm = stepper_motor->get_steps_per_mm();
        e->filament_diameter = this->filament_diameter;
        e->flow_rate = this->extruder_multiplier;
        e->accleration = stepper_motor->get_acceleration();
        e->retract_length = this->retract_length;
        e->current_position = stepper_motor->get_current_position();
        return true;
    }

    // handle extrude rates request from robot
    if(strcmp(key, "get_rate") == 0 && this->selected) {
        // disabled extruders do not reply NOTE only one enabled extruder supported
        float *d = static_cast<float *>(value);
        float delta = d[0]; // the E passed in on Gcode is the delta volume in mm³
        float isecs = d[1]; // inverted secs

        // check against maximum speeds and return rate modifier
        d[1] = check_max_speeds(delta, isecs);
        return true;
    }

    return false;
}

void Extruder::save_position()
{
    // we need to save these separately as they may have been scaled
    this->saved_position = std::make_tuple(Robot::getInstance()->get_axis_position(motor_id), stepper_motor->get_last_milestone(), stepper_motor->get_last_milestone_steps());
}

void Extruder::restore_position()
{
    Robot::getInstance()->reset_axis_position(std::get<0>(this->saved_position), motor_id);
    stepper_motor->set_last_milestones(std::get<1>(this->saved_position), std::get<2>(this->saved_position));
}

// check against maximum speeds and return the rate modifier
float Extruder::check_max_speeds(float delta, float isecs)
{
    float rm = 1.0F; // default no rate modification

    if(this->max_volumetric_rate > 0 && this->filament_diameter > 0.01F) {
        // volumetric enabled and check for volumetric rate
        float v = delta * isecs; // the flow rate in mm³/sec

        // return the rate change needed to stay within the max rate
        if(v > max_volumetric_rate) {
            rm = max_volumetric_rate / v;
        }
        //printf("requested flow rate: %f mm³/sec, corrected flow rate: %f  mm³/sec\n", v, v * rm);
    }

    return rm;
}

bool Extruder::handle_mcode(GCode& gcode, OutputStream& os)
{
    // M codes most execute immediately, most only execute if enabled
    if (gcode.get_code() == 114 && this->selected) {
        char buf[16];
        if(gcode.get_subcode() == 0) {
            float pos = Robot::getInstance()->get_axis_position(motor_id);
            snprintf(buf, sizeof(buf), " E:%1.4f ", pos);
            os.set_prepend_ok();
            os.set_append_nl();
            os.puts(buf);

        } else if(gcode.get_subcode() == 1) { // realtime position
            snprintf(buf, sizeof(buf), " E:%1.4f ", stepper_motor->get_current_position() / get_e_scale());
            os.set_prepend_ok();
            os.set_append_nl();
            os.puts(buf);

        } else if(gcode.get_subcode() == 3) { // realtime actuator position
            snprintf(buf, sizeof(buf), " E:%1.4f ", stepper_motor->get_current_position());
            os.set_prepend_ok();
            os.set_append_nl();
            os.puts(buf);
        }

        return true;

    } else if (gcode.get_code() == 92 && ( (this->selected && !gcode.has_arg('P')) || (gcode.has_arg('P') && gcode.get_int_arg('P') == this->tool_id) ) ) {
        float spm = stepper_motor->get_steps_per_mm();
        if (gcode.has_arg('E')) {
            spm = gcode.get_arg('E');
            stepper_motor->change_steps_per_mm(spm);
        }

        os.set_append_nl();
        os.printf("E:%f ", spm);
        return true;

    } else if (gcode.get_code() == 200 && ( (this->selected && !gcode.has_arg('P')) || (gcode.has_arg('P') && gcode.get_int_arg('P') == this->tool_id)) ) {
        if (gcode.has_arg('D')) {
            this->filament_diameter = gcode.get_arg('D');
            float last_scale = this->volumetric_multiplier;
            if(filament_diameter > 0.01F) {
                this->volumetric_multiplier = 1.0F / (powf(this->filament_diameter / 2, 2) * PI);
            } else {
                this->volumetric_multiplier = 1.0F;
            }
            // the trouble here is that the last milestone will be for the previous multiplier so a change can cause a big blob
            // so we must change the E last milestone accordingly so it continues smoothly....
            // change E last milestone to what it would have been if it had used this new multiplier
            float delta = this->volumetric_multiplier / last_scale;
            float nm = this->stepper_motor->get_last_milestone() * delta;
            this->stepper_motor->change_last_milestone(nm);

        } else {
            if(filament_diameter > 0.01F) {
                os.printf("Filament Diameter: %f\n", this->filament_diameter);
            } else {
                os.printf("Volumetric extrusion is disabled\n");
            }
        }

        return true;

    } else if (gcode.get_code() == 203 && ( (this->selected && !gcode.has_arg('P')) || (gcode.has_arg('P') && gcode.get_int_arg('P') == this->tool_id)) ) {
        // M203 Exxx Vyyy Set maximum feedrates xxx mm/sec and/or yyy mm³/sec
        if(gcode.get_num_args() == 0) {
            os.set_append_nl();
            os.printf("E:%g V:%g", this->stepper_motor->get_max_rate(), this->max_volumetric_rate);

        } else {
            if(gcode.has_arg('E')) {
                this->stepper_motor->set_max_rate(gcode.get_arg('E'));
            }
            if(gcode.has_arg('V')) {
                this->max_volumetric_rate = gcode.get_arg('V');
            }
        }

        return true;

    } else if (gcode.get_code() == 204 && gcode.has_arg('E') &&
               ( (this->selected && !gcode.has_arg('P')) || (gcode.has_arg('P') && gcode.get_int_arg('P') == this->tool_id)) ) {
        // extruder acceleration M204 Ennn mm/sec^2 (Pnnn sets the specific extruder for M500)
        stepper_motor->set_acceleration(gcode.get_arg('E'));
        return true;

    } else if (gcode.get_code() == 207 && ( (this->selected && !gcode.has_arg('P')) || (gcode.has_arg('P') && gcode.get_int_arg('P') == this->tool_id)) ) {
        // M207 - set retract length S[positive mm] F[feedrate mm/min] Z[additional zlift/hop] Q[zlift feedrate mm/min]
        if(gcode.has_arg('S')) retract_length = gcode.get_arg('S');
        if(gcode.has_arg('F')) retract_feedrate = gcode.get_arg('F') / 60.0F; // specified in mm/min converted to mm/sec
        if(gcode.has_arg('Z')) retract_zlift_length = gcode.get_arg('Z'); // specified in mm
        if(gcode.has_arg('Q')) retract_zlift_feedrate = gcode.get_arg('Q') / 60.0F; // specified in mm/min converted to mm/sec
        return true;

    } else if (gcode.get_code() == 208 && ( (this->selected && !gcode.has_arg('P')) || (gcode.has_arg('P') && gcode.get_int_arg('P') == this->tool_id)) ) {
        // M208 - set retract recover length S[positive mm surplus to the M207 S*] F[feedrate mm/min]
        if(gcode.has_arg('S')) retract_recover_length = gcode.get_arg('S');
        if(gcode.has_arg('F')) retract_recover_feedrate = gcode.get_arg('F') / 60.0F; // specified in mm/min converted to mm/sec
        return true;

    } else if (gcode.get_code() == 221 && this->selected) { // M221 S100 change flow rate by percentage
        if(gcode.has_arg('S')) {
            float last_scale = this->extruder_multiplier;
            this->extruder_multiplier = gcode.get_arg('S') / 100.0F;
            // the trouble here is that the last milestone will be for the previous multiplier so a change can cause a big blob
            // so we must change the E last milestone accordingly so it continues smoothly....
            // change E last milestone to what it would have been if it had used this new multiplier
            float delta = this->extruder_multiplier / last_scale;
            float nm = this->stepper_motor->get_last_milestone() * delta;
            this->stepper_motor->change_last_milestone(nm);

        } else {
            os.printf("Flow rate at %6.2f %%\n", this->extruder_multiplier * 100.0F);
        }
        return true;

    } else if (gcode.get_code() == 500) { // M500 saves some volatile settings to config override file, M500.3 just prints the settings
        os.printf(";E Steps per mm:\nM92 E%1.4f P%d\n", stepper_motor->get_steps_per_mm(), this->tool_id);
        os.printf(";E Filament diameter:\nM200 D%1.4f P%d\n", this->filament_diameter, this->tool_id);
        os.printf(";E retract length, feedrate:\nM207 S%1.4f F%1.4f Z%1.4f Q%1.4f P%d\n", this->retract_length, this->retract_feedrate * 60.0F, this->retract_zlift_length, this->retract_zlift_feedrate * 60.0F, this->tool_id);
        os.printf(";E retract recover length, feedrate:\nM208 S%1.4f F%1.4f P%d\n", this->retract_recover_length, this->retract_recover_feedrate * 60.0F, this->tool_id);
        os.printf(";E acceleration mm/sec/sec:\nM204 E%1.4f P%d\n", stepper_motor->get_acceleration(), this->tool_id);
        os.printf(";E max feed rate mm/sec:\nM203 E%1.4f P%d\n", stepper_motor->get_max_rate(), this->tool_id);
        if(this->max_volumetric_rate > 0) {
            os.printf(";E max volumetric rate mm^3/sec:\nM203 V%1.4f P%d\n", this->max_volumetric_rate, this->tool_id);
        }
        return true;
    }

    // if we get here one of the above did not handle the gcode
    return false;
}

bool Extruder::handle_gcode(GCode & gcode, OutputStream & os)
{
    if(!this->selected) return false;

    if( (gcode.get_code() == 10 || gcode.get_code() == 11) && !gcode.has_arg('L') ) {
        // firmware retract command (Ignore if has L parameter that is not for us)
        // check we are in the correct state of retract or unretract
        if(gcode.get_code() == 10 && !retracted) {
            this->retracted = true;
            this->cancel_zlift_restore = false;
            this->g92e0_detected = false;
        } else if(gcode.get_code() == 11 && retracted) {
            this->retracted = false;
        } else
            return true; // ignore duplicates

        if(gcode.get_code() == 10) {
            // retract
            float delta[motor_id + 1];
            for (int i = 0; i < motor_id; ++i) {
                delta[i] = 0;
            }

            delta[motor_id] = -retract_length / get_e_scale(); // convert from mm to mm³, and unapply flow_rate
            Robot::getInstance()->delta_move(delta, retract_feedrate, motor_id + 1);

            // zlift
            if(retract_zlift_length > 0) {
                float d[3] {0, 0, retract_zlift_length};
                Robot::getInstance()->delta_move(d, retract_zlift_feedrate, 3);
            }

        } else if(gcode.get_code() == 11) {
            // unretract
            if(retract_zlift_length > 0 && !this->cancel_zlift_restore) {
                // reverse zlift happens before unretract
                // NOTE we do not do this if cancel_zlift_restore is set to true, which happens if there is an absolute Z move inbetween G10 and G11
                float delta[3] {0, 0, -retract_zlift_length};
                Robot::getInstance()->delta_move(delta, retract_zlift_feedrate, 3);
            }

            float delta[motor_id + 1];
            for (int i = 0; i < motor_id; ++i) {
                delta[i] = 0;
            }
            // HACK ALERT due to certain slicers reseting E with G92 E0 between the G10 and G11 we need to restore
            // the current position after we do the unretract, this is horribly hacky :(
            // also as the move has not completed yet, when we restore the current position will be incorrect once the move finishes,
            // however this is not fatal for an extruder
            if(g92e0_detected) save_position();
            delta[motor_id] = (retract_length + retract_recover_length) / get_e_scale(); // convert from mm to mm³, and unapply flow_rate
            Robot::getInstance()->delta_move(delta, retract_recover_feedrate, motor_id + 1);
            if(g92e0_detected) restore_position();
        }

        return true;

    } else if( this->retracted && (gcode.get_code() == 0 || gcode.get_code() == 1) && gcode.has_arg('Z')) {
        // NOTE we cancel the zlift restore for the following G11 as we have moved to an absolute Z which we need to stay at
        this->cancel_zlift_restore = true;
        return true;

    } else if( this->retracted && gcode.get_code() == 92 && gcode.has_arg('E')) {
        // old versions of slic3rs issued a G92 E0 after G10, handle that case
        this->g92e0_detected = true;
        return true;
    }


    // should return false if it reaches here
    return false;
}
