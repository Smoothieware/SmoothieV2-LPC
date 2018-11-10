#pragma once

#include <array>

#ifndef MAX_ROBOT_ACTUATORS
    // includes 1 extruder or A
    #define MAX_ROBOT_ACTUATORS 4
#endif

#if MAX_ROBOT_ACTUATORS < 3 || MAX_ROBOT_ACTUATORS > 6
#error "MAX_ROBOT_ACTUATORS must be >= 3 and <= 6"
#endif

#ifndef N_PRIMARY_AXIS
    // This may change and include ABC
    #define N_PRIMARY_AXIS 3
#endif

#if N_PRIMARY_AXIS < 3 || N_PRIMARY_AXIS > MAX_ROBOT_ACTUATORS
#error "N_PRIMARY_AXIS must be >= 3 and <= MAX_ROBOT_ACTUATORS"
#endif

// Keep MAX_ROBOT_ACTUATORS as small as practical it impacts block size and therefore free memory.
const size_t k_max_actuators = MAX_ROBOT_ACTUATORS;
typedef struct std::array<float, k_max_actuators> ActuatorCoordinates;
