// serial.h
#pragma once

#include <asio.hpp>
#include <thread>
#include <mutex>
#include <string>
#include <queue>
#include <list>
#include <functional>

class MySerial
{
public:
	MySerial();
	virtual ~MySerial();
	bool open(const char *devname, unsigned int baud_rate,
        asio::serial_port_base::parity opt_parity=
            asio::serial_port_base::parity(
                asio::serial_port_base::parity::none),
        asio::serial_port_base::character_size opt_csize=
            asio::serial_port_base::character_size(8),
        asio::serial_port_base::flow_control opt_flow=
            asio::serial_port_base::flow_control(
                asio::serial_port_base::flow_control::none),
        asio::serial_port_base::stop_bits opt_stop=
            asio::serial_port_base::stop_bits(
                asio::serial_port_base::stop_bits::one));

    void close();
    void send(const std::vector<std::string> &lines, bool priority = false);
    void send(const std::string &s, bool priority = false);
    using read_callback_t = std::function<bool(std::string, bool error)>;
    void set_read_callback(read_callback_t fnc) { read_callback= fnc; }
    bool is_sent();

private:
    MySerial(const MySerial&) = delete;
    MySerial& operator=(const MySerial&) = delete;

	asio::io_service io; ///< Io service object
    asio::serial_port port; ///< MySerial port object
    std::thread backgroundThread; ///< Thread that runs read/write operations

    std::string input_buffer;
    std::deque<std::string> queue;
    std::list<std::string> priqueue;
    mutable std::mutex queue_mutex;
    read_callback_t read_callback;
    std::deque<std::string> last_sent;

    void do_close();
    void do_read();
    void do_send();
    void handle_read(const asio::error_code& error, std::size_t n);
    void handle_write(const asio::error_code& error);
    void send();
};
