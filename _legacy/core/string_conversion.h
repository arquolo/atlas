#pragma once

#define EXPORT

#include <string>
#include <sstream>
#include <typeinfo>
#include <iomanip>
#include <vector>
// #include "boost/lexical_cast.hpp"

namespace core {

//Checks that the given string is a valid representation
// of a numerical type T
template<typename T>
bool is_valid(const std::string& num) {
    try {
        T tmp = boost::lexical_cast<T>(num);
        return true;
    } catch (boost::bad_lexical_cast&) {
        return false;
    }
}

// Converts a string to type T.
template <typename T>
inline T fromstring(const std::string s) {
    T result = 0;
    std::stringstream str;
    str << s;
    str >> result;
    return result;
}

//////////
// Specialization for converting a string to a string.
template <> inline std::string fromstring<std::string>(const std::string s) {
    return s;
}

/////////////////////
// Splits s into pieces, separated with split.
void EXPORT split(
    const std::string &s,
    std::vector<std::string> &vs,
    const std::string &split);

//////////
// Converts a string to a vector of type T, separated at sep
template <typename T> inline
std::vector<T> fromstring(const std::string& s, const std::string &sep) {
    std::vector<T> result;

    //Split string
    std::vector<std::string> vs;
    split(s, vs, sep);

    size_t size = vs.size();
    result.resize(size);

    //Convert
    for (size_t i = 0; i < size; ++i) {
        result[i] = fromstring<T>(vs[i]);
    }
    return result;
}


//////////
// Converts t to a string. There is no limitation on the length of the string
// representation of t.
  template <typename T>
  inline std::string tostring(const T& t)
  {
    // There should come a possibility to set a format specifier by setting fields
    // in ios_base from which stringstream inherits.
    std::string s;
    std::stringstream str;
    str << t;
    str >> s;
    return s;
  }

//////////
// Converts vector<t> to a string, elements are separated by sep
  template <typename T>
  inline std::string tostring(const std::vector<T>& t, std::string sep)
  {
    // There should come a possibility to set a format specifier by setting fields
    // in ios_base from which stringstream inherits.
    std::string s = "";
    for(size_t i = 0; i < t.size(); i++)
      s += tostring(t[i]) + sep;

    s = s.substr(0, s.size() - 1); //remove last separator
    return s;
  }


//////////
// Converts t to a string. Sets number of digits after the decimal point.
template <typename T>
inline std::string tostring(const T& t, int prec) {
    // There should come a possibility to set a format specifier by setting fields
    // in ios_base from which stringstream inherits.
    std::string s;
    std::stringstream str;
    str.precision(prec);
    str << std::fixed << t;
    str >> s;
    return s;
}

//////////
// Specialization for converting a string to a string.
template <> inline std::string tostring<std::string>(const std::string& s) {
    return s;
}

//////////
// Specialization for char*. Otherwise the string is cut off at spaces
// in the const char.
inline std::string tostring(char *c) {
    return std::string {c};
}
inline std::string tostring(char* const &c) {
    return std::string {c};
}

// Specialization for const char*. Otherwise the string is cut off at spaces
// in the const char.
inline std::string tostring(const char* &c) {
    return std::string {c};
}

// Specialization for const char*. Otherwise the string is cut off at spaces
// in the const char.
inline std::string tostring(const char* const &c) {
    return std::string {c};
}

// Converts an integer type to a string in hexidecimal notation.
// Width can be set but defaults to 6.
template <typename T> inline
std::string inttohex(T i, int width = 6) {
    std::string s;
    std::stringstream str;
    str << std::hex << std::showbase << std::internal << std::setfill('0') << std::setw(width) << i;
    str >> s;

    return s;
}

void EXPORT lower(std::string &s);

void EXPORT upper(std::string &s);

void EXPORT trim(std::string &s);

// Replace all occurrences of olds with news in s. Returns true iff
// any replacements have been made.
bool EXPORT replace_all(
    std::string& s,
    const std::string& olds,
    const std::string& news
);

//escape
void EXPORT escape(std::string& str, char toEscape);
void EXPORT unescape(std::string& str, char toUnEscape);

}
