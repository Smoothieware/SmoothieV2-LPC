static const char *string_config= "\
[general]\n\
grbl_mode = false\n\
\n\
[motion control]\n\
default_feed_rate = 4000 # Default speed (mm/minute) for G1/G2/G3 moves\n\
default_seek_rate = 4000 # Default speed (mm/minute) for G0 moves\n\
mm_per_arc_segment = 0.0 # Fixed length for line segments that divide arcs, 0 to disable\n\
mm_max_arc_error = 0.01 # The maximum error for line segments that divide arcs 0 to disable\n\
arc_correction = 5\n\
junction_deviation = 0.05 # See http://smoothieware.org/motion-control#junction-deviation\n\
default_acceleration = 1000.0 # default acceleration in mm/sec²\n\
arm_solution = cartesian\n\
x_axis_max_speed = 30000 # Maximum speed in mm/min\n\
y_axis_max_speed = 30000 # Maximum speed in mm/min\n\
z_axis_max_speed = 1800 # Maximum speed in mm/min\n\
\n\
[planner]\n\
junction_deviation = 0.05\n\
#z_junction_deviation = 0.0\n\
minimum_planner_speed = 0\n\
planner_queue_size = 32\n\
\n\
[actuator]\n\
common.motor_reset_pin = GPIO3_5 # reset pin for all drivers\n\
\n\
alpha.steps_per_mm = 400 # Steps per mm for alpha ( X ) stepper\n\
alpha.step_pin = gpio7_12 # Pin for alpha stepper step signal\n\
alpha.dir_pin = gpio7_9 # Pin for alpha stepper direction, add '!' to reverse direction\n\
alpha.en_pin = gpio3_10 # Pin for alpha enable pin\n\
alpha.ms1_pin = gpio1_13 # Pin for alpha micro stepping\n\
alpha.ms2_pin = gpio7_14 # Pin for alpha micro stepping\n\
#alpha.ms3_pin = gpio7_11 # Pin for alpha micro stepping\n\
alpha.microstepping = 1,1 # settings for alpha micro stepping pins ms1,ms2 default 1/16\n\
alpha.max_rate = 1800.0 # Maximum rate in mm/min\n\
\n\
beta.steps_per_mm = 400 # Steps per mm for beta ( Y ) stepper\n\
beta.step_pin = gpio3_6 # Pin for beta stepper step signal\n\
beta.dir_pin = gpio5_15 # Pin for beta stepper direction, add '!' to reverse direction\n\
beta.en_pin = gpio1_12 # Pin for beta enable\n\
beta.ms1_pin = gpio1_11 # Pin for beta micro stepping\n\
beta.ms2_pin = gpio7_7 # Pin for beta micro stepping\n\
#beta.ms3_pin = gpio2_8 # Pin for beta micro stepping\n\
beta.microstepping = 1,1 # settings for beta micro stepping pins ms1,ms2 default 1/16\n\
beta.max_rate = 1800.0 # Maxmimum rate in mm/min\n\
\n\
gamma.steps_per_mm = 400 # Steps per mm for gamma ( Z ) stepper\n\
gamma.step_pin = gpio0_5 # Pin for gamma stepper step signal\n\
gamma.dir_pin = gpio7_2 # Pin for gamma stepper direction, add '!' to reverse direction\n\
gamma.en_pin = gpio7_3 # Pin for gamma enable\n\
gamma.ms1_pin = gpio7_4 # Pin for gamma micro stepping\n\
gamma.ms2_pin = gpio5_6 # Pin for gamma micro stepping\n\
#gamma.ms3_pin = gpio3_1 # Pin for gamma micro stepping\n\
gamma.microstepping = 1,1 # settings for gamma micro stepping pins ms1,ms2 default 1/16\n\
gamma.max_rate = 1800 # Maximum rate in mm/min\n\
gamma.acceleration = 500  # overrides the default acceleration for this axis\n\
\n\
# Delta is first extruder, we set common stuff here instead of in extruder section\n\
delta.steps_per_mm = 140        # Steps per mm for extruder stepper\n\
delta.step_pin = gpio3_0        # Pin for extruder step signal\n\
delta.dir_pin = gpio3_3         # Pin for extruder dir signal ( add '!' to reverse direction )\n\
delta.en_pin = gpio7_6          # Pin for extruder enable signal\n\
delta.ms1_pin = gpio7_5         # Pin for delta micro stepping\n\
delta.ms2_pin = gpio3_2         # Pin for delta micro stepping\n\
#delta.ms3_pin = gpio3_4        # Pin for delta micro stepping\n\
delta.microstepping = 1,1       # settings for delta micro stepping pins 1/16\n\
delta.acceleration = 1800        # Acceleration for the stepper motor mm/sec²\n\
delta.max_rate = 3000.0           # Maximum rate in mm/min\n\
\n\
[current control]\n\
alpha.current  = 0.7    # X stepper motor current Amps\n\
alpha.pin      = P7.4   # PWM pin for alpha channel\n\
beta.current   = 0.7    # Y stepper motor current\n\
beta.pin       = PB.2   # PWM pin for beta channel\n\
gamma.current  = 0.7    # Z stepper motor current\n\
gamma.pin      = PB.3   # PWM pin for gamma channel\n\
delta.current  = 1.5    # First extruder stepper motor current\n\
delta.pin      = PB.1   # PWM pin for delta channel\n\
\n\
[switch]\n\
fan.enable = true # Enable this module\n\
fan.input_on_command = M106 # Command that will turn this switch on\n\
fan.input_off_command = M107 # Command that will turn this switch off\n\
fan.output_pin = PB.0 # Pin this module controls\n\
fan.output_type = sigmadeltapwm # PWM output settable with S parameter in the input_on_comand\n\
\n\
misc.enable = false             # Enable this module\n\
misc.input_on_command = M42              # Command that will turn this switch on\n\
misc.input_off_command = M43              # Command that will turn this switch off\n\
misc.output_pin = p6.1              # Pin this module controls\n\
misc.output_type = digital          # Digital means this is just an on or off pin\n\
\n\
led1.enable            = true\n\
led1.input_on_command  = M1\n\
led1.input_off_command = M2\n\
led1.output_pin        = gpio6_12\n\
led1.output_type       = digital\n\
\n\
#led2.enable            = true\n\
#led2.input_on_command  = M3\n\
#led2.input_off_command = M4\n\
#led2.output_pin        = gpio2_7\n\
#led2.output_type       = sigmadeltapwm\n\
\n\
#but1.enable             = true                     # Enable this module\n\
#but1.input_pin          = gpio0_7!                 # button\n\
#but1.output_on_command  = M1                       # command to send\n\
#but1.output_off_command = M2                       # command to send\n\
#but1.input_pin_behavior = toggle\n\
\n\
[extruder]\n\
hotend.enable = true             # Whether to activate the extruder module at all. All configuration is ignored if false\n\
hotend.tool_id = 0               # T0 will select\n\
# Second extruder module configuration\n\
hotend2.enable = false            # Whether to activate the extruder module at all. All configuration is ignored if false\n\
hotend2.tool_id = 1               # T1 will select\n\
\n\
hotend2.x_offset = 0             # x offset from origin in mm\n\
hotend2.y_offset = 25.0          # y offset from origin in mm\n\
hotend2.z_offset = 0             # z offset from origin in mm\n\
\n\
[temperature control]\n\
hotend.enable = true             # Whether to activate this ( 'hotend' ) module at all.\n\
hotend.tool_id = 0               # T0 will select\n\
hotend.thermistor_pin = ADC0_1     # Pin for the thermistor to read\n\
hotend.heater_pin = P7.1         # Pin that controls the heater, set to nc if a readonly thermistor is being defined\n\
hotend.thermistor = EPCOS100K    # See http://smoothieware.org/temperaturecontrol#toc5\n\
hotend.set_m_code = 104          # M-code to set the temperature for this module\n\
hotend.set_and_wait_m_code = 109 # M-code to set-and-wait for this module\n\
hotend.designator = T            # Designator letter for this module\n\
hotend.pwm_frequency = 1000      # FIXME slow for now when is SPIFI\n\
\n\
hotend2.enable = false            # Whether to activate this ( 'hotend' ) module at all.\n\
hotend2.tool_id = 1               # T1 will select\n\
hotend2.thermistor_pin = P7.4     # Pin for the thermistor to read\n\
hotend2.heater_pin = P6.8         # Pin that controls the heater, set to nc if a readonly thermistor is being defined\n\
hotend2.thermistor = EPCOS100K    # See http://smoothieware.org/temperaturecontrol#toc5\n\
hotend2.set_m_code = 104          # M-code to set the temperature for this module\n\
hotend2.set_and_wait_m_code = 109 # M-code to set-and-wait for this module\n\
hotend2.designator = T            # Designator letter for this module\n\
\n\
bed.enable = true # Whether to activate this module at all.\n\
bed.thermistor_pin = ADC0_2 # Pin for the thermistor to read\n\
bed.heater_pin = P7.5     # Pin that controls the heater\n\
bed.thermistor = Honeywell100K # See http://smoothieware.org/temperaturecontrol#thermistor\n\
bed.set_m_code = 140 # M-code to set the temperature for this module\n\
bed.set_and_wait_m_code = 190 # M-code to set-and-wait for this module\n\
bed.designator = B # Designator letter for this module\n\
bed.pwm_frequency = 1000      # FIXME slow for now when is SPIFI\n\
\n\
[kill button]\n\
enable = true          # Set to true to enable a kill button\n\
pin = p2.7             # Kill button pin.\n\
toggle_enable = false  # set to true to make it a toggle button (like an estop)\n\
unkill_enable = true   # enable kill button hold for 2 seconds does unkill\n\
\n\
[system leds]\n\
idle_led = gpio6_12    # flashes when running but idle\n\
play_led = gpio6_13    # on when playing, flashes when halted\n\
\n\
[pwm]\n\
frequency=10000        # PWM frequency\n\
[laser]\n\
enable = false # Whether to activate the laser module at all\n\
pwm_pin = P1.8 # This pin will be PWMed to control the laser.\n\
#inverted_pwm = false # set to true to invert the pwm\n\
#ttl_pin = P1.30  # This pin turns on when the laser turns on, and off when the laser turns off.\n\
#maximum_power = 1.0 # This is the maximum duty cycle that will be applied to the laser\n\
#minimum_power = 0.0 # This is a value just below the minimum duty cycle that keeps the laser active without actually burning.\n\
#default_power = 0.8 # This is the default laser power that will be used for cuts if a power has not been specified.  The value is a scale between the maximum and minimum power levels specified above\n\
\n\
[endstops]\n\
common.debounce_ms = 0         # debounce time in ms (actually 10ms min)\n\
#common.is_delta = true\n\
#common.homing_order = XYZ     # order in which axis homes (if defined)\n\
\n\
minx.enable = true             # enable an endstop\n\
minx.pin = gpio4_0            # pin\n\
minx.homing_direction = home_to_min      # direction it moves to the endstop\n\
minx.homing_position = 0                # the cartesian coordinate this is set to when it homes\n\
minx.axis = X                # the axis designator\n\
minx.max_travel = 500              # the maximum travel in mm before it times out\n\
minx.fast_rate = 30               # fast homing rate in mm/sec\n\
minx.slow_rate = 5               # slow homing rate in mm/sec\n\
minx.retract = 5                # bounce off endstop in mm\n\
minx.limit_enable = false        # enable hard limit\n\
\n\
miny.enable = true                  # enable an endstop\n\
miny.pin = gpio2_0                  # pin\n\
miny.homing_direction = home_to_min # direction it moves to the endstop\n\
miny.homing_position = 0            # the cartesian coordinate this is set to when it homes\n\
miny.axis = Y                       # the axis designator\n\
miny.max_travel = 500               # the maximum travel in mm before it times out\n\
miny.fast_rate = 30                 # fast homing rate in mm/sec\n\
miny.slow_rate = 5                  # slow homing rate in mm/sec\n\
miny.retract = 5                    # bounce off endstop in mm\n\
miny.limit_enable = false            # enable hard limits\n\
\n\
#minz.pin = gpio7_22\n\
#probe.pin = gpio7_21\n\
\n\
\n";
