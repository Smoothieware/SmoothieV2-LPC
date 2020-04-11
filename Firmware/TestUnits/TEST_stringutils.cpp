#include "StringUtils.h"

#include <vector>
#include <stdio.h>
#include <string.h>

#include "../Unity/src/unity.h"
#include "TestRegistry.h"

REGISTER_TEST(UtilsTest,split)
{
    const char *s= "one two three";
    std::vector<std::string> v= stringutils::split(s, ' ');
    TEST_ASSERT_EQUAL_INT(3, v.size());
    TEST_ASSERT_TRUE(v[0] == "one");
    TEST_ASSERT_TRUE(v[1] == "two");
    TEST_ASSERT_TRUE(v[2] == "three");
}

REGISTER_TEST(UtilsTest,split_empty_string)
{
    const char *s= "";
    std::vector<std::string> v= stringutils::split(s, ' ');

    TEST_ASSERT_EQUAL_INT(0, v.size());
}

REGISTER_TEST(UtilsTest,parse_number_list)
{
    const char *s= "1.1,2.2,3.3";
    std::vector<float> v= stringutils::parse_number_list(s);
    TEST_ASSERT_EQUAL_INT(3, v.size());
    TEST_ASSERT_TRUE(v[0] == 1.1F);
    TEST_ASSERT_TRUE(v[1] == 2.2F);
    TEST_ASSERT_TRUE(v[2] == 3.3F);
}

REGISTER_TEST(UtilsTest,shift_parameter)
{
	std::string params= "one two three";
	std::string fn1 = stringutils::shift_parameter( params );
	TEST_ASSERT_EQUAL_STRING("one", fn1.c_str());
	TEST_ASSERT_EQUAL_STRING("two three", params.c_str());
    std::string fn2 = stringutils::shift_parameter( params );
	TEST_ASSERT_EQUAL_STRING("two", fn2.c_str());
	TEST_ASSERT_EQUAL_STRING("three", params.c_str());
    std::string fn3 = stringutils::shift_parameter( params );
	TEST_ASSERT_EQUAL_STRING("three", fn3.c_str());
	TEST_ASSERT_TRUE(params.empty());
}

REGISTER_TEST(UtilsTest,shift_parameter_with_quotes)
{
	std::string params= "one \"two three\" \"four\" \"five six\"";
	std::string fn1 = stringutils::shift_parameter( params );
	TEST_ASSERT_EQUAL_STRING("one", fn1.c_str());
	TEST_ASSERT_EQUAL_STRING("\"two three\" \"four\" \"five six\"", params.c_str());
    std::string fn2 = stringutils::shift_parameter( params );
	TEST_ASSERT_EQUAL_STRING("two three", fn2.c_str());
	TEST_ASSERT_EQUAL_STRING("\"four\" \"five six\"", params.c_str());
    std::string fn3 = stringutils::shift_parameter( params );
	TEST_ASSERT_EQUAL_STRING("four", fn3.c_str());
	TEST_ASSERT_EQUAL_STRING("\"five six\"", params.c_str());
    std::string fn4 = stringutils::shift_parameter( params );
	TEST_ASSERT_EQUAL_STRING("five six", fn4.c_str());
	TEST_ASSERT_TRUE(params.empty());
}


