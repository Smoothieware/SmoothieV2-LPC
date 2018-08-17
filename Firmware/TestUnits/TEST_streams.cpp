#include <iostream>
#include <sstream>
#include <set>
#include <tuple>

//#include "OutputStream.h"
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

REGISTER_TEST(StreamsTest, cout)
{
	std::cout << "Hello World!" << "\n";
	std::cout << "Hello World, " << 1.234F << " that was a number\n";
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

#if 0
REGISTER_TEST(StreamsTest, OutputStream_null)
{
	OutputStream os;
	os.printf("hello");
}

REGISTER_TEST(StreamsTest, OutputStream_sstream)
{
	std::ostringstream oss;
	OutputStream os(&oss);
	os.printf("hello world");
	printf("oss = %s\n", oss.str().c_str());
	std::cout << oss.str() << "\n";
	TEST_ASSERT_EQUAL_STRING("hello world", oss.str().c_str());
}

REGISTER_TEST(StreamsTest, OutputStream_fdstream)
{
	OutputStream os(1); // stdout
	os.printf("hello world on fd stdout OutputStream\n");

	// also test cout
	OutputStream os2(&std::cout);
	os2.printf("hello world from cout OutputStream\n");
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
	TEST_ASSERT_EQUAL_INT(135, n);
	TEST_ASSERT_EQUAL_STRING("123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012...", oss.str().c_str());
}
#endif
