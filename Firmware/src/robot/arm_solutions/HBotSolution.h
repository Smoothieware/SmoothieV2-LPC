#pragma once

#include "BaseSolution.h"

class HBotSolution : public BaseSolution {
    public:
        HBotSolution(ConfigReader&){};
        void cartesian_to_actuator(const float[], ActuatorCoordinates &) const override;
        void actuator_to_cartesian(const ActuatorCoordinates &, float[]) const override;
};
