#include "ConfigReader.h"
#include "StringUtils.h"

#include <cstring>
#include <cstdlib>

// match the line for a section header
bool ConfigReader::match_section(const char *line, std::string& section_name)
{
    if(strlen(line) < 3) return false;
    if(line[0] != '[') return false;

    const char *p = strchr(line, ']');
    if(p == nullptr) return false;

    section_name.assign(&line[1], p - line - 1);

    return true;
}

bool ConfigReader::extract_key_value(const char *line, std::string& key, std::string& value)
{
    if(strlen(line) < 3) return false;

    const char *p = strchr(line, '=');
    if(p == nullptr) return false;
    key.assign(line, p - line - 1);
    key = stringutils::trim(key);
    value.assign(p+1);
    value = stringutils::trim(value);
    return true;
}

bool ConfigReader::extract_sub_key_value(const char *line, std::string& key1, std::string& key2, std::string& value)
{
    if(strlen(line) < 5) return false;

    const char *p = strchr(line, '=');
    if(p == nullptr) return false;

    const char *p1 = strchr(line, '.');
    if(p1 == nullptr) return false; // no sub key
    if(p1 > p) return false; // make sure the a.b is before the =

    key1.assign(line, p1 - line);
    key1 = stringutils::trim(key1);

    key2.assign(p1 + 1, p - p1 - 1);
    key2 = stringutils::trim(key2);

    value.assign(p+1);

    value = stringutils::trim(value);
    return true;
}

// strips the commend from the line and returns the comment
std::string ConfigReader::strip_comments(std::string& s)
{
    auto n= s.find_first_of("#");
    if(n != std::string::npos) {
        std::string comment= s.substr(n);
        s= s.substr(0, n);
        return comment;
    }

    return "";
}

// just extract the key/values from the specified section
bool ConfigReader::get_section(const char *section, section_map_t& config)
{
    reset();
    current_section =  section;
    bool in_section = false;
    std::string s;
    while (std::getline(is, s)) {
        s = stringutils::trim(s);
        if(s.empty()) continue;

        // only check lines that are not blank and are not all comments
        if (s.size() > 0 && s[0] != '#') {
            strip_comments(s);

            std::string sec;

            if (match_section(s.c_str(), sec)) {
                // if this is the section we are looking for
                if(sec == section) {
                    in_section = true;

                } else if(in_section) {
                    // we are no longer in the section we want
                    break;
                }
            }

            if(in_section) {
                std::string key;
                std::string value;
                // extract all key/values from this section
                if(extract_key_value(s.c_str(), key, value)) {
                    // set this as a key value pair on the current name
                    config[key] = value;
                }

            }
        }
    }

    return !config.empty();
}

// just extract the key/values from the specified section and split them into sub sections
bool ConfigReader::get_sub_sections(const char *section, sub_section_map_t& config)
{
    reset();
    current_section =  section;
    bool in_section = false;
    std::string s;
    while (std::getline(is, s)) {
        s = stringutils::trim(s);
        if(s.empty()) continue;

        // only check lines that are not blank and are not all comments
        if (s.size() > 0 && s[0] != '#') {
            strip_comments(s);
            std::string sec;

            if (match_section(s.c_str(), sec)) {
                // if this is the section we are looking for
                if(sec == section) {
                    in_section = true;

                } else if(in_section) {
                    // we are no longer in the section we want
                    break;
                }
            }

            if(in_section) {
                std::string key1;
                std::string key2;
                std::string value;
                // extract all key/values from this section
                // and split them into subsections
                if(extract_sub_key_value(s.c_str(), key1, key2, value)) {
                    // set this as a key value pair on the current name
                    config[key1][key2] = value;
                }

            }
        }
    }
    return !config.empty();
}

// just extract the sections
bool ConfigReader::get_sections(sections_t& config)
{
    reset();
    current_section =  "";

    std::string s;
    while (std::getline(is, s)) {
        s = stringutils::trim(s);
        if(s.empty()) continue;

        // only check lines that are not blank and are not all comments
        if (s.size() > 0 && s[0] != '#') {
            strip_comments(s);
            std::string sec;

            if (match_section(s.c_str(), sec)) {
                config.insert(sec);
            }
        }
    }

    return !config.empty();
}

const char *ConfigReader::get_string(const section_map_t& m, const char *key, const char *def) const
{
    auto s = m.find(key);
    if(s != m.end()) {
        return s->second.c_str();
    }

    return def;
}

float ConfigReader::get_float(const section_map_t& m, const char *key, float def)
{
    auto s = m.find(key);
    if(s != m.end()) {
        // TODO should check it is a valid float
        return strtof(s->second.c_str(), nullptr);
    }

    return def;
}

bool ConfigReader::get_bool(const section_map_t& m, const char *key, bool def)
{
    auto s = m.find(key);
    if(s != m.end()) {
        return s->second == "true" || s->second == "t" || s->second == "1";
    }

    return def;
}

int ConfigReader::get_int(const section_map_t& m, const char *key, int def)
{
    auto s = m.find(key);
    if(s != m.end()) {
        // TODO should check it is a valid number
        return strtol (s->second.c_str(), nullptr, 0);
    }

    return def;
}

#if 0
#include "../TestUnits/prettyprint.hpp"
#include <iostream>
#include <fstream>

int main(int argc, char const *argv[])
{

    std::fstream fs;
    fs.open(argv[1], std::fstream::in);
    if(!fs.is_open()) {
        std::cout << "Error opening file: " << argv[1] << "\n";
        return 0;
    }

    ConfigReader cr(fs);
    if(argc == 2) {
        ConfigReader::sections_t sections;
        if(cr.get_sections(sections)) {
            std::cout << sections << "\n";
        }

        for(auto& i : sections) {
            cr.reset();
            std::cout << i << "...\n";
            ConfigReader::section_map_t config;
            if(cr.get_section(i.c_str(), config)) {
                std::cout << config << "\n";
            }
        }

    } else if(argc == 3) {
        ConfigReader::section_map_t config;
        if(cr.get_section(argv[2], config)) {
            std::cout << config << "\n";
        }

        // see if we have sub sections
        bool is_sub_section = false;
        for(auto& i : config) {
            if(i.first.find_first_of('.') != std::string::npos) {
                is_sub_section = true;
                break;
            }
        }

        if(is_sub_section) {
            cr.reset();
            std::cout << "\nSubsections...\n";
            ConfigReader::sub_section_map_t ssmap;
            // dump sub sections too
            if(cr.get_sub_sections(argv[2], ssmap)) {
                std::cout << ssmap << "\n";
            }

            for(auto& i : ssmap) {
                std::string ss = i.first;
                std::cout << ss << ":\n";
                for(auto& j : i.second) {
                    std::cout << "  " << j.first << ": " << j.second << "\n";
                }
            }
        }
    }

    fs.close();

    return 0;
}
#endif

#if 0
#include "../TestUnits/prettyprint.hpp"
#include <iostream>
#include <sstream>

int main(int argc, char const *argv[])
{

    std::string str("[switch]\nfan.enable = true\nfan.input_on_command = M106\nfan.input_off_command = M107\n\
fan.output_pin = 2.6\nfan.output_type = pwm\nmisc.enable = true\nmisc.input_on_command = M42\nmisc.input_off_command = M43\n\
misc.output_pin = 2.4\nmisc.output_type = digital\nmisc.value = 123.456\npsu.enable = false\n\
[dummy]\nenable = false");

    std::stringstream ss(str);
    ConfigReader cr(ss);
    ConfigReader::sub_section_map_t ssmap;
    if(!cr.get_sub_sections("switch", ssmap)) {
        std::cout << "no switch section found\n";
        exit(0);
    }

    for(auto& i : ssmap) {
        // foreach switch
        std::string name = i.first;
        auto& m = i.second;
        if(cr.get_bool(m, "enable", false)) {
            std::cout << "Found switch: " << name << "\n";
            //std::string input_on_command = cr.value("switch", name, "input_on_command").by_default("").as_string();
            std::string input_on_command = cr.get_string(m, "input_on_command", "");
            std::string input_off_command = cr.get_string(m, "input_off_command", "");
            std::string pin = cr.get_string(m, "output_pin", "nc");
            std::string type = cr.get_string(m, "output_type", "");
            float value = cr.get_float(m, "value", 0.0F);

            std::cout << "input_on_command: " << input_on_command << ", ";
            std::cout << "input_off_command: " << input_off_command << ", ";
            std::cout << "pin: " << pin << ", ";
            std::cout << "type: " << type << "\n";
            std::cout << "value: " << value << "\n";
        }

    }
}

#endif

#if 0
#include "unity.h"

#include <iostream>
#include <sstream>
#include "../TestUnits/prettyprint.hpp"

static std::string str("[switch]\nfan.enable = true\nfan.input_on_command = M106 # comment\nfan.input_off_command = M107\n\
fan.output_pin = 2.6 # pin to use\nfan.output_type = pwm\nmisc.enable = true\nmisc.input_on_command = M42\nmisc.input_off_command = M43\n\
misc.output_pin = 2.4\nmisc.output_type = digital\nmisc.value = 123.456\nmisc.ivalue= 123\npsu.enable = false\npsu#.x = bad\n\
[dummy]\nenable = false #set to true\ntest2 # = bad\n   #ignore comment\n #[bogus]\n[bogus2 #]\n");

static std::stringstream ss1(str);
static ConfigReader cr(ss1);

void ConfigTest_get_sections(void)
{
    ConfigReader::sections_t sections;
    TEST_ASSERT_TRUE(cr.get_sections(sections));
    std::cout << "sections:\n" << sections << "\n";
    TEST_ASSERT_TRUE(sections.find("switch") != sections.end());
    TEST_ASSERT_TRUE(sections.find("dummy") != sections.end());
    TEST_ASSERT_TRUE(sections.find("none") == sections.end());
    TEST_ASSERT_TRUE(sections.find("bogus") == sections.end());
    TEST_ASSERT_EQUAL_INT(2, sections.size());
}

void ConfigTest_load_section(void)
{
    ConfigReader::section_map_t m;
    bool b = cr.get_section("dummy", m);
    std::cout << m << "\n";

    TEST_ASSERT_TRUE(b);
    TEST_ASSERT_EQUAL_INT(1, m.size());
    TEST_ASSERT_TRUE(m.find("enable") != m.end());
    TEST_ASSERT_EQUAL_STRING("false", m["enable"].c_str());
    TEST_ASSERT_FALSE(cr.get_bool(m, "enable", true));
}

void ConfigTest_load_sub_sections(void)
{
    ConfigReader::sub_section_map_t ssmap;
    TEST_ASSERT_TRUE(ssmap.empty());

    TEST_ASSERT_TRUE(cr.get_sub_sections("switch", ssmap));

    std::cout << ssmap << "\n";

    TEST_ASSERT_EQUAL_STRING("switch", cr.get_current_section().c_str());
    TEST_ASSERT_EQUAL_INT(3, ssmap.size());

    TEST_ASSERT_TRUE(ssmap.find("fan") != ssmap.end());
    TEST_ASSERT_TRUE(ssmap.find("misc") != ssmap.end());
    TEST_ASSERT_TRUE(ssmap.find("psu") != ssmap.end());

    TEST_ASSERT_EQUAL_INT(1, ssmap["psu"].size());
    TEST_ASSERT_EQUAL_INT(5, ssmap["fan"].size());
    TEST_ASSERT_EQUAL_INT(7, ssmap["misc"].size());

    bool fanok = false;
    bool miscok = false;
    bool psuok = false;
    for(auto& i : ssmap) {
        // foreach switch
        std::string name = i.first;
        auto& m = i.second;
        if(cr.get_bool(m, "enable", false)) {
            const char* pin = cr.get_string(m, "output_pin", "nc");
            const char* input_on_command = cr.get_string(m, "input_on_command", "");
            const char* input_off_command = cr.get_string(m, "input_off_command", "");
            const char* output_on_command = cr.get_string(m, "output_on_command", "");
            const char* output_off_command = cr.get_string(m, "output_off_command", "");
            const char* type = cr.get_string(m, "output_type", "");
            const char* ipb = cr.get_string(m, "input_pin_behavior", "momentary");
            int iv = cr.get_int(m, "ivalue", 0);
            float fv = cr.get_float(m, "value", 0.0F);

            if(name == "fan") {
                TEST_ASSERT_EQUAL_STRING("2.6", pin);
                TEST_ASSERT_EQUAL_STRING("M106", input_on_command);
                TEST_ASSERT_EQUAL_STRING("M107", input_off_command);
                TEST_ASSERT_TRUE(output_on_command[0] == 0);
                TEST_ASSERT_TRUE(output_off_command[0] == 0);
                TEST_ASSERT_EQUAL_STRING(type, "pwm");
                TEST_ASSERT_EQUAL_STRING(ipb, "momentary");
                TEST_ASSERT_EQUAL_INT(0, iv);
                TEST_ASSERT_EQUAL_FLOAT(0.0F, fv);
                fanok = true;

            } else if(name == "misc") {
                TEST_ASSERT_EQUAL_STRING("2.4", pin);
                TEST_ASSERT_EQUAL_STRING("M42", input_on_command);
                TEST_ASSERT_EQUAL_STRING("M43", input_off_command);
                TEST_ASSERT_TRUE(output_on_command[0] == 0);
                TEST_ASSERT_TRUE(output_off_command[0] == 0);
                TEST_ASSERT_EQUAL_STRING("digital", type);
                TEST_ASSERT_EQUAL_STRING("momentary", ipb);
                TEST_ASSERT_EQUAL_INT(123, iv);
                TEST_ASSERT_EQUAL_FLOAT(123.456F, fv);
                miscok = true;

            } else if(name == "psu") {
                psuok = true;

            } else {
                TEST_FAIL();
            }
        }
    }
    TEST_ASSERT_TRUE(fanok);
    TEST_ASSERT_TRUE(miscok);
    TEST_ASSERT_FALSE(psuok);
}


int main()
{
    UNITY_BEGIN();
    RUN_TEST(ConfigTest_get_sections);
    RUN_TEST(ConfigTest_load_section);
    RUN_TEST(ConfigTest_load_sub_sections);
    return UNITY_END();
}
#endif
