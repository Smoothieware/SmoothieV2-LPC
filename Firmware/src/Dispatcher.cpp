#include "Dispatcher.h"
#include "GCode.h"
#include "GCodeProcessor.h"
#include "OutputStream.h"
#include "Module.h"
#include "StringUtils.h"

#include <ctype.h>
#include <cmath>
#include <string.h>
#include <cstdarg>

using namespace std;

//#define DEBUG_WARNING printf
#define DEBUG_WARNING(...)

Dispatcher *Dispatcher::instance= nullptr;

Dispatcher::Dispatcher()
{
    if(instance == nullptr) instance= this;
}

std::set<std::string> Dispatcher::get_commands() const
{
	std::set<std::string> s;
	for(auto& it : command_handlers) {
		s.insert(it.first);
	}
	return s;
}

// goes in Flash, list of Mxxx codes that are allowed when in Halted state
static const int allowed_mcodes[]= {2,5,9,30,105,114,119,80,81,911,503,106,107}; // get temp, get pos, get endstops etc
static bool is_allowed_mcode(int m) {
    for (size_t i = 0; i < sizeof(allowed_mcodes)/sizeof(int); ++i) {
        if(allowed_mcodes[i] == m) return true;
    }
    return false;
}

// Must be called from the command thread context
bool Dispatcher::dispatch(GCode& gc, OutputStream& os, bool need_ok) const
{
	// if(gc.has_m() && gc.get_code() == 503) {
	// 	// alias M503 to M500.3
	// 	gc.set_command('M', 500, 3);
	// }

	if(Module::is_halted()) {
		// If we are halted then we reject most g/m codes unless in exception list
		if(gc.has_m() && gc.get_code() == 999) {
			Module::broadcast_halt(false);
			os.printf("WARNING: After HALT you should HOME as position is currently unknown\nok\n");
			return true;
		}

		// we return an error if it is not an allowed m code
		if(!gc.has_m() || !is_allowed_mcode(gc.get_code())){
			if(grbl_mode) {
				os.printf("error:Alarm lock\n");
			} else {
				os.printf("!!\n");
			}
			return true;
		}
	}

	auto& handler = gc.has_g() ? gcode_handlers : mcode_handlers;
	const auto& f = handler.equal_range(gc.get_code());
	bool ret = false;

	for (auto it = f.first; it != f.second; ++it) {
		if(it->second(gc, os)) {
			ret = true;
		} else {
			// not really useful as many handlers will only process if certain params are set, so not an error unless no handler deals with it.
			DEBUG_WARNING("handler did not handle %c%d\n", gc.has_g() ? 'G' : 'M', gc.get_code());
		}
	}

	// special case is M500 - M503
	// if(gc.has_m() && gc.get_code() >= 500 && gc.get_code() <= 503) {
	// 	ret = handle_configuration_commands(gc, os);
	// }

	if(ret) {
		bool send_ok = true;

		if (gc.has_error()) {
			// report error
			if(grbl_mode) {
				os.printf("error: ");
			} else {
				os.printf("Error: ");
			}

			const char *errmsg = gc.get_error_message();
			if(errmsg != nullptr) {
				os.printf("%s\n", errmsg);
			} else {
				os.printf("unknown\n");
			}

			// we cannot continue safely after an error so we enter HALT state
			os.printf("Entering Alarm/Halt state\n");
			Module::broadcast_halt(true);
			return true;
		}


		if(os.is_prepend_ok()) {
			// output the result after the ok
			os.set_prepend_ok(false);
			os.printf("ok ");
			os.flush_prepend(); // this flushes the internally stored prepend string to the output
			send_ok = false;
		}

		if(os.is_append_nl()) {
			// append newline
			os.set_append_nl(false); // clear the flag
			os.printf("\n");
		}

		if(send_ok && need_ok) {
			os.printf("ok\n");
		}

		return true;
	}

	return false;
}

// convenience to dispatch a one off command
// Usage: dispatch(os, 'M', 123, [subcode,] 'X', 456.0F, 'Y', 789.0F, ..., 0); // must terminate with 0
// dispatch(os, 'M', 123, 0);
// NOTE parameters must be floats
// Must be called from the command thread context
bool Dispatcher::dispatch(OutputStream& os, char cmd, uint16_t code, ...) const
{
	GCode gc;
	va_list args;
	va_start(args, code);
	char c = va_arg(args, int); // get first arg
	if(c > 0 && c < 'A') { // infer subcommand
		gc.set_command(cmd, code, (uint16_t)c);
		c = va_arg(args, int); // get next arg
	} else {
		gc.set_command(cmd, code);
	}

	while(c != 0) {
		float v = (float)va_arg(args, double);
		gc.add_arg(c, v);
		c = va_arg(args, int); // get next arg
	}

	va_end(args);
	return dispatch(gc, os);
}

// dispatch command to a command handler if one is registered
bool Dispatcher::dispatch(const char *line, OutputStream& os) const
{
	std::string params(line);
	std::string cmd = stringutils::get_command_arguments(params);
	const auto& f = command_handlers.equal_range(cmd);
	bool ret = false;

	for (auto it = f.first; it != f.second; ++it) {
		if(it->second(params, os)) {
			ret = true;
		} else {
			DEBUG_WARNING("command handler did not handle %s\n", line);
		}
	}

	return ret;
}

Dispatcher::Handlers_t::iterator Dispatcher::add_handler(HANDLER_NAME gcode, uint16_t code, Handler_t fnc)
{
	Handlers_t::iterator ret;
	switch(gcode) {
		case GCODE_HANDLER: ret = gcode_handlers.insert( Handlers_t::value_type(code, fnc) ); break;
		case MCODE_HANDLER: ret = mcode_handlers.insert( Handlers_t::value_type(code, fnc) ); break;
	}
	return ret;
}

void Dispatcher::remove_handler(HANDLER_NAME gcode, Handlers_t::iterator i)
{
	switch(gcode) {
		case GCODE_HANDLER: gcode_handlers.erase(i); break;
		case MCODE_HANDLER: mcode_handlers.erase(i); break;
	}
}

Dispatcher::CommandHandlers_t::iterator Dispatcher::add_handler(std::string cmd, CommandHandler_t fnc)
{
	return command_handlers.insert( CommandHandlers_t::value_type(cmd, fnc) );
}

// mainly used for testing
void Dispatcher::clear_handlers()
{
	gcode_handlers.clear();
	mcode_handlers.clear();
	command_handlers.clear();
}

bool Dispatcher::handle_configuration_commands(GCode& gc, OutputStream& os) const
{
	// if(gc.get_code() == 500) {
	// 	if(gc.getSubcode() == 3) {
	// 		if(loaded_configuration)
	// 			gc.getOS().printf("// Saved configuration is active\n");
	// 		// just print it
	// 		return true;

	// 	}else if(gc.getSubcode() == 0){
	// 		return writeConfiguration(gc.getOS());
	// 	}

	// }else if(gc.get_code() == 501) {
	// 	return loadConfiguration(gc.getOS());

	// }else if(gc.get_code() == 502) {
	// 	// delete the saved configuration
	// 	uint32_t zero= 0xFFFFFFFFUL;
	// 	if(THEKERNEL.nonVolatileWrite(&zero, 4, 0) == 4) {
	// 		gc.getOS().printf("// Saved configuration deleted - reset to restore defaults\n");
	// 	}else{
	// 		gc.getOS().printf("// Failed to delete saved configuration\n");
	// 	}
	// 	return true;
	// }

	return false;
}

bool Dispatcher::write_configuration(OutputStream& output_stream) const
{
	// write stream to non volatile memory
	// prepend magic number so we know there is a valid configuration saved
	// std::string str= "CONF";
	// str.append(output_stream.str());
	// str.append(4, 0); // terminate with 4 nuls
	// output_stream.clear(); // clear to save some memory
	// size_t n= str.size();
	// size_t r= THEKERNEL.nonVolatileWrite((void *)str.data(), n, 0);
	// if(r == n) {
	// 	output_stream.printf("// Configuration saved\n");
	// }else{
	// 	output_stream.printf("// Failed to save configuration, needed to write %d, wrote %d\n", n, r);
	// }
	return true;
}

bool Dispatcher::load_configuration() const
{
	// OutputStream os;
	// return load_configuration(os);
	return false;
}

bool Dispatcher::load_configuration(OutputStream& output_stream) const
{
	// load configuration from non volatile memory
	// char buf[64];
	// size_t r= THEKERNEL.nonVolatileRead(buf, 4, 0);
	// if(r == 4) {
	// 	if(strncmp(buf, "CONF", 4) == 0) {
	// 		size_t off= 4;
	// 		// read everything until we get some nulls
	// 		std::string str;
	// 		do {
	// 			r= THEKERNEL.nonVolatileRead(buf, sizeof(buf), off);
	// 			if(r != sizeof(buf)) {
	// 				output_stream.printf("// Read failed\n");
	// 				return true;
	// 			}
	// 			str.append(buf, r);
	// 			off += r;
	// 		}while(str.find('\0') == string::npos);

	// 		// foreach line dispatch it
	// 		std::stringstream ss(str);
	// 		std::string line;
	// 		std::vector<string> lines;
	// 		GCodeProcessor& gp= THEKERNEL.getGCodeProcessor();
	// 	    while(std::getline(ss, line, '\n')){
//  				if(line.find('\0') != string::npos) break; // hit the end
//  				lines.push_back(line);
//  				// Parse the Gcode
	// 			GCodeProcessor::GCodes_t gcodes;
	// 			gp.parse(line.c_str(), gcodes);
	// 			// dispatch it
	// 			for(auto& i : gcodes) {
	// 				if(i.get_code() >= 500 && i.get_code() <= 503) continue; // avoid recursion death
	// 				dispatch(i);
	// 			}
	// 		}
	// 		for(auto& s : lines) {
	// 			output_stream.printf("// Loaded %s\n", s.c_str());
	// 		}
	// 		loaded_configuration= true;

	// 	}else{
	// 		output_stream.printf("// No saved configuration\n");
	// 		loaded_configuration= false;
	// 	}

	// }else{
	// 	output_stream.printf("// Failed to read saved configuration\n");
	// }
	return true;
}
