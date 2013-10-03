/*
    Texel - A UCI chess engine.
    Copyright (C) 2012-2013  Peter Österlund, peterosterlund2@gmail.com

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
 * util.hpp
 *
 *  Created on: Feb 26, 2012
 *      Author: petero
 */

#ifndef UTIL_HPP_
#define UTIL_HPP_

#include <cstddef>
#include <cstdint>
#include <string>
#include <cstdlib>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cctype>
#include <iomanip>

typedef uint64_t U64;
typedef int64_t  S64;
typedef uint32_t U32;
typedef int32_t  S32;
typedef uint16_t U16;
typedef int16_t  S16;
typedef int8_t   S8;
typedef uint8_t  U8;
typedef signed char byte;
typedef unsigned char ubyte;

template <typename T, size_t N> char (&ArraySizeHelper(T(&array)[N]))[N];
#define COUNT_OF(array) (sizeof(ArraySizeHelper(array)))

#define ACCESS_ONCE(x) (*static_cast<std::remove_reference<decltype(x)>::type volatile*>(&(x)))
#define ACCESS_ONCE_CONST(x) (*static_cast<std::remove_reference<decltype(x)>::type const volatile*>(&(x)))

template <typename T> class AlignedAllocator;
/** std::vector with cache line aware allocator. */
template <typename T>
class vector_aligned : public std::vector<T, AlignedAllocator<T> > { };


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
splitString(const std::string& str, std::vector<std::string>& out)
{
    std::string word;
    std::istringstream iss(str, std::istringstream::in);
    while (iss >> word)
        out.push_back(word);
}

/** Convert a string to a number. */
template <typename T>
bool
str2Num(const std::string& str, T& result) {
    std::stringstream ss(str);
    ss >> result;
    return !!ss;
}

template <typename T>
bool
hexStr2Num(const std::string& str, T& result) {
    std::stringstream ss;
    ss << std::hex << str;
    ss >> result;
    return !!ss;
}

template <typename T>
inline std::string
num2Str(const T& num) {
    std::stringstream ss;
    ss << num;
    return ss.str();
}

inline std::string
num2Hex(U64 num) {
    std::stringstream ss;
    ss << std::hex << std::setw(16) << std::setfill('0') << num;
    return ss.str();
}

/** Convert string to lower case. */
inline std::string
toLowerCase(std::string str) {
  for (size_t i = 0; i < str.length(); i++)
    str[i] = std::tolower(str[i]);
  return str;
}

inline bool
startsWith(const std::string& str, const std::string& startsWith) {
    size_t N = startsWith.length();
    if (str.length() < N)
        return false;
    for (size_t i = 0; i < N; i++)
        if (str[i] != startsWith[i])
            return false;
    return true;
}


/** Return true if vector v contains element e. */
template <typename T>
inline bool
contains(const std::vector<T>& v, const T& e) {
    return std::find(v.begin(), v.end(), e) != v.end();
}

/** Return true if vector v contains element e converted to a string. */
inline bool
contains(const std::vector<std::string> v, const char* e) {
    return contains(v, std::string(e));
}

inline std::string
trim(const std::string& s) {
    for (int i = 0; i < (int)s.length(); i++) {
        if (!isspace(s[i])) {
            for (int j = (int)s.length()-1; j >= i; j--)
                if (!isspace(s[j]))
                    return s.substr(i, j-i+1);
            return "";
        }
    }
    return "";
}

// ----------------------------------------------------------------------------

// A fixed size array that can compute the sum of a range of values in time O(log N)
template <int N>
class RangeSumArray {
public:

    /** Get value at index i. */
    int get(size_t i) const {
        return arr[i];
    }

    /** Add delta to value at index i. */
    void add(size_t i, int delta) {
        arr[i] += delta;
        pairs.add(i/2, delta);
    }

    /** Compute sum of all elements in [b,e). */
    int sum(size_t b, size_t e) const {
        int result = 0;
        sumHelper(b, e, result);
        return result;
    }

private:
    template <int N2> friend class RangeSumArray;

    void sumHelper(size_t b, size_t e, int& result) const {
        if (b >= e)
            return;
        if (b & 1) {
            result += arr[b];
            b++;
        }
        if (e & 1) {
            if (e != N)
                result -= arr[e];
            e++;
        }
        pairs.sumHelper(b/2, e/2, result);
    }

    std::array<int, N> arr {};
    RangeSumArray<(N+1)/2> pairs;
};

template<>
class RangeSumArray<1> {
public:
    int get(size_t i) const {
        return value;
    }

    void add(size_t i, int delta) {
        value += delta;
    }

    int sum(size_t b, size_t e) const {
        int result = 0;
        sumHelper(b, e, result);
        return result;
    }

private:
    template <int N2> friend class RangeSumArray;

    void sumHelper(size_t b, size_t e, int& result) const {
        if (b < e)
            result += value;
    }

    int value { 0 };
};

#endif /* UTIL_HPP_ */