// gcode processor and dispatcher

#include "GCodeProcessor.h"
#include "GCode.h"

#include <map>
#include <vector>
#include <stdint.h>
#include <ctype.h>
#include <cmath>
#include <cstdlib>
#include <cstring>


std::tuple<uint16_t, uint16_t> GCodeProcessor::parse_code(const char *&p)
{
    int a = 0, b = 0;

    while(*p && isdigit(*p)) {
        a = (a * 10) + (*p - '0');
        ++p;
    }
    if(*p == '.') {
        ++p;
        while(*p && isdigit(*p)) {
            b = (b * 10) + (*p - '0');
            ++p;
        }
    }
    return std::make_tuple(a, b);
}
//static
GCode GCodeProcessor::group1;

GCodeProcessor::GCodeProcessor()
{
    line_no = -1;
}

GCodeProcessor::~GCodeProcessor() {}

// Parse the line containing 1 or more gcode words
bool GCodeProcessor::parse(const char *line, GCodes_t& gcodes)
{
    GCode gc;
    bool start = true;
    const char *p = line;
    const char *eos = line + strlen(line);
    int ln = 0;
    int cs = 0;
    int checksum= 0;

    // deal with line numbers and checksums before we parse
    if(*p == 'N') {
        char *pp;

        // Get linenumber
        if(p+1 != eos) {
            ln = strtol(p + 1, &pp, 10);
        }

        // get checksum
        char *csp = strchr(pp, '*');
        if(csp == nullptr || csp+1 == eos) {
            checksum = 0;
        } else {
            checksum = strtol(csp + 1, nullptr, 10);
            eos = csp; // terminate line at checksum start
        }

        // if it is M110: Set Current Line Number
        if(pp+5 >= eos && strncmp(pp + 1, "M110", 4) == 0) {
            line_no = ln;
            return true;
        }

        // Calculate checksum of string
        while(p != eos) {
            cs = cs ^ *p++;
        }
        cs &= 0x00ff;
        cs -= checksum;
        p = pp;

    } else {
        //Assume checks succeeded
        cs = 0x00;
        ln = line_no + 1;
    }

    // check the checksum
    int nextline = line_no + 1;
    if(cs == 0x00 && ln == nextline) {
        line_no = nextline;

    } else {
        // checksum failed
        gcodes.clear();
        return false;
    }

    while(p != eos) {
        if(isspace(*p)) {
            ++p;
            continue;
        }
        if(*p == ';') break; // skip rest of line for comment
        if(*p == '(') {
            // skip comments in ( ... )
            while(*p != '\0' && *p != ')') ++p;
            if(*p) ++p;
            continue;
        }

        // Words as per NIST are always an upper case letter followed by a signed or unsigned floating point number
        // [A-Z][-+]*[/d.]+
        char c = toupper(*p++);
        if(c < 'A' || c > 'Z') {
            // This is an error
            gc.set_error("Illegal word");
            gcodes.push_back(gc);
            return false;
        }

        // see if we have another G or M code on the same line
        if((c == 'G' || c == 'M') && !start) {
            gcodes.push_back(gc);
            gc.clear();
            start = true;
        }

        if(start) {
            start = false;
            if(c == 'G' || c == 'M' || c == 'T'){
                // it is a command word
                if(!isdigit(*p)) {
                    // this is an error
                    gc.set_error("Illegal command word");
                    gcodes.push_back(gc);
                    return false;
                }
                // extract gcode command word G01{.123}
                std::tuple<uint16_t, uint16_t> code = parse_code(p);

                if(c == 'G' || c == 'M') {
                    gc.set_command(c, std::get<0>(code), std::get<1>(code));
                    if(c == 'G' && std::get<0>(code) <= 3) {
                        group1.clear();
                        group1.set_command(c, std::get<0>(code), std::get<1>(code));
                    }

                } else if(c == 'T') {
                    // tool change but for 3dprinters this is really just select an extruder/heater to use
                    // but we force mcode to be M6 which is change tool and set the T parameter
                    // TODO tool change will break when/if real tool change is added and will need to be re thought
                    gc.set_t();
                    gc.set_command('M', 6, 0);
                    gc.add_arg('T', std::get<0>(code));
                }

                continue;

            } else {
                // parameter word with no command word so use modal command word
                // group1, copies G code and subcode for this line
                gc.set_command('G', group1.get_code(), group1.get_subcode());
                // fall through to process parameter word
            }
        }

        // process the parameter word
        if(!isdigit(*p) && *p != '-' && *p != '.') {
            // this is an error
            gc.set_error("Illegal parameter word");
            gcodes.push_back(gc);
            return false;
        }
        // parse argument word (X-1.23)
        char *np;
        float f = strtof(p, &np);
        gc.add_arg(c, f);
        p= np;
    }

    gcodes.push_back(gc);
    return true;
}
