#pragma once

#include <string>
#include <ostream>
#include <sstream>
#include <unistd.h>
#include <functional>

/**
	Handles an output stream from gcode/mcode handlers
	can be told to append a NL at end, and also to prepend or postpend the ok
*/
class OutputStream
{
public:
	using wrfnc = std::function<int(const char *buffer, size_t size)>;
	// create a null output stream
	OutputStream() : os(nullptr), fdbuf(nullptr), append_nl(false), prepend_ok(false), deleteos(false), no_response(false) {};
	// create from an existing ostream
	OutputStream(std::ostream *o) : os(o), fdbuf(nullptr), append_nl(false), prepend_ok(false), deleteos(false), no_response(false) {};
	// create using a supplied write fnc
	OutputStream(wrfnc f);

	virtual ~OutputStream();

	void clear() { append_nl = false; prepend_ok = false; no_response= false; prepending.clear(); if(fdbuf != nullptr) fdbuf->str(""); }
	int write(const char *buffer, size_t size);
	int printf(const char *format, ...);
	int puts(const char *str);
	void set_append_nl(bool flg = true) { append_nl = flg; }
	void set_prepend_ok(bool flg = true) { prepend_ok = flg; }
	void set_no_response(bool flg = true) { no_response = flg; }
	bool is_append_nl() const { return append_nl; }
	bool is_prepend_ok() const { return prepend_ok; }
	bool is_no_response() const { return no_response; }
	int flush_prepend();

private:
	// Hack to allow us to create a ostream writing to a supplied write function (used for the USBCDC)
	class FdBuf : public std::stringbuf
	{
	public:
		FdBuf(wrfnc f) : fnc(f) {};
		virtual int sync();
	private:
		wrfnc fnc;
	};

	std::ostream *os;
	FdBuf *fdbuf;
	std::string prepending;
	struct {
		bool append_nl: 1;
		bool prepend_ok: 1;
		bool deleteos: 1;
		bool no_response: 1;
	};
};
