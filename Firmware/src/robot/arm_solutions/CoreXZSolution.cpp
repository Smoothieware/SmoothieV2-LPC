#include "CoreXZSolution.h"
#include "ConfigReader.h"
#include "AxisDefns.h"

#define x_reduction_key         "x_reduction"
#define z_reduction_key         "z_reduction"

CoreXZSolution::CoreXZSolution(ConfigReader& cr)
{
    ConfigReader::section_map_t m;
    if(cr.get_section("corexz", m)) {
        x_reduction = cr.get_float(m, x_reduction_key, 1.0f);
        z_reduction = cr.get_float(m, z_reduction_key, 3.0f);
    }
}

void CoreXZSolution::cartesian_to_actuator(const float cartesian_mm[], ActuatorCoordinates &actuator_mm ) const {
    actuator_mm[ALPHA_STEPPER] = (this->x_reduction * cartesian_mm[X_AXIS]) + (this->z_reduction * cartesian_mm[Z_AXIS]);
    actuator_mm[BETA_STEPPER ] = (this->x_reduction * cartesian_mm[X_AXIS]) - (this->z_reduction * cartesian_mm[Z_AXIS]);
    actuator_mm[GAMMA_STEPPER] = cartesian_mm[Y_AXIS];
}

void CoreXZSolution::actuator_to_cartesian(const ActuatorCoordinates &actuator_mm, float cartesian_mm[] ) const {
    cartesian_mm[X_AXIS] = (0.5F/this->x_reduction) * (actuator_mm[ALPHA_STEPPER] + actuator_mm[BETA_STEPPER]);
    cartesian_mm[Z_AXIS] = (0.5F/this->z_reduction) * (actuator_mm[ALPHA_STEPPER] - actuator_mm[BETA_STEPPER]);
    cartesian_mm[Y_AXIS] = actuator_mm[GAMMA_STEPPER];
}
