/*
 * util.hpp
 *
 *  Created on: Feb 26, 2012
 *      Author: petero
 */

#ifndef UTIL_HPP_
#define UTIL_HPP_

#include <stddef.h>
#include <string>
#include <cstdlib>
#include <sstream>
#include <vector>

typedef unsigned long U64;
typedef signed char byte;

template <typename T, size_t N> char (&_ArraySizeHelper(T(&array)[N]))[N];
#define COUNT_OF(array) (sizeof(_ArraySizeHelper(array)))


/** Helper class to perform static initialization of a class T. */
template <typename T>
class StaticInitializer {
public:
    StaticInitializer() {
        T::staticInitialize();
    }
};

/** Split a string using " " as delimiter. Append words to out. */
inline void
stringSplit(const std::string& str, std::vector<std::string>& out)
{
    std::string word;
    std::istringstream iss(str, std::istringstream::in);
    while (iss >> word)
        out.push_back(word);
}

/** Convert a string to a number. Return 0 on failure. */
template <typename T>
inline T
str2Num(const std::string& str) {
    std::stringstream ss(str);
    T result;
    return ss >> result ? result : 0;
}

/** Convert string to lower case. */
inline std::string
toLowerCase(std::string str) {
  for (int i = 0; i < (int)str.length(); i++)
    str[i] = std::tolower(str[i]);
  return str;
}

inline bool
startsWith(const std::string& str, const std::string& startsWith) {
    int N = startsWith.length();
    if ((int)str.length() < N)
        return false;
    for (int i = 0; i < N; i++)
        if (str[i] != startsWith[i])
            return false;
    return true;
}


#endif /* UTIL_HPP_ */
