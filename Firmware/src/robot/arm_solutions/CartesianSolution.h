#pragma once

#include "BaseSolution.h"

class CartesianSolution : public BaseSolution {
    public:
        CartesianSolution(ConfigReader&){};
        void cartesian_to_actuator( const float millimeters[], ActuatorCoordinates &steps ) const override;
        void actuator_to_cartesian( const ActuatorCoordinates &steps, float millimeters[] ) const override;
};
