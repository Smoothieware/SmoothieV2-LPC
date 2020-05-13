#include "GCode.h"
#include "GCodeProcessor.h"

#include "../Unity/src/unity.h"
#include "TestRegistry.h"

#include <cstring>

REGISTER_TEST(GCodeTest,basic)
{
    GCodeProcessor gp;
    GCodeProcessor::GCodes_t gca;

    const char *g1("G32 X1.2 Y-2.3");
    bool ok= gp.parse(g1, gca);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_INT(1, gca.size());
    GCode gc1= gca[0];
    TEST_ASSERT_TRUE(gc1.has_g());
    TEST_ASSERT_TRUE(!gc1.has_m());
    TEST_ASSERT_EQUAL_INT(32, gc1.get_code());
    TEST_ASSERT_EQUAL_INT(0, gc1.get_subcode());
    TEST_ASSERT_EQUAL_INT(2, gc1.get_num_args());
    TEST_ASSERT_TRUE(gc1.has_arg('X'));
    TEST_ASSERT_TRUE(gc1.has_arg('Y'));
    TEST_ASSERT_TRUE(!gc1.has_arg('Z'));
    TEST_ASSERT_EQUAL_FLOAT(1.2, gc1.get_arg('X'));
    TEST_ASSERT_EQUAL_FLOAT(-2.3, gc1.get_arg('Y'));
}

GCode gc2;

REGISTER_TEST(GCodeTest,subcode)
{
    GCodeProcessor gp;
    GCodeProcessor::GCodes_t gca;
    const char *g2("G32.2 X1.2 Y2.3");
    bool ok= gp.parse(g2, gca);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_INT(1, gca.size());
    gc2= gca[0];

    TEST_ASSERT_TRUE(gc2.has_g());
    TEST_ASSERT_TRUE(!gc2.has_m());
    TEST_ASSERT_EQUAL_INT(32, gc2.get_code());
    TEST_ASSERT_EQUAL_INT(2, gc2.get_subcode());
    TEST_ASSERT_EQUAL_INT(2, gc2.get_num_args());
    TEST_ASSERT_TRUE(gc2.has_arg('X'));
    TEST_ASSERT_TRUE(gc2.has_arg('Y'));
    TEST_ASSERT_EQUAL_FLOAT(1.2, gc2.get_arg('X'));
    TEST_ASSERT_EQUAL_FLOAT(2.3, gc2.get_arg('Y'));
}

REGISTER_TEST(GCodeTest,copy)
{
    // test equals
    GCode gc3;
    gc3= gc2;
    TEST_ASSERT_TRUE(gc3.has_g());
    TEST_ASSERT_TRUE(!gc3.has_m());
    TEST_ASSERT_EQUAL_INT(32, gc3.get_code());
    TEST_ASSERT_EQUAL_INT(2, gc3.get_subcode());
    TEST_ASSERT_EQUAL_INT(2, gc3.get_num_args());
    TEST_ASSERT_TRUE(gc3.has_arg('X'));
    TEST_ASSERT_TRUE(gc3.has_arg('Y'));
    TEST_ASSERT_EQUAL_FLOAT(1.2, gc3.get_arg('X'));
    TEST_ASSERT_EQUAL_FLOAT(2.3, gc3.get_arg('Y'));

    // test copy ctor
    GCode gc4(gc2);
	TEST_ASSERT_TRUE(gc4.has_g());
	TEST_ASSERT_TRUE(!gc4.has_m());
	TEST_ASSERT_EQUAL_INT(32, gc4.get_code());
	TEST_ASSERT_EQUAL_INT(2, gc4.get_subcode());
	TEST_ASSERT_EQUAL_INT(2, gc4.get_num_args());
	TEST_ASSERT_TRUE(gc4.has_arg('X'));
	TEST_ASSERT_TRUE(gc4.has_arg('Y'));
	TEST_ASSERT_EQUAL_FLOAT(1.2, gc4.get_arg('X'));
	TEST_ASSERT_EQUAL_FLOAT(2.3, gc4.get_arg('Y'));
}

REGISTER_TEST(GCodeTest, Multiple_commands_on_line_no_spaces) {
    GCodeProcessor gp;
    const char *gc= "M123X1Y2G1X10Y20Z0.634";
    GCodeProcessor::GCodes_t gcodes;
    bool ok= gp.parse(gc, gcodes);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_INT( 2, gcodes.size());
    auto a= gcodes[0];
    TEST_ASSERT_TRUE(a.has_m());
    TEST_ASSERT_EQUAL_INT(123, a.get_code());
    TEST_ASSERT_TRUE(a.has_arg('X')); TEST_ASSERT_EQUAL_INT(1, a.get_arg('X'));
    TEST_ASSERT_TRUE(a.has_arg('Y')); TEST_ASSERT_EQUAL_INT(2, a.get_arg('Y'));
    auto b= gcodes[1];
    TEST_ASSERT_TRUE(b.has_g());
    TEST_ASSERT_EQUAL_INT(1, b.get_code());
    TEST_ASSERT_TRUE(b.has_arg('X')); TEST_ASSERT_EQUAL_INT(10, b.get_arg('X'));
    TEST_ASSERT_TRUE(b.has_arg('Y')); TEST_ASSERT_EQUAL_INT(20, b.get_arg('Y'));
    TEST_ASSERT_TRUE(b.has_arg('Z')); TEST_ASSERT_EQUAL_INT(0.634f, b.get_arg('Z'));
}

REGISTER_TEST(GCodeTest, Modal_G1_and_comments) {
    GCodeProcessor gp;
    GCodeProcessor::GCodes_t gcodes;
    bool ok= gp.parse("G1 X0", gcodes);
    TEST_ASSERT_TRUE(ok);
    const char *gc= "( this is a comment )X100Y200 ; G23 X0";
    gcodes.clear();
    ok= gp.parse(gc, gcodes);
    printf("%s\n", gcodes[0].get_error_message());
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_INT(1, gcodes.size());
    auto a = gcodes[0];
    TEST_ASSERT_TRUE(a.has_g());
    TEST_ASSERT_EQUAL_INT(1, a.get_code());
    TEST_ASSERT_TRUE(a.has_arg('X')); TEST_ASSERT_EQUAL_INT(100, a.get_arg('X'));
    TEST_ASSERT_TRUE(a.has_arg('Y')); TEST_ASSERT_EQUAL_INT(200, a.get_arg('Y'));
    TEST_ASSERT_FALSE(a.has_arg('Z'));
}

REGISTER_TEST(GCodeTest, Line_numbers_and_checksums) {
    GCodeProcessor gp;
    GCodeProcessor::GCodes_t gcodes;
    bool ok= gp.parse("N10 G1 X0", gcodes);
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_TRUE(gcodes.empty());

    gcodes.clear();
    ok= gp.parse("N10 M110*123", gcodes);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_INT(10, gp.get_line_number());
    TEST_ASSERT_TRUE(gcodes.empty());

    // Bad line number
    gcodes.clear();
    ok= gp.parse("N95 G1 X-4.992 Y-14.792 F12000.000*97", gcodes);
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_TRUE(gcodes.empty());

    gcodes.clear();
    ok= gp.parse("N94 M110*123", gcodes);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_INT(94, gp.get_line_number());
    ok= gp.parse("N95 G1 X-4.992 Y-14.792 F12000.000*97", gcodes);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_INT(1, gcodes.size());

    // Bad checksum
    gcodes.clear();
    ok= gp.parse("N94 M110*123", gcodes);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_INT(94, gp.get_line_number());
    ok= gp.parse("N95 G1 X-4.992 Y-14.792 F12000.000*98", gcodes);
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_TRUE(gcodes.empty());
}

REGISTER_TEST(GCodeTest,t_code)
{
    GCodeProcessor gp;
    GCodeProcessor::GCodes_t gca;

    // test that a T1 will be converted effectively to M6 T1
    const char *g1("T1");
    bool ok= gp.parse(g1, gca);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_INT(1, gca.size());
    GCode gc1= gca[0];
    TEST_ASSERT_FALSE(gc1.has_g());
    TEST_ASSERT_TRUE(gc1.has_m());
    TEST_ASSERT_EQUAL_INT(6, gc1.get_code());
    TEST_ASSERT_EQUAL_INT(0, gc1.get_subcode());
    TEST_ASSERT_EQUAL_INT(1, gc1.get_num_args());
    TEST_ASSERT_TRUE(gc1.has_arg('T'));
    TEST_ASSERT_EQUAL_INT(1, gc1.get_arg('T'));
}

REGISTER_TEST(GCodeTest, illegal_command_word) {
    GCodeProcessor gp;
    GCodeProcessor::GCodes_t gcodes;
    bool ok= gp.parse("1 X0", gcodes);
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_FALSE(gcodes.empty());
    TEST_ASSERT_TRUE(gcodes.back().has_error());
    TEST_ASSERT_NOT_NULL(gcodes.back().get_error_message());
    TEST_ASSERT_TRUE(strlen(gcodes.back().get_error_message()) > 0);
}

REGISTER_TEST(GCodeTest, illegal_parameter_word) {
    GCodeProcessor gp;
    GCodeProcessor::GCodes_t gcodes;
    bool ok= gp.parse("G1 1.2 X0", gcodes);
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_FALSE(gcodes.empty());
    TEST_ASSERT_TRUE(gcodes.back().has_error());
    TEST_ASSERT_NOT_NULL(gcodes.back().get_error_message());
    TEST_ASSERT_TRUE(strlen(gcodes.back().get_error_message()) > 0);
}

REGISTER_TEST(GCodeTest, illegal_parameter_word_value) {
    GCodeProcessor gp;
    GCodeProcessor::GCodes_t gcodes;
    bool ok= gp.parse("G1 Y X0", gcodes);
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_FALSE(gcodes.empty());
    TEST_ASSERT_TRUE(gcodes.back().has_error());
    TEST_ASSERT_NOT_NULL(gcodes.back().get_error_message());
    TEST_ASSERT_TRUE(strlen(gcodes.back().get_error_message()) > 0);
}
