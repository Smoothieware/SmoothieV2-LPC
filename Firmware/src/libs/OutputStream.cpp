#include "OutputStream.h"

#include <cstdarg>
#include <cstring>
#include <stdio.h>
#include <stdlib.h>

#include "FreeRTOS.h"
#include "semphr.h"

OutputStream::OutputStream(wrfnc f) : deleteos(true)
{
	clear_flags();
	stop_request= false;
	// create an output stream using the given write fnc
	fdbuf = new FdBuf(this, f);
	os = new std::ostream(fdbuf);
	*os << std::unitbuf; // auto flush on every write
	xWriteMutex = xSemaphoreCreateMutex();
}

OutputStream::~OutputStream()
{
	if(deleteos)
		delete os;
	if(fdbuf)
		delete fdbuf;
	if(xWriteMutex != nullptr)
		vSemaphoreDelete(xWriteMutex);
};

int OutputStream::flush_prepend()
{
	int n = prepending.size();
	if(n > 0) {
		prepend_ok = false;
		this->write(prepending.c_str(), n);
		prepending.clear();
	}
	return n;
}

// this needs to be protected by a semaphore as it could be preempted by another
// task which can write as well
int OutputStream::write(const char *buffer, size_t size)
{
	if(os == nullptr || closed) return 0;
	if(xWriteMutex != nullptr)
		xSemaphoreTake(xWriteMutex, portMAX_DELAY);
	if(prepend_ok) {
		prepending.append(buffer, size);
	} else {
		// this is expected to always write everything out
		os->write(buffer, size);
	}
	if(xWriteMutex != nullptr)
		xSemaphoreGive(xWriteMutex);
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
	int ret= 0;
	size_t len= this->str().size();
	if(!parent->closed && len > 0) {
		// fnc is expected to write everything
		size_t n = fnc(this->str().data(), len);
		if(n != len) {
			::printf("OutputStream error: write fnc failed\n");
			parent->set_closed();
			ret= -1;
		}
		this->str("");
	}

	return ret;
}
