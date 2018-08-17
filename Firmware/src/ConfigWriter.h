#pragma once

#include <string>
#include <map>
#include <set>
#include <istream>
#include <ostream>

class ConfigWriter
{
public:
    ConfigWriter(std::istream& i, std::ostream& o) : is(i), os(o) {};
    ~ConfigWriter(){};

    // either replace the key/value in the given section or add it
    bool write(const char *section, const char* key, const char *value);

private:
    std::istream& is;
    std::ostream& os;
};
