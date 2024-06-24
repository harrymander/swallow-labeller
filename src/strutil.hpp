#ifndef RECAP_STRUTIL_HPP_INCLUDE
#define RECAP_STRUTIL_HPP_INCLUDE

#include <algorithm>
#include <cctype>
#include <locale>
#include <string>

namespace recap::strutil {

// trim from start (in place)
inline void ltrim(std::string& s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
                return !std::isspace(ch);
            }));
}

// trim from end (in place)
inline void rtrim(std::string& s)
{
    s.erase(
        std::find_if(
            s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }
        ).base(),
        s.end()
    );
}

inline void trim(std::string& s)
{
    ltrim(s);
    rtrim(s);
}

inline std::string trimmed(std::string s)
{
    trim(s);
    return s;
}

}; // namespace recap::strutil

#endif // RECAP_STRUTIL_HPP_INCLUDE
