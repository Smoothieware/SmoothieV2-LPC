static const char *string_config= "\
[motion control]\n\
default_feed_rate = 4000 # Default speed (mm/minute) for G1/G2/G3 moves\n\
default_seek_rate = 4000 # Default speed (mm/minute) for G0 moves\n\
mm_per_arc_segment = 0.0 # Fixed length for line segments that divide arcs, 0 to disable\n\
mm_max_arc_error = 0.01 # The maximum error for line segments that divide arcs 0 to disable\n\
arc_correction = 5\n\
default_acceleration = 3000 # Acceleration in mm/second/second.\n\
junction_deviation = 0.05 # See http://smoothieware.org/motion-control#junction-deviation\n\
default_acceleration = 100.0 # default acceleration in mm/sec²\n\
arm_solution = cartesian\n\
\n\
[planner]\n\
junction_deviation = 0.05\n\
#z_junction_deviation = 0.0\n\
minimum_planner_speed = 0\n\
planner_queue_size = 32\n\
\n\
[actuator]\n\
alpha.steps_per_mm = 80 # Steps per mm for alpha ( X ) stepper\n\
alpha.step_pin = p2_9 # Pin for alpha stepper step signal\n\
alpha.dir_pin = P3_2 # Pin for alpha stepper direction, add '!' to reverse direction\n\
alpha.en_pin = nc # Pin for alpha enable pin\n\
alpha.max_rate = 30000.0 # Maximum rate in mm/min\n\
x.axis_max_speed = 30000 # Maximum speed in mm/min\n\
\n\
beta.steps_per_mm = 80 # Steps per mm for beta ( Y ) stepper\n\
beta.step_pin = p3_1 # Pin for beta stepper step signal\n\
beta.dir_pin = p2_12 # Pin for beta stepper direction, add '!' to reverse direction\n\
beta.en_pin = nc # Pin for beta enable\n\
beta.max_rate = 30000.0 # Maxmimum rate in mm/min\n\
y.axis_max_speed = 30000 # Maximum speed in mm/min\n\
\n\
gamma.steps_per_mm = 1600 # Steps per mm for gamma ( Z ) stepper\n\
gamma.step_pin = p2_13 # Pin for gamma stepper step signal\n\
gamma.dir_pin = p7_1 # Pin for gamma stepper direction, add '!' to reverse direction\n\
gamma.en_pin = nc # Pin for gamma enable\n\
gamma.max_rate = 300.0 # Maximum rate in mm/min\n\
z.axis_max_speed = 300 # Maximum speed in mm/min\n\
gamma.acceleration = 500  # overrides the default acceleration for this axis\n\
\n\
[switch]\n\
fan.enable = false # Enable this module\n\
fan.input_on_command = M106 # Command that will turn this switch on\n\
fan.input_off_command = M107 # Command that will turn this switch off\n\
fan.output_pin = p1.8 # Pin this module controls\n\
fan.output_type = pwm # PWM output settable with S parameter in the input_on_comand\n\
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
led1.output_pin        = gpio1_0\n\
led1.output_type       = digital\n\
\n\
led2.enable            = true\n\
led2.input_on_command  = M3\n\
led2.input_off_command = M4\n\
led2.output_pin        = gpio3_3\n\
led2.output_type       = sigmadeltapwm\n\
\n\
but1.enable             = true                     # Enable this module\n\
but1.input_pin          = gpio0_7!                 # button\n\
but1.output_on_command  = M1                       # command to send\n\
but1.output_off_command = M2                       # command to send\n\
but1.input_pin_behavior = toggle\n\
\n\
[extruder]\n\
hotend.enable = true             # Whether to activate the extruder module at all. All configuration is ignored if false\n\
hotend.tool_id = 0               # T0 will select\n\
hotend.steps_per_mm = 140        # Steps per mm for extruder stepper\n\
hotend.default_feed_rate = 600   # Default rate ( mm/minute ) for moves where only the extruder moves\n\
hotend.acceleration = 500        # Acceleration for the stepper motor mm/sec²\n\
hotend.max_speed = 50            # Maximum speed in mm/s\n\
hotend.step_pin = p5_3            # Pin for extruder step signal\n\
hotend.dir_pin = p9_6            # Pin for extruder dir signal ( add '!' to reverse direction )\n\
hotend.en_pin = p6.6             # Pin for extruder enable signal\n\
\n\
# Second extruder module configuration\n\
hotend2.enable = false            # Whether to activate the extruder module at all. All configuration is ignored if false\n\
hotend2.tool_id = 1               # T1 will select\n\
hotend2.steps_per_mm = 140       # Steps per mm for extruder stepper\n\
hotend2.default_feed_rate = 600  # Default rate ( mm/minute ) for moves where only the extruder moves\n\
hotend2.acceleration = 500       # Acceleration for the stepper motor, as of 0.6, arbitrary ratio\n\
hotend2.max_speed = 50           # mm/s\n\
\n\
hotend2.step_pin = 2.8           # Pin for extruder step signal\n\
hotend2.dir_pin = 2.13           # Pin for extruder dir signal ( add '!' to reverse direction )\n\
hotend2.en_pin = 4.29            # Pin for extruder enable signal\n\
\n\
hotend2.x_offset = 0             # x offset from origin in mm\n\
hotend2.y_offset = 25.0          # y offset from origin in mm\n\
hotend2.z_offset = 0             # z offset from origin in mm\n\
\n\
[temperature control]\n\
hotend.enable = true              # Whether to activate this ( 'hotend' ) module at all.\n\
hotend.tool_id = 0                # T0 will select\n\
hotend.heater_pin = P6.7          # Pin that controls the heater, set to nc if a readonly thermistor is being defined\n\
hotend.set_m_code = 104           # M-code to set the temperature for this module\n\
hotend.set_and_wait_m_code = 109  # M-code to set-and-wait for this module\n\
hotend.designator = T             # Designator letter for this module\n\
hotend.sensor = max31855          # Type of sensor\n\
hotend.dev_path_id = 2            # Device path ID number\n\
hotend.readings_per_second = 9    # Recommended 9 for max31855 and 4 for max6675\n\
\n\
hotend2.enable = false            # Whether to activate this ( 'hotend' ) module at all.\n\
hotend2.tool_id = 1               # T1 will select\n\
hotend2.heater_pin = P6.8         # Pin that controls the heater, set to nc if a readonly thermistor is being defined\n\
hotend2.set_m_code = 104          # M-code to set the temperature for this module\n\
hotend2.set_and_wait_m_code = 109 # M-code to set-and-wait for this module\n\
hotend2.designator = T            # Designator letter for this module\n\
hotend2.sensor = max31855         # Type of sensor\n\
hotend2.dev_path_id = 3           # Device path ID number\n\
hotend2.readings_per_second = 9   # Recommended 9 for max31855 and 4 for max6675\n\
\n\
bed.enable = true # Whether to activate this module at all.\n\
bed.tool_id = 250                 # beds do not have tool ids but we need to set a unique one anyway\n\
bed.thermistor_pin = P7.4 # Pin for the thermistor to read\n\
bed.heater_pin = P6.8 # Pin that controls the heater\n\
bed.thermistor = Honeywell100K # See http://smoothieware.org/temperaturecontrol#thermistor\n\
bed.set_m_code = 140 # M-code to set the temperature for this module\n\
bed.set_and_wait_m_code = 190 # M-code to set-and-wait for this module\n\
bed.designator = B # Designator letter for this module\n\
\n\
[kill button]\n\
enable = true          # Set to true to enable a kill button\n\
pin = p2.7             # Kill button pin.\n\
toggle_enable = false  # set to true to make it a toggle button (like an estop)\n\
unkill_enable = true   # enable kill button hold for 2 seconds does unkill\n\
\n\
[system leds]\n\
idle_led = P4_10    # flashes when running but idle\n\
\n";
