#include "Thermistor.h"

#include "ConfigReader.h"
#include "OutputStream.h"
#include "StringUtils.h"
#include "Adc.h"

// a const list of predefined thermistors
#include "predefined_thermistors.h"

#include <math.h>
#include <limits>

#define UNDEFINED -1

#define thermistor_key     "thermistor"
#define r0_key             "r0"
#define t0_key             "t0"
#define beta_key           "beta"
#define vadc_key           "vadc"
#define vcc_key            "vcc"
#define r1_key             "r1"
#define r2_key             "r2"
#define thermistor_pin_key "thermistor_pin"
#define rt_curve_key       "rt_curve"
#define coefficients_key   "coefficients"
#define use_beta_table_key "use_beta_table"


Thermistor::Thermistor()
{
    this->use_steinhart_hart = false;
    this->beta = 0.0F; // not used by default
    min_temp = 999;
    max_temp = 0;
    this->thermistor_number = 0; // not a predefined thermistor
}

Thermistor::~Thermistor()
{
    delete thermistor_pin;
}

// Get configuration from the config file
bool Thermistor::configure(ConfigReader& cr, ConfigReader::section_map_t& m)
{
    // Values are here : http://reprap.org/wiki/Thermistor
    this->r0   = 100000;
    this->t0   = 25;
    this->r1   = 0;
    this->r2   = 4700;
    this->beta = 4066;

    // force use of beta perdefined thermistor table based on betas
    bool use_beta_table = cr.get_bool(m, use_beta_table_key, false);

    bool found = false;
    int cnt = 0;
    // load a predefined thermistor name if found
    std::string thermistor = cr.get_string(m, thermistor_key, "");
    if(!thermistor.empty()) {
        if(!use_beta_table) {
            for (auto& i : predefined_thermistors) {
                cnt++;
                if(thermistor.compare(i.name) == 0) {
                    this->c1 = i.c1;
                    this->c2 = i.c2;
                    this->c3 = i.c3;
                    this->r1 = i.r1;
                    this->r2 = i.r2;
                    use_steinhart_hart = true;
                    found = true;
                    break;
                }
            }
        }

        // fall back to the old beta pre-defined table if not found above
        if(!found) {
            cnt = 0;
            for (auto& i : predefined_thermistors_beta) {
                cnt++;
                if(thermistor.compare(i.name) == 0) {
                    this->beta = i.beta;
                    this->r0 = i.r0;
                    this->t0 = i.t0;
                    this->r1 = i.r1;
                    this->r2 = i.r2;
                    use_steinhart_hart = false;
                    cnt |= 0x80; // set MSB to indicate beta table
                    found = true;
                    break;
                }
            }
        }

        if(!found) {
            thermistor_number = 0;
        } else {
            thermistor_number = cnt;
        }
    }

    // Preset values are overriden by specified values
    if(!use_steinhart_hart) {
        this->beta = cr.get_float(m, beta_key, this->beta); // Thermistor beta rating. See http://reprap.org/bin/view/Main/MeasuringThermistorBeta
    }
    this->r0 = cr.get_float(m, r0_key, this->r0); // Stated resistance eg. 100K
    this->t0 = cr.get_float(m, t0_key, this->t0); // Temperature at stated resistance, eg. 25C
    this->r1 = cr.get_float(m, r1_key, this->r1);
    this->r2 = cr.get_float(m, r2_key, this->r2);

    // Thermistor pin for ADC readings
    thermistor_pin= new Adc(); // returns a sub instance of the Adc Singleton
    if(!thermistor_pin->is_created()) {
        delete thermistor_pin;
        printf("config-thermistor: Thermistor pin not created\n");
        return false;
    }

    // for the dedicated ADC pins use ADC0_n where n is channel to use 0-7
    // or use a valid pin specification for an ADC dual function pin eg P7.5 for ADC0_3
    if(this->thermistor_pin->from_string(cr.get_string(m, thermistor_pin_key, "nc")) == nullptr) {
        printf("config-thermistor: no thermistor pin defined, or invalid ADC pin or bad format\n");
        return false;
    }

    if(!thermistor_pin->connected()) {
        printf("config-thermistor: Thermistor pin not initialized\n");
        return false;
    }

    // specify the three Steinhart-Hart coefficients
    // specified as three comma separated floats, no spaces
    std::string coef = cr.get_string(m, coefficients_key, "");

    // speficy three temp,resistance pairs, best to use 25° 150° 240° and the coefficients will be calculated
    // specified as 25.0,100000.0,150.0,1355.0,240.0,203.0 which is temp in °C,resistance in ohms
    std::string rtc = cr.get_string(m, rt_curve_key, "");
    if(!rtc.empty()) {
        // use the http://en.wikipedia.org/wiki/Steinhart-Hart_equation instead of beta, as it is more accurate over the entire temp range
        // we use three temps/resistor values taken from the thermistor R-C curve found in most datasheets
        // eg http://sensing.honeywell.com/resistance-temperature-conversion-table-no-16, we take the resistance for 25°,150°,240° and the resistance in that table is 100000*R-T Curve coefficient
        // eg http://www.atcsemitec.co.uk/gt-2_thermistors.html for the semitec is even easier as they give the resistance in column 8 of the R/T table

        // then calculate the three Steinhart-Hart coefficients
        // format in config is T1,R1,T2,R2,T3,R3 if all three are not sepcified we revert to an invalid config state
        std::vector<float> trl = stringutils::parse_number_list(rtc.c_str());
        if(trl.size() != 6) {
            // punt we need 6 numbers, three pairs
            printf("config-thermistor: Error in config need 6 numbers for Steinhart-Hart\n");
            return false;
        }

        // calculate the coefficients
        std::tie(this->c1, this->c2, this->c3) = calculate_steinhart_hart_coefficients(trl[0], trl[1], trl[2], trl[3], trl[4], trl[5]);

        this->use_steinhart_hart = true;

    } else if(!coef.empty()) {
        // the three Steinhart-Hart coefficients
        // format in config is C1,C2,C3 if three are not specified we revert to an invalid config state
        std::vector<float> v = stringutils::parse_number_list(coef.c_str());
        if(v.size() != 3) {
            // punt we need 6 numbers, three pairs
            printf("config-thermistor: Error in config need 3 Steinhart-Hart coefficients\n");
            return false;
        }

        this->c1 = v[0];
        this->c2 = v[1];
        this->c3 = v[2];
        this->use_steinhart_hart = true;

    } else if(!use_steinhart_hart) {
        // if using beta
        if(!calc_jk()) return false;

    } else if(!found) {
        printf("config-thermistor: Error in config need rt_curve, coefficients, beta or a valid predefined thermistor defined\n");
        return false;
    }

    return true;
}

// print out predefined thermistors
void Thermistor::print_predefined_thermistors(OutputStream& os)
{
    int cnt = 1;
    os.printf("S/H table\n");
    for (auto& i : predefined_thermistors) {
        os.printf("%d - %s\n", cnt++, i.name);
    }

    cnt = 129;
    os.printf("Beta table\n");
    for (auto& i : predefined_thermistors_beta) {
        os.printf("%d - %s\n", cnt++, i.name);
    }
}

// calculate the coefficients from the supplied three Temp/Resistance pairs
// copied from https://github.com/MarlinFirmware/Marlin/blob/Development/Marlin/scripts/createTemperatureLookupMarlin.py
std::tuple<float, float, float> Thermistor::calculate_steinhart_hart_coefficients(float t1, float r1, float t2, float r2, float t3, float r3)
{
    float l1 = logf(r1);
    float l2 = logf(r2);
    float l3 = logf(r3);

    float y1 = 1.0F / (t1 + 273.15F);
    float y2 = 1.0F / (t2 + 273.15F);
    float y3 = 1.0F / (t3 + 273.15F);
    float x = (y2 - y1) / (l2 - l1);
    float y = (y3 - y1) / (l3 - l1);
    float c = (y - x) / ((l3 - l2) * (l1 + l2 + l3));
    float b = x - c * (powf(l1, 2) + powf(l2, 2) + l1 * l2);
    float a = y1 - (b + powf(l1, 2) * c) * l1;

    if(c < 0) {
        printf("WARNING: negative coefficient in calculate_steinhart_hart_coefficients. Something may be wrong with the measurements\n");
        c = -c;
    }
    return std::make_tuple(a, b, c);
}

bool Thermistor::calc_jk()
{
    // Thermistor math
    if(beta > 0.0F) {
        j = (1.0F / beta);
        k = (1.0F / (t0 + 273.15F));
    } else {
        printf("WARNING: beta cannot be 0\n");
        return false;
    }

    return true;
}

float Thermistor::get_temperature()
{
    float t = adc_value_to_temperature(new_thermistor_reading());
    if(!isinf(t)) {
        // keep track of min/max for M305
        if(t > max_temp) max_temp = t;
        if(t < min_temp) min_temp = t;
    }
    return t;
}

void Thermistor::get_raw(OutputStream& os)
{
    int adc_value = new_thermistor_reading();
    const uint32_t max_adc_value = Adc::get_max_value();

    // resistance of the thermistor in ohms
    float r = r2 / (((float)max_adc_value / adc_value) - 1.0F);
    if (r1 > 0.0F) r = (r1 * r) / (r1 - r);

    os.printf("adc= %d, resistance= %f, errors: %d\n", adc_value, r, thermistor_pin->get_errors());

    float t;
    if(this->use_steinhart_hart) {
        os.printf("S/H c1= %1.18f, c2= %1.18f, c3= %1.18f\n", c1, c2, c3);
        float l = logf(r);
        t = (1.0F / (this->c1 + this->c2 * l + this->c3 * powf(l, 3))) - 273.15F;
        os.printf("S/H temp= %f, min= %f, max= %f, delta= %f\n", t, min_temp, max_temp, max_temp - min_temp);
    } else {
        t = (1.0F / (k + (j * logf(r / r0)))) - 273.15F;
        os.printf("beta temp= %f, min= %f, max= %f, delta= %f\n", t, min_temp, max_temp, max_temp - min_temp);
    }

    // if using a predefined thermistor show its name and which table it is from
    if(thermistor_number != 0) {
        std::string name = (thermistor_number & 0x80) ? predefined_thermistors_beta[(thermistor_number & 0x7F) - 1].name :  predefined_thermistors[thermistor_number - 1].name;
        os.printf("Using predefined thermistor %d in %s table: %s\n", thermistor_number & 0x7F, (thermistor_number & 0x80) ? "Beta" : "S/H", name.c_str());
    }

    // reset the min/max
    min_temp = max_temp = t;
}

float Thermistor::adc_value_to_temperature(uint32_t adc_value)
{
    const uint32_t max_adc_value = Adc::get_max_value();
    if ((adc_value >= max_adc_value) || (adc_value == 0))
        return std::numeric_limits<float>::infinity();

    // resistance of the thermistor in ohms
    float r = r2 / (((float)max_adc_value / adc_value) - 1.0F);
    if (r1 > 0.0F) r = (r1 * r) / (r1 - r);

    if(r > this->r0 * 8) return std::numeric_limits<float>::infinity(); // 800k is probably open circuit

    float t;
    if(this->use_steinhart_hart) {
        float l = logf(r);
        t = (1.0F / (this->c1 + this->c2 * l + this->c3 * powf(l, 3))) - 273.15F;
    } else {
        // use Beta value
        t = (1.0F / (k + (j * logf(r / r0)))) - 273.15F;
    }

    return t;
}

int Thermistor::new_thermistor_reading()
{
    // filtering done in Adc
    return thermistor_pin->read();
}

bool Thermistor::set_optional(const sensor_options_t& options)
{
    bool define_beta = false;
    bool change_beta = false;
    uint8_t define_shh = 0;
    uint8_t predefined = 0;

    for(auto &i : options) {
        switch(i.first) {
            case 'B': this->beta = i.second; define_beta = true; break;
            case 'R': this->r0 = i.second; change_beta = true; break;
            case 'X': this->t0 = i.second; change_beta = true; break;
            case 'I': this->c1 = i.second; define_shh++; break;
            case 'J': this->c2 = i.second; define_shh++; break;
            case 'K': this->c3 = i.second; define_shh++; break;
            case 'P': predefined = roundf(i.second); break;
        }
    }

    if(predefined != 0) {
        if(define_beta || change_beta || define_shh != 0) {
            // cannot use a predefined with any other option
            return false;
        }

        if(predefined & 0x80) {
            // use the predefined beta table
            uint8_t n = (predefined & 0x7F) - 1;
            if(n >= sizeof(predefined_thermistors_beta) / sizeof(thermistor_beta_table_t)) {
                // not a valid index
                return false;
            }
            auto &i = predefined_thermistors_beta[n];
            this->beta = i.beta;
            this->r0 = i.r0;
            this->t0 = i.t0;
            this->r1 = i.r1;
            this->r2 = i.r2;
            use_steinhart_hart = false;
            if(!calc_jk()) return false;
            thermistor_number = predefined;
            return true;

        } else {
            // use the predefined S/H table
            uint8_t n = predefined - 1;
            if(n >= sizeof(predefined_thermistors) / sizeof(thermistor_table_t)) {
                // not a valid index
                return false;
            }
            auto &i = predefined_thermistors[n];
            this->c1 = i.c1;
            this->c2 = i.c2;
            this->c3 = i.c3;
            this->r1 = i.r1;
            this->r2 = i.r2;
            use_steinhart_hart = true;
            thermistor_number = predefined;
            return true;
        }
    }

    bool error = false;
    // if in Steinhart-Hart mode make sure B is specified, if in beta mode make sure all C1,C2,C3 are set and no beta settings
    // this is needed if swapping between modes
    if(use_steinhart_hart && define_shh == 0 && !define_beta) error = true; // if switching from SHH to beta need to specify new beta
    if(!use_steinhart_hart && define_shh > 0 && (define_beta || change_beta)) error = true; // if in beta mode and switching to SHH malke sure no beta settings are set
    if(!use_steinhart_hart && !(define_beta || change_beta) && define_shh != 3) error = true; // if in beta mode and switching to SHH must specify all three SHH
    if(use_steinhart_hart && define_shh > 0 && (define_beta || change_beta)) error = true; // if setting SHH anfd already in SHH do not specify any beta values

    if(error) {
        return false;
    }

    if(define_beta || change_beta) {
        if(!calc_jk()) return false;
        use_steinhart_hart = false;
    } else if(define_shh > 0) {
        use_steinhart_hart = true;
    } else {
        return false;
    }

    return true;
}

bool Thermistor::get_optional(sensor_options_t& options)
{
    if(thermistor_number != 0) {
        options['P'] = thermistor_number;
        return true;
    }

    if(use_steinhart_hart) {
        options['I'] = this->c1;
        options['J'] = this->c2;
        options['K'] = this->c3;

    } else {
        options['B'] = this->beta;
        options['X'] = this->t0;
        options['R'] = this->r0;
    }

    return true;
};
