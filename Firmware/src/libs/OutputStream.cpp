#include "OutputStream.h"

#include <cstdarg>
#include <cstring>
#include <stdio.h>
#include <stdlib.h>

OutputStream::OutputStream(wrfnc f) : deleteos(true)
{
	clear_flags();
	// create an output stream using the given write fnc
	fdbuf = new FdBuf(this, f);
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
	if(os == nullptr || closed) return 0;
	if(prepend_ok) {
		prepending.append(buffer, size);
	} else {
		// this is expected to always write everything out
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
	if (size >= sizeof(buffer)) {
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
	if(!this->str().empty()) {
		size_t len = this->str().size();
		int n = fnc(this->str().c_str(), len);
		if(n < 0) {
			::printf("OutputStream error: write fnc failed\n");
			parent->set_closed();
		}
		this->str("");
	}

	return 0;
}
