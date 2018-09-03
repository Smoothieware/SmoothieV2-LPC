#include "ConfigWriter.h"
#include "ConfigReader.h"
#include "StringUtils.h"

#include <cstring>

/*
 * We create a new config.ini file
 * copy all lines to the new file until we hit a match for the line we want to change
 * write the new line to the file then copy the rest of the lines
 * then rename current file to config.ini.bak
 * rename the new file to config.ini
 *
 * if we do not find the line we want we insert it at the end of the section
 * then copy the rest of the file
 *
 * This is easier than trying to insert or update the line in place
 *
 */

// TODO there is a lot of code duplication here, need to refactor to simplify and remove it
bool ConfigWriter::write(const char *section, const char* key, const char *value)
{
    bool in_section = false;
    bool left_section = false;
    bool changed_line = false;

    // sanity check
    if(section == nullptr || strlen(section) == 0 || key == nullptr || strlen(key) == 0)
        return false;

    // first find the section we want
    std::string s;
    while (std::getline(is, s)) {

        if(changed_line) {
            // just copy the lines. no need to check anymore
            s.push_back('\n');
            os.write(s.c_str(), s.size());
            if(!os.good()) return false;
            continue;
        }

        std::string sn = stringutils::trim(s);

        // only check lines that are not blank and are not all comments
        if (sn.size() > 0 && sn[0] != '#') {
            std::string comment= ConfigReader::strip_comments(sn);

            std::string sec;

            if (ConfigReader::match_section(sn.c_str(), sec)) {
                // if this is the section we are looking for
                if(sec == section) {
                    in_section = true;

                } else if(in_section) {
                    // we are no longer in the section we want
                    left_section = true;
                    in_section = false;
                }
            }

            if(in_section) {
                std::string k;
                std::string v;
                // extract key/value from this line and check if it is the one we want
                if(ConfigReader::extract_key_value(sn.c_str(), k, v) && k == key) {
                    // this is the one we want to change
                    // write the new key value
                    // then copy the rest of the file
                    // if value is blank then we simply delete the key
                    if(value != nullptr && strlen(value) > 0) {
                        std::string new_line(key);
                        new_line.append(" = ");
                        new_line.append(value);
                        if(!comment.empty()) {
                            // append the original comment
                            new_line.append("  ");
                            new_line.append(comment);
                        }
                        new_line.push_back('\n');
                        os.write(new_line.c_str(), new_line.size());
                        if(!os.good()) return false;
                    }
                    changed_line = true;

                } else {
                    // in section but not the key/value we want or not a key/value just write it out
                    s.push_back('\n');
                    os.write(s.c_str(), s.size());
                    if(!os.good()) return false;
                }

            } else if(left_section) {
                // we just left the section, and we did not find the key/value so we write the new one
                // TODO this leaves a space after the last entry if there was one in the source file
                std::string new_line;
                if(value != nullptr && strlen(value) > 0) {
                    new_line.append(key);
                    new_line.append(" = ");
                    new_line.append(value);
                    new_line.append("\n\n");
                }
                // add the last read section header
                new_line.append(s);
                new_line.push_back('\n');
                os.write(new_line.c_str(), new_line.size());
                if(!os.good()) return false;
                changed_line = true;

            } else {
                // not in the section of interest so just copy lines
                s.push_back('\n');
                os.write(s.c_str(), s.size());
                if(!os.good()) return false;
            }

        } else {
            // not a line of interest so just copy line
            s.push_back('\n');
            os.write(s.c_str(), s.size());
            if(!os.good()) return false;
        }
    }

    // make sure we don't need to add a new section
    if(!changed_line && value != nullptr && strlen(value) > 0) {
        std::string new_line;
        if(!in_section) {
            // need to add new section at the end of the file, unless we were in the last section
            new_line.push_back('[');
            new_line.append(section);
            new_line.append("]\n");
        }
        new_line.append(key);
        new_line.append(" = ");
        new_line.append(value);
        new_line.push_back('\n');
        os.write(new_line.c_str(), new_line.size());
        return os.good();
    }
    return true;
}

#if 0
#include <iostream>
#include <fstream>

int main(int argc, char const *argv[])
{
    if(argc < 6) {
        std::cout << "Usage: infile outfile section key value\n";
        return 0;
    }

    std::fstream fsin;
    std::fstream fsout;
    fsin.open(argv[1], std::fstream::in);
    if(!fsin.is_open()) {
        std::cout << "Error opening file: " << argv[1] << "\n";
        return 0;
    }

    fsout.open(argv[2], std::fstream::out);
    if(!fsout.is_open()) {
        std::cout << "Error opening file: " << argv[2] << "\n";
        fsin.close();
        return 0;
    }

    ConfigWriter cw(fsin, fsout);

    const char *section = argv[3];
    const char *key = argv[4];
    const char *value = argv[5];

    if(cw.write(section, key, value)) {
        std::cout << "added ok\n";
    } else {
        std::cout << "failed to add\n";
        std::cout << "fsin: good()=" << fsin.good();
        std::cout << " eof()=" << fsin.eof();
        std::cout << " fail()=" << fsin.fail();
        std::cout << " bad()=" << fsin.bad() << "\n";

        std::cout << "failed to add\n";
        std::cout << "fsout: good()=" << fsout.good();
        std::cout << " eof()=" << fsout.eof();
        std::cout << " fail()=" << fsout.fail();
        std::cout << " bad()=" << fsout.bad();
    }

    fsin.close();
    fsout.close();

    return 1;
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


void ConfigTest_write_no_change(void)
{
    std::istringstream iss(str);
    std::ostringstream oss;
    ConfigWriter cw(iss, oss);
    TEST_ASSERT_TRUE(oss.str().empty());

    TEST_ASSERT_TRUE(cw.write("switch", "fan.enable", "true"));
    TEST_ASSERT_FALSE(oss.str().empty());
    TEST_ASSERT_TRUE(oss.str() == iss.str());
}

void ConfigTest_write_change_value(void)
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

void ConfigTest_write_new_section(void)
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

void ConfigTest_write_new_key_to_section(void)
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

void ConfigTest_write_remove_key_from_section(void)
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

void ConfigTest_write_remove_nonexistant_key_from_section(void)
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

void ConfigTest_write_remove_nonexistant_key(void)
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

int main()
{
    UNITY_BEGIN();
    RUN_TEST(ConfigTest_write_no_change);
    RUN_TEST(ConfigTest_write_change_value);
    RUN_TEST(ConfigTest_write_new_section);
    RUN_TEST(ConfigTest_write_new_key_to_section);
    RUN_TEST(ConfigTest_write_remove_key_from_section);
    RUN_TEST(ConfigTest_write_remove_nonexistant_key_from_section);
    RUN_TEST(ConfigTest_write_remove_nonexistant_key);

    return UNITY_END();
}
#endif
