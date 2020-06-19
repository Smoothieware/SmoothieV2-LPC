#include <iostream>
#include <sstream>
#include <set>
#include <tuple>
#include <vector>
#include <cstring>

#include "OutputStream.h"
#include "prettyprint.hpp"
#include "../Unity/src/unity.h"

#include "TestRegistry.h"

// just swaps the parameters
#define TEST_ASSERT_STRING_S(a, b) TEST_ASSERT_EQUAL_STRING(b, a)

REGISTER_TEST(StreamsTest, stringstream)
{
	std::ostringstream oss;
	oss << "Hello World!";
	TEST_ASSERT_STRING_S(oss.str().c_str(), "Hello World!");

	std::ostringstream oss2;
	oss2.write("hello", 5);

	TEST_ASSERT_STRING_S(oss2.str().c_str(), "hello");
}

REGISTER_TEST(StreamsTest, xprintf)
{
	printf("Hello %s\n", "xprintf");
	char buf[32]{0};
	int n= snprintf(buf, 31, "%s", "This is a test");
	TEST_ASSERT_EQUAL_INT(14, n);
	TEST_ASSERT_STRING_S(buf, "This is a test");

	char *str= new char[25];
	*str= '\0';
	strcpy(str, "12345678901234567890");
	n= snprintf(buf, 10, "%s", str);
	TEST_ASSERT_EQUAL_INT(20, n);
	TEST_ASSERT_STRING_S(buf, "123456789");


}

REGISTER_TEST(StreamsTest, cout)
{
	std::cout << "Hello World!" << "\n";
	std::cout << "Hello World, " << 1.234F << " that was a float 1.234\n";
	std::cout << "Hello World, " << 1234 << " that was an int 1234\n";
	std::set<int> s;
	s.insert(1);
	s.insert(2);
	s.insert(3);
	s.insert(4);

	// test prettyprint (also tests stringstream)
	{
		std::ostringstream oss;
		oss << s;
		TEST_ASSERT_STRING_S(oss.str().c_str(), "{1, 2, 3, 4}");
	}

	auto t= std::make_tuple(1,2,3,4);
	{
		std::ostringstream oss;
		oss << t;
		printf("tuple: %s\n", oss.str().c_str());
		TEST_ASSERT_STRING_S(oss.str().c_str(), "(1, 2, 3, 4)");
	}
}

REGISTER_TEST(StreamsTest, OutputStream_null)
{
	OutputStream os;
	os.printf("hello null stream");
}

REGISTER_TEST(StreamsTest, OutputStream_sstream)
{
	std::ostringstream oss;
	OutputStream os(&oss);
	os.printf("hello world: %1.1f", 1.2F);
	printf("oss = %s\n", oss.str().c_str());
	std::cout << oss.str() << "\n";
	TEST_ASSERT_EQUAL_STRING("hello world: 1.2", oss.str().c_str());
	// also test cout
	OutputStream os2(&std::cout);
	os2.printf("hello world from cout OutputStream\n");
}

static int stdout_write_fnc(const char *buf, size_t len)
{
	return write(1, buf, len);
}
static std::ostringstream toss;
static int partial_write_fnc(const char *buf, size_t len)
{
	size_t sent= 0;
	while(sent < len) {
		size_t n= std::min(4U, len-sent);
		toss.write(buf+sent, n);
		sent += n;
	}
	return sent;
}

REGISTER_TEST(StreamsTest, OutputStream_fncstream)
{
	OutputStream::wrfnc fnc(stdout_write_fnc);
	OutputStream os(fnc); // stdout
	os.printf("hello world on fd stdout OutputStream\n");

	// test that write fncs returning < len work
	TEST_ASSERT_TRUE(toss.str().empty());
	OutputStream::wrfnc fnc2(partial_write_fnc);
	OutputStream os2(fnc2);
	int n= os2.puts("1234567890");
	TEST_ASSERT_EQUAL_INT(10, n);
	TEST_ASSERT_EQUAL_STRING("1234567890", toss.str().c_str());
	toss.str("");
}

REGISTER_TEST(StreamsTest, OutputStream_prependok)
{
	std::ostringstream oss;
	OutputStream os(&oss);
	// output the result after the ok
	os.set_prepend_ok(true);
	os.printf("This is after the ok\n");
	os.set_prepend_ok(false);
	os.printf("ok ");
	int n= os.flush_prepend(); // this flushes the internally stored string to the output
	TEST_ASSERT_EQUAL_INT(21, n);
	TEST_ASSERT_EQUAL_STRING("ok This is after the ok\n", oss.str().c_str());
}

REGISTER_TEST(StreamsTest, OutputStream_long_line)
{
	std::ostringstream oss;
	OutputStream os(&oss);
	int n= os.printf("12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890");
	TEST_ASSERT_EQUAL_INT(140, n);
	TEST_ASSERT_EQUAL_STRING("12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890", oss.str().c_str());
}

