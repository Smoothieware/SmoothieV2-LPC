#pragma once

#include "BaseSolution.h"

class CoreXZSolution : public BaseSolution {
    public:
        CoreXZSolution(ConfigReader&);
        void cartesian_to_actuator(const float[], ActuatorCoordinates & ) const override;
        void actuator_to_cartesian(const ActuatorCoordinates &, float[] ) const override;

    private:
        float x_reduction{1.0F};
        float z_reduction{3.0F};
};
