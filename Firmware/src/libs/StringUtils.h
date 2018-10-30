#include <string>
#include <vector>

namespace stringutils {
    std::vector<std::string> split(const char *str, const char *sep);
    std::vector<std::string> split(const char *str, char sep);
    std::string shift_parameter( std::string &parameters );
    std::vector<float> parse_number_list(const char *str);
    std::vector<uint32_t> parse_number_list(const char *str, int radix);
    std::string wcs2gcode(int wcs);
    std::string toUpper(std::string str);
    std::string trim(const std::string &s);
    std::string get_command_arguments(std::string& line);
}
