This document is a brief discussion on how to port a V1 smoothie module to v2.
There are many differences between V1 and V2 many come from using FreeRTOS as the base RTOS, and a discussion of that is beyond the scope of this note.

The other major difference is the use of a new config file format. V2 uses a .ini formatted file for the config.
All  modules have their own section header, and there is an extension that allows multiple instances of the same module
```
[MyModule]
instance1.enable = true
instance2.enable = true
etc...
```
See the tools/example-config.ini file

Checksums are no longer used and the #defines are renamed to xxx_key instead and are strings.
There is a helper script that converts many of the checksums into keys and calls to the new config library are converted too.
tools/convert-config.rb is the script and you need to install ruby and the gem rio to use it. You can also do it manually and use an existing ported module for examples.
```ruby convert-config.rb mymodule.cpp > my_converted-module.cpp```

An example of reading config entries is below:-
```
#define enable_key "enable"
#define pwm_pin_key "pwm_pin"
#define inverted_key "inverted_pwm"

bool Laser::configure(ConfigReader& cr)
{
    ConfigReader::section_map_t m;
    if(!cr.get_section("laser", m)) return false;

    if( !cr.get_bool(m,  enable_key , false) ) {
        // as not needed free up resource
        return false;
    }

    pwm_pin= new Pwm();
    pwm_pin->from_string(cr.get_string(m, pwm_pin_key, "nc"));

    if(!pwm_pin->is_valid()) {
        printf("Error: laser-config: Specified pin is not a valid PWM pin.\n");
        delete pwm_pin;
        pwm_pin= nullptr;
        return false;
    }

    this->pwm_inverting = cr.get_bool(m, inverted_key, false);
```

There are no longer any events to subscribe to, and modules are more flexible now. Any module can be looked up by name and a reference to that module returned if it exists. requests can be made direct to that module which replaces the PublicRequest system in V1.

```
    // lookup module named switch, with sub section name
    Module *m= Module::lookup("switch", name.c_str());
    if(m == nullptr) {
        // it does not exist
        os.printf("no such switch: %s\n", name.c_str());
        return true;
    }

    // get switch state by requesting direct to module
    bool state;
    ok= m->request("state", &state);
    if (!ok) {
        os.printf("unknown command %s.\n", "state");
        return true;
    }
    os.printf("switch %s is %d\n", name.c_str(), state);
```

Modules can self register themselves so adding them to main.cpp is no longer required...

```
    REGISTER_MODULE(CurrentControl, CurrentControl::create)
```

This tells the startup code to call the static method ```bool CurrentControl::create(ConfigReader& cr)``` where the module can configure itself. It should return false if there were any errors or it decided to not load itself.

Optionally if the module needs to be started up after everything has been initialised it can add itself to a startup callback list...
```
    // register a startup function
    register_startup(std::bind(&Network::start, network));
```
Here ```bool Network::start(void)``` is called once the boot phase is complete.


All gcodes and mcodes need to be registered with the Dispatcher, examples can be found in the many ported modules.
```
    // add a command handler
    THEDISPATCHER->add_handler( "fire", std::bind( &Laser::handle_fire_cmd, this, _1, _2) );

    // add handler for M221
    THEDISPATCHER->add_handler(Dispatcher::MCODE_HANDLER, 221, std::bind(&Laser::handle_M221, this, _1, _2));
```

The slowticker is limited to 100Hz, there is a fastticker for events faster than this.

```
    SlowTicker::getInstance()->attach(100, std::bind(&Laser::set_proportional_power, this));
```

NOTE that it is safe to do prints and SPI calls (and blocking routines that do not block for long) etc from a SlowTicker as it is an RTOS Timer, however FastTicker callbacks are ISRs and may not do prints or any slow or blocking calls.

Note all callbacks are of type std::function

As the CPU has a FPU never use doubles always use floats.
