#include "StringUtils.h"

#include <cctype>
#include <algorithm>
#include <cstring>

// split string into array on separator
std::vector<std::string> stringutils::split(const char *str, const char *sep)
{
    const char *p= str;
    const char *fp;
    std::string s;
    std::vector<std::string> arr;

    if(str == NULL || strlen(str) == 0 || sep == NULL || strlen(sep) == 0)
        return arr;

    while(*p != '\0'){
        fp= strpbrk(p, sep);
        if(fp){
            s.assign(p, fp-p);
            p= fp+1;
            arr.push_back(s);
        }else{
            s.assign(p);
            arr.push_back(s);
            break;
        }
    }

    return arr;
}

// ditto but takes character for sep
std::vector<std::string> stringutils::split(const char *str, char sep)
{
    const char s[2]{sep, '\0'};
    return stringutils::split(str, s);
}

// Get the first parameter, and remove it from the original string
// if a quoted parameter extract it as one parameter including spaces but excluding quotes
std::string stringutils::shift_parameter( std::string &parameters )
{
    std::string ret;
    size_t pos = parameters.find_first_of(" \"");
    if(pos == std::string::npos) {
        ret= parameters;
        parameters= "";

    } else if(parameters[pos] == '\"') {
        // if it is quoted then return up to the closing quote
        size_t o= parameters.find_first_of("\"", pos+1);
        if( o != std::string::npos ) {
            ret= parameters.substr(pos+1, o-1); // remove leading and trailing "

            size_t n= parameters.find_first_of(" ", o);
            if(n != std::string::npos) {
                parameters= parameters.substr(n+1, parameters.size());
            }else{
                parameters= parameters.substr(o+1, parameters.size());
            }
        }

    } else {
        ret = parameters.substr( 0, pos );
        parameters = parameters.substr(pos+1, parameters.size());
    }
    return ret;
}


// parse a number list "1.1,2.2,3.3" and return the numbers in a std::vector of floats
std::vector<float> stringutils::parse_number_list(const char *str)
{
    std::vector<std::string> l= stringutils::split(str, ',');
    std::vector<float> r;
    for(auto& s : l){
        float x = strtof(s.c_str(), nullptr);
        r.push_back(x);
    }
    return r;
}

std::vector<uint32_t> stringutils::parse_number_list(const char *str, int radix)
{
    std::vector<std::string> l= stringutils::split(str, ',');
    std::vector<uint32_t> r;
    for(auto& s : l){
        uint32_t x = strtoul(s.c_str(), nullptr, radix);
        r.push_back(x);
    }
    return r;
}

// convert the wcs to the string Gcode version
std::string stringutils::wcs2gcode(int wcs) {
    std::string str= "G5";
    str.append(1, std::min(wcs, 5) + '4');
    if(wcs >= 6) {
        str.append(".").append(1, '1' + (wcs - 6));
    }
    return str;
}

std::string stringutils::toUpper(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), ::toupper);
    return str;
}

std::string stringutils::trim(const std::string &s)
{
    auto wsfront = std::find_if_not(s.begin(), s.end(), [](int c) {return std::isspace(c);});
    auto wsback = std::find_if_not(s.rbegin(), s.rend(), [](int c) {return std::isspace(c);}).base();
    return (wsback <= wsfront ? std::string() : std::string(wsfront, wsback));
}

// Separate command from arguments
// return command and strip it from line
std::string stringutils::get_command_arguments(std::string& line )
{
    std::string t = line;
    size_t pos = line.find_first_of(" ");
    if( pos == std::string::npos ) {
        line = "";
        return t;
    }

    line = line.substr( pos + 1);
    return t.substr(0, pos);
}
