#include "../Unity/src/unity.h"
#include <sstream>

#include "ConfigReader.h"
#include "ConfigWriter.h"
#include "TestRegistry.h"

#include "prettyprint.hpp"
#include <iostream>

static std::string str("[switch]\nfan.enable = true\nfan.input_on_command = M106 # comment\nfan.input_off_command = M107\n\
fan.output_pin = 2.6 # pin to use\nfan.output_type = pwm\nmisc.enable = true\nmisc.input_on_command = M42\nmisc.input_off_command = M43\n\
misc.output_pin = 2.4\nmisc.output_type = digital\nmisc.value = 123.456\nmisc.ivalue= 123\npsu.enable = false\npsu#.x = bad\n\
[dummy]\nenable = false #set to true\ntest2 # = bad\n   #ignore comment\n #[bogus]\n[bogus2 #]\n");

REGISTER_TEST(ConfigTest, get_sections)
{
    std::stringstream ss1(str);
    ConfigReader cr(ss1);

    ConfigReader::sections_t sections;
    //systime_t st = clock_systimer();
    TEST_ASSERT_TRUE(cr.get_sections(sections));
    //systime_t en = clock_systimer();
    //printf("elapsed time %d us\n", TICK2USEC(en-st));

    TEST_ASSERT_TRUE(sections.find("switch") != sections.end());
    TEST_ASSERT_TRUE(sections.find("dummy") != sections.end());
    TEST_ASSERT_TRUE(sections.find("none") == sections.end());
}

REGISTER_TEST(ConfigTest, load_section)
{
    std::stringstream ss1(str);
    ConfigReader cr(ss1);
    //cr.reset();

    ConfigReader::section_map_t m;
    //systime_t st = clock_systimer();
    bool b= cr.get_section("dummy", m);
    //systime_t en = clock_systimer();
    //printf("elapsed time %d us\n", TICK2USEC(en-st));
    std::cout << m << "\n";

    TEST_ASSERT_TRUE(b);
    TEST_ASSERT_EQUAL_INT(1, m.size());
    TEST_ASSERT_TRUE(m.find("enable") != m.end());
    TEST_ASSERT_EQUAL_STRING("false", m["enable"].c_str());
    TEST_ASSERT_FALSE(cr.get_bool(m, "enable", true));
}

REGISTER_TEST(ConfigTest, load_sub_sections)
{
    std::stringstream ss1(str);
    ConfigReader cr(ss1);

    ConfigReader::sub_section_map_t ssmap;
    TEST_ASSERT_TRUE(ssmap.empty());

    //systime_t st = clock_systimer();
    TEST_ASSERT_TRUE(cr.get_sub_sections("switch", ssmap));
    //systime_t en = clock_systimer();
    // printf("elapsed time %d us\n", TICK2USEC(en-st));

    TEST_ASSERT_EQUAL_STRING("switch", cr.get_current_section().c_str());
    TEST_ASSERT_EQUAL_INT(3, ssmap.size());

    TEST_ASSERT_TRUE(ssmap.find("fan") != ssmap.end());
    TEST_ASSERT_TRUE(ssmap.find("misc") != ssmap.end());
    TEST_ASSERT_TRUE(ssmap.find("psu") != ssmap.end());

    TEST_ASSERT_EQUAL_INT(1, ssmap["psu"].size());
    TEST_ASSERT_EQUAL_INT(5, ssmap["fan"].size());
    TEST_ASSERT_EQUAL_INT(7, ssmap["misc"].size());

    bool fanok= false;
    bool miscok= false;
    bool psuok= false;
    for(auto& i : ssmap) {
        // foreach switch
        std::string name= i.first;
        auto& m= i.second;
        if(cr.get_bool(m, "enable", false)) {
            const char* pin= cr.get_string(m, "output_pin", "nc");
            const char* input_on_command = cr.get_string(m, "input_on_command", "");
            const char* input_off_command = cr.get_string(m, "input_off_command", "");
            const char* output_on_command = cr.get_string(m, "output_on_command", "");
            const char* output_off_command = cr.get_string(m, "output_off_command", "");
            const char* type = cr.get_string(m, "output_type", "");
            const char* ipb = cr.get_string(m, "input_pin_behavior", "momentary");
            int iv= cr.get_int(m, "ivalue", 0);
            float fv= cr.get_float(m, "value", 0.0F);

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
                fanok= true;

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
                miscok= true;

            } else if(name == "psu") {
                psuok= true;

            } else {
                TEST_FAIL();
            }
        }
    }
    TEST_ASSERT_TRUE(fanok);
    TEST_ASSERT_TRUE(miscok);
    TEST_ASSERT_FALSE(psuok);
}

REGISTER_TEST(ConfigTest, read_bad_values)
{
    std::stringstream ss("[dummy]\nbad_int = fred\nbad_float = dan\n");
    ConfigReader cr(ss);

    ConfigReader::section_map_t m;
    bool b= cr.get_section("dummy", m);
    TEST_ASSERT_TRUE(b);
    TEST_ASSERT_EQUAL_INT(2, m.size());
    TEST_ASSERT_TRUE(m.find("bad_int") != m.end());
    TEST_ASSERT_EQUAL_STRING("fred", m["bad_int"].c_str());
    TEST_ASSERT_EQUAL_INT(0, cr.get_int(m, "bad_int", -1));
    TEST_ASSERT_EQUAL_STRING("dan", m["bad_float"].c_str());
    TEST_ASSERT_EQUAL_FLOAT(0.0F, cr.get_int(m, "bad_float", -1));
}


REGISTER_TEST(ConfigTest, write_no_change)
{
    std::istringstream iss(str);
    std::ostringstream oss;
    ConfigWriter cw(iss, oss);
    TEST_ASSERT_TRUE(oss.str().empty());

    TEST_ASSERT_TRUE(cw.write("switch", "fan.enable", "true"));
    TEST_ASSERT_FALSE(oss.str().empty());
    TEST_ASSERT_TRUE(oss.str() == iss.str());
}

REGISTER_TEST(ConfigTest, write_change_value)
{
    std::istringstream iss(str);
    std::ostringstream oss;
    ConfigWriter cw(iss, oss);
    TEST_ASSERT_TRUE(oss.str().empty());

    TEST_ASSERT_TRUE(cw.write("switch", "misc.enable", "false"));
    TEST_ASSERT_FALSE(oss.str().empty());
    TEST_ASSERT_FALSE(oss.str() == iss.str());


    auto pos = oss.str().find("misc.enable");
    TEST_ASSERT_TRUE(pos != std::string::npos);
    TEST_ASSERT_EQUAL_INT(iss.str().find("misc.enable"), pos);

    // check it is the same upto that change
    TEST_ASSERT_TRUE(oss.str().substr(0, pos + 11) == iss.str().substr(0, pos + 11));

    // make sure it was changed to false
    TEST_ASSERT_EQUAL_STRING("false", oss.str().substr(pos + 14, 5).c_str());

    // check rest is unchanged
    TEST_ASSERT_TRUE(oss.str().substr(pos + 19) == iss.str().substr(pos + 18));
}

REGISTER_TEST(ConfigTest, write_new_section)
{
    std::istringstream iss(str);
    std::ostringstream oss;
    ConfigWriter cw(iss, oss);
    TEST_ASSERT_TRUE(oss.str().empty());

    TEST_ASSERT_TRUE(cw.write("new_section", "key1", "key2"));
    TEST_ASSERT_FALSE(oss.str().empty());
    TEST_ASSERT_FALSE(oss.str() == iss.str());

    auto pos = oss.str().find("[new_section]\nkey1 = key2\n");
    TEST_ASSERT_TRUE(pos != std::string::npos);
    TEST_ASSERT_EQUAL_INT(iss.str().size(), pos);
    TEST_ASSERT_TRUE(oss.str().substr(0, pos) == iss.str().substr(0, pos));
}

REGISTER_TEST(ConfigTest, write_new_key_to_section)
{
    std::istringstream iss(str);
    std::ostringstream oss;
    ConfigWriter cw(iss, oss);
    TEST_ASSERT_TRUE(oss.str().empty());

    TEST_ASSERT_TRUE(cw.write("switch", "new.enable", "false"));
    TEST_ASSERT_FALSE(oss.str().empty());
    TEST_ASSERT_FALSE(oss.str() == iss.str());

    // find new entry
    auto pos = oss.str().find("new.enable = false\n");
    TEST_ASSERT_TRUE(pos != std::string::npos);

    // check it is the same upto that change
    TEST_ASSERT_TRUE(oss.str().substr(0, pos) == iss.str().substr(0, pos));

    // make sure it was inserted at the end of the [switch] section
    TEST_ASSERT_EQUAL_INT(iss.str().find("[dummy]"), pos);

    // check rest is unchanged
    TEST_ASSERT_EQUAL_STRING(oss.str().substr(pos + 20).c_str(), iss.str().substr(pos).c_str());
}

REGISTER_TEST(ConfigTest, write_remove_key_from_section)
{
    std::istringstream iss(str);
    std::ostringstream oss;
    ConfigWriter cw(iss, oss);
    TEST_ASSERT_TRUE(oss.str().empty());

    TEST_ASSERT_TRUE(cw.write("switch", "fan.input_on_command", nullptr));
    TEST_ASSERT_FALSE(oss.str().empty());
    TEST_ASSERT_FALSE(oss.str() == iss.str());

    // make sure entry is gone
    TEST_ASSERT_TRUE(std::string::npos == oss.str().find("fan.input_on_command"));

    // check it is the same upto that change
    auto pos = iss.str().find("fan.input_on_command");
    TEST_ASSERT_TRUE(pos != std::string::npos);
    TEST_ASSERT_TRUE(oss.str().substr(0, pos) == iss.str().substr(0, pos));

    // check rest is unchanged
    TEST_ASSERT_EQUAL_STRING(oss.str().substr(pos).c_str(), iss.str().substr(pos + 38).c_str());
}

REGISTER_TEST(ConfigTest, write_remove_nonexistant_key_from_section)
{
    std::istringstream iss(str);
    std::ostringstream oss;
    ConfigWriter cw(iss, oss);
    TEST_ASSERT_TRUE(oss.str().empty());

    TEST_ASSERT_TRUE(cw.write("switch", "fan.xxx", nullptr));
    TEST_ASSERT_FALSE(oss.str().empty());

    // check it is unchanged
    TEST_ASSERT_TRUE(oss.str() == iss.str());
}

REGISTER_TEST(ConfigTest, write_remove_nonexistant_key)
{
    std::istringstream iss(str);
    std::ostringstream oss;
    ConfigWriter cw(iss, oss);
    TEST_ASSERT_TRUE(oss.str().empty());

    TEST_ASSERT_TRUE(cw.write("xxx", "yyy", nullptr));
    TEST_ASSERT_FALSE(oss.str().empty());

    // check it is unchanged
    TEST_ASSERT_TRUE(oss.str() == iss.str());
}
