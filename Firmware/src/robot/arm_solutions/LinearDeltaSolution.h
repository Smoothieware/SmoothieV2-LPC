#pragma once
#include "BaseSolution.h"

class LinearDeltaSolution : public BaseSolution {
    public:
        LinearDeltaSolution(ConfigReader&);
        void cartesian_to_actuator(const float[], ActuatorCoordinates &) const override;
        void actuator_to_cartesian(const ActuatorCoordinates &, float[] ) const override;

        bool set_optional(const arm_options_t& options) override;
        bool get_optional(arm_options_t& options, bool force_all) const override;

    private:
        void init();

        float arm_length{250.0F};
        float arm_radius{124.0F};
        float tower1_offset{0.0F};
        float tower2_offset{0.0F};
        float tower3_offset{0.0F};
        float tower1_angle{0.0F};
        float tower2_angle{0.0F};
        float tower3_angle{0.0F};

        float arm_length_squared{0.0F};

        float delta_tower1_x{0.0F};
        float delta_tower1_y{0.0F};
        float delta_tower2_x{0.0F};
        float delta_tower2_y{0.0F};
        float delta_tower3_x{0.0F};
        float delta_tower3_y{0.0F};

};
