#include "OutputStream.h"

#include <cstdarg>
#include <cstring>
#include <stdio.h>
#include <stdlib.h>

OutputStream::OutputStream(wrfnc f) : append_nl(false), prepend_ok(false), deleteos(true)
{
	// create an output stream using the given write fnc
	fdbuf = new FdBuf(f);
	os = new std::ostream(fdbuf);
	*os << std::unitbuf; // auto flush on every write
}

OutputStream::~OutputStream()
{
	if(deleteos)
		delete os;
	if(fdbuf)
		delete fdbuf;
};

int OutputStream::flush_prepend()
{
	int n = prepending.size();
	if(n > 0) {
		os->write(prepending.c_str(), n);
		prepending.clear();
		prepend_ok = false;
	}
	return n;
}

int OutputStream::write(const char *buffer, size_t size)
{
	if(os == nullptr) return 0;
	if(prepend_ok) {
		prepending.append(buffer, size);
	} else {
		os->write((const char*)buffer, size);
	}
	return size;
}

int OutputStream::puts(const char *str)
{
	if(os == nullptr) return 0;
	size_t n = strlen(str);
	return this->write(str, n);
}

int OutputStream::printf(const char *format, ...)
{
	if(os == nullptr) return 0;

	char buffer[132];
	va_list args;
	va_start(args, format);

	size_t size = vsnprintf(buffer, sizeof(buffer), format, args);
	if (size > sizeof(buffer)) {
		// too big for internal buffer so allocate it from heap
		char *buf= (char *)malloc(size+1);
		size = vsnprintf(buf, size+1, format, args);
		int n= this->write(buf, size);
		free(buf);
		va_end(args);
		return n;

	}else{
		va_end(args);
	}

	return this->write(buffer, size);
}

int OutputStream::FdBuf::sync()
{
	// USB CDC can't write more than 64 bytes at a time so limit it here
	if(!this->str().empty()) {
		size_t len = this->str().size();
		if(len <= 64) {
			// write fnc may return less than len need to address that case
			size_t sent_cnt = 0;
			while(sent_cnt < len) {
				int n = fnc(this->str().substr(sent_cnt, len).c_str(), len-sent_cnt);
				if(n >= 0) {
					sent_cnt += n;
				} else {
					::printf("error: write fnc failed\n");
					break;
				}
			}

			//::printf("write: %d\n", s);

		} else {
			// FIXME: hack before we fix the cdc driver
			size_t n = len;
			int off = 0;
			while(n > 0) {
				int s = std::min((size_t)64, n);
				s = fnc(this->str().substr(off, s).c_str(), s);
				if( s >= 0) {
					off += s;
					n -= s;
				} else {
					::printf("error: write fnc failed\n");
					break;
				}
			}
		}
		this->str("");
	}

	return 0;
}
