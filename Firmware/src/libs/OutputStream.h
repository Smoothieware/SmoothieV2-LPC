#pragma once

#include <string>
#include <ostream>
#include <sstream>
#include <unistd.h>

/**
	Handles an output stream from gcode/mcode handlers
	can be told to append a NL at end, and also to prepend or postpend the ok
*/
class OutputStream
{
public:
	// create a null output stream
	OutputStream() : os(nullptr), fdbuf(nullptr), append_nl(false), prepend_ok(false), deleteos(false) {};
	// create from an existing ostream
	OutputStream(std::ostream *o) : os(o), fdbuf(nullptr), append_nl(false), prepend_ok(false), deleteos(false) {};
	// create from an open file descriptor
	OutputStream(int fd);

	virtual ~OutputStream();

	void clear() { append_nl = false; prepend_ok = false; prepending.clear(); if(fdbuf != nullptr) fdbuf->str(""); }
	int write(const char *buffer, size_t size);
	int printf(const char *format, ...);
	int puts(const char *str);
	void set_append_nl(bool flg = true) { append_nl = flg; }
	void set_prepend_ok(bool flg = true) { prepend_ok = flg; }
	bool is_append_nl() const { return append_nl; }
	bool is_prepend_ok() const { return prepend_ok; }
	int flush_prepend();

private:
	// Hack to allow us to create a ostream writing to the USBCDC fd we have open
	class FdBuf : public std::stringbuf
	{
	public:
		FdBuf(int f) : fd(f) {};
		virtual int sync();
	private:
		int fd;
	};

	std::ostream *os;
	FdBuf *fdbuf;
	std::string prepending;
	struct {
		bool append_nl: 1;
		bool prepend_ok: 1;
		bool deleteos: 1;
	};
};
