#include "serial.h"

#include <functional>
#include <iostream>

#if ASIO_VERSION < 101601
#error need ASIO_VERSION >= 101601
#endif

using std::placeholders::_1;
using std::placeholders::_2;

MySerial::MySerial(): io(), port(io), backgroundThread()
{
}

MySerial::~MySerial()
{
}

bool MySerial::open(const char *devname, unsigned int baud_rate,
                    asio::serial_port_base::parity opt_parity,
                    asio::serial_port_base::character_size opt_csize,
                    asio::serial_port_base::flow_control opt_flow,
                    asio::serial_port_base::stop_bits opt_stop)
{
	try {
		this->port.open(devname);
		this->port.set_option(asio::serial_port_base::baud_rate(baud_rate));
		this->port.set_option(opt_parity);
		this->port.set_option(opt_csize);
		this->port.set_option(opt_flow);
		this->port.set_option(opt_stop);
	} catch(std::exception& e) {
		std::cout << "Error (open): " << e.what() << "\n";
		return false;
	}

	//This gives some work to the io_service before it is started
	this->io.post(std::bind(&MySerial::do_read, this));

	std::thread t([this]() { this->io.run(); });
	this->backgroundThread.swap(t);
	return true;
}

void MySerial::close()
{
	this->io.post(std::bind(&MySerial::do_close, this));
	this->backgroundThread.join();
	this->io.reset();
	priqueue.clear();
	queue.clear();
	last_sent.clear();
}

void MySerial::do_close()
{
	asio::error_code ec;
	this->port.cancel(ec);
	this->port.close(ec);
}

void MySerial::do_read()
{
	// read one line
	asio::async_read_until(
	    this->port,
	    asio::dynamic_buffer(this->input_buffer),
	    '\n',
	    std::bind(&MySerial::handle_read, this, _1, _2)
	);
}

void MySerial::handle_read(const asio::error_code& error, std::size_t n)
{
	if (!error) {
		// Extract the newline-delimited message from the buffer.
		std::string msg(input_buffer.substr(0, n - 1));
		input_buffer.erase(0, n);

		if (!msg.empty()) {
			if(read_callback) {
				if(!read_callback(msg, false)) close();
			}
		}

		do_read();
	} else {
		if(error.value() != 125) {
			std::cout << "error (handle_read): " << error.message() << " " << error.value() << "\n";
			if(read_callback) {
				read_callback(error.message(), true);
			}
		}
	}
}

void MySerial::send(const std::vector<std::string> &lines, bool priority)
{
	// append lines to queue
	{
		std::lock_guard<std::mutex> l(this->queue_mutex);
		for (std::vector<std::string>::const_iterator line = lines.begin(); line != lines.end(); ++line) {
			if (priority) {
				this->priqueue.push_back(*line);
			} else {
				this->queue.push_back(*line);
			}
		}
	}
	this->send();
}

void MySerial::send(const std::string &line, bool priority)
{
	// append line to queue
	{
		std::lock_guard<std::mutex> l(this->queue_mutex);
		if (priority) {
			this->priqueue.push_back(line);
		} else {
			this->queue.push_back(line);
		}
	}
	this->send();
}

void MySerial::send()
{
	this->io.post(std::bind(&MySerial::do_send, this));
}

void MySerial::do_send()
{
	std::lock_guard<std::mutex> l(this->queue_mutex);
 	if(this->priqueue.empty() && this->queue.empty()) return;

	std::string line;

	if (!this->priqueue.empty()) {
		line = this->priqueue.front();
		this->priqueue.pop_front();
	} else {
		line = this->queue.front();
		this->queue.pop_front();
	}

	// we save it so memory is preserved until write completes
	last_sent.push_back(line);
	// Start an asynchronous operation to send a message.
	asio::async_write(this->port, asio::buffer(last_sent.back()),
	                  std::bind(&MySerial::handle_write, this, _1));

}

void MySerial::handle_write(const asio::error_code& error)
{
	bool more;
	std::string line;
	{
		// release the last line sent as it has completed
		std::lock_guard<std::mutex> l(this->queue_mutex);
		line= last_sent.front();
		last_sent.pop_front();
	 	more= !(this->priqueue.empty() && this->queue.empty());
	}

	if (error) {
		std::cout << "Error on write: " << error.message() << ", " << line << "\n";
	}else{
		// std::cout << "Wrote: " << line << "\n";
		if(more) {
	 		// more in queue to send
			do_send();
		}
	}
}

bool MySerial::is_sent()
{
	std::lock_guard<std::mutex> l(this->queue_mutex);
 	return (this->priqueue.empty() && this->queue.empty() && this->last_sent.empty());
}
