#include "serial.h"

#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <condition_variable>
#include <atomic>
#include <chrono>             // std::chrono::seconds
#include <queue>
#include <algorithm>

#include "md5.h"

std::mutex mutex_;
std::condition_variable condVar;
volatile uint okcnt = 0;
size_t lines_sent= 0;

bool wait_for_ok()
{
	return (okcnt >= 1);
}

bool wait_for_all_oks()
{
    return (okcnt >= lines_sent);
}

void inc_ok()
{
    {
        std::lock_guard<std::mutex> lck(mutex_);
        ++okcnt;
    }
    condVar.notify_one();
}

static char last_char = '0';
char seq_char()
{
    char c= last_char;
    ++last_char;
    if(last_char < 'A' && last_char > '9') last_char= 'A';
    else if(last_char > 'Z') last_char= '0';
    return c;
}

size_t stream_data(MySerial& serial, size_t& lines)
{
    const size_t buflen=26;
	size_t cnt_sent= 0, lcnt= 0;
    MD5 md5;

    while(cnt_sent < 10000*buflen) {
        std::string l(std::to_string(++lcnt));
        l.append(":");
        size_t n= buflen-l.size()-1;
        std::string s(n, '\0');
        std::generate_n (begin(s), n, seq_char);
        s.append("\n");
        l.append(s);
        serial.send(l);
        cnt_sent += l.size();
        md5.update(l.c_str(), l.size());
    }
    // wait for all to be sent
    while(!serial.is_sent()) ;

    printf("md5: %s, cnt: %lu, lines: %lu, length: %lu\n", md5.finalize().hexdigest().c_str(), cnt_sent, lcnt, buflen);
    lines= lcnt;
    return cnt_sent;
}

bool data_read(std::string msg, bool error)
{
	if(error) {
		std::cout << "received error: " << msg << "\n";
		return false;
	}else{
		if(msg == "ok") {
			inc_ok();
		}else{
		  std::cout << "received: " << msg << "\n";
        }
	}
	return true;
}

#define M28
int main(int argc, char const *argv[])
{
	const char *dev;
	if(argc >= 2) {
		dev= argv[1];
	}else{
		dev= "/dev/ttyACM0";
	}

	MySerial serial;
    try {
    	if(!serial.open(dev, 115200)){
    		return 1;
    	}
    	serial.set_read_callback(data_read);
        okcnt= 0;
        serial.send("\n");

        {
            std::unique_lock<std::mutex> lck(mutex_);
            condVar.wait(lck, wait_for_ok);
        }

        okcnt= 0;
        #ifndef M28
        serial.send("rxtest\n");
        #else
        serial.send("M28 test\n");
        #endif
        {
            std::unique_lock<std::mutex> lck(mutex_);
            condVar.wait(lck, wait_for_ok);
        }

     	std::cout << "streaming..." << "\n";

        okcnt= 0;
        auto start = std::chrono::steady_clock::now();
        size_t cnt= stream_data(serial, lines_sent);
        if(cnt) {
    		printf("stream done\n");
    	}else{
    		printf("stream failed\n");
    	}
        auto end = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed_seconds = end-start;
        std::cout << "elapsed time: " << elapsed_seconds.count() << " s\n";
        std::cout << "rate: " << cnt/elapsed_seconds.count() << " bytes/s, ";
        std::cout << lines_sent/elapsed_seconds.count() << " lines/s\n";
        std::cout << "lines: " << lines_sent << ", oks: " << okcnt << "\n";

        #ifdef M28
        // wait for oks to finish
        {
            std::unique_lock<std::mutex> lck(mutex_);
            condVar.wait(lck, wait_for_all_oks);
        }
        #endif

    	// terminate upload sequence
        okcnt= 0;
        #ifndef M28
        std::string eod("\004");
        #else
        std::string eod("M29\n");
        #endif
     	serial.send(eod);

        {
            std::unique_lock<std::mutex> lck(mutex_);
            condVar.wait(lck, wait_for_ok);
        }

        std::cout << "closing serial\n";
    	serial.close();

  	} catch(std::exception& e) {
        std::cout<<"Error (main): " << e.what() << std::endl;
        return 1;
    }
	return 0;
}
