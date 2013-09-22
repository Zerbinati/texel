/*
    Texel - A UCI chess engine.
    Copyright (C) 2013  Peter Österlund, peterosterlund2@gmail.com

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
 * utilTest.cpp
 *
 *  Created on: Sep 21, 2013
 *      Author: petero
 */

#include "utilTest.hpp"
#include "util.hpp"
#include "timeUtil.hpp"

#include <iostream>

#include "cute.h"

void
UtilTest::testUtil() {
    int arr1[10];
    ASSERT_EQUAL(10, COUNT_OF(arr1));

    std::vector<std::string> splitResult;
    splitString(std::string("a b c def"), splitResult);
    auto v = std::vector<std::string>{"a", "b", "c", "def"};
    ASSERT_EQUAL((std::vector<std::string>{"a", "b", "c", "def"}), splitResult);

    U64 val;
    ASSERT(str2Num("123456789012345", val));
    ASSERT_EQUAL(123456789012345ULL, val);
    ASSERT(hexStr2Num("1f2c", val));
    ASSERT_EQUAL(0x1f2c, val);
    ASSERT_EQUAL("12345", num2Str(12345));
    ASSERT_EQUAL("000000001234fdec", num2Hex(0x1234fdec));

    ASSERT_EQUAL("peter", toLowerCase("Peter"));
    ASSERT(startsWith("Peter", "Pe"));
    ASSERT(startsWith("Peter", "Peter"));
    ASSERT(!startsWith("Peter", "PeterO"));
    ASSERT(!startsWith("Peter", "Pex"));
    ASSERT(!startsWith("Peter", "eter"));

    ASSERT(contains((std::vector<int>{1,2,3,4}), 3));
    ASSERT(!contains((std::vector<int>{1,2,3,4}), 5));
    ASSERT(contains((std::vector<int>{1,3,2,3,4}), 3));
    ASSERT(!contains((std::vector<int>{}), 0));
    ASSERT(!contains((std::vector<int>{}), 1));

    ASSERT_EQUAL(std::string("asdf  adf"), trim(std::string(" asdf  adf  ")));
    ASSERT_EQUAL(std::string("asdf xyz"), trim(std::string("\t asdf xyz")));
}

void
UtilTest::testSampleStat() {
    const double tol = 1e-14;

    SampleStatistics stat;
    ASSERT_EQUAL(0, stat.numSamples());
    ASSERT_EQUAL_DELTA(0, stat.avg(), 1e-14);
    ASSERT_EQUAL_DELTA(0, stat.std(), 1e-14);

    stat.addSample(3);
    ASSERT_EQUAL(1, stat.numSamples());
    ASSERT_EQUAL_DELTA(3, stat.avg(), tol);
    ASSERT_EQUAL_DELTA(0, stat.std(), tol);

    stat.addSample(4);
    ASSERT_EQUAL(2, stat.numSamples());
    ASSERT_EQUAL_DELTA(3.5, stat.avg(), tol);
    ASSERT_EQUAL_DELTA(::sqrt(0.5), stat.std(), tol);

    stat.reset();
    ASSERT_EQUAL(0, stat.numSamples());
    ASSERT_EQUAL_DELTA(0, stat.avg(), 1e-14);
    ASSERT_EQUAL_DELTA(0, stat.std(), 1e-14);

    for (int i = 0; i < 10; i++)
        stat.addSample(i);
    ASSERT_EQUAL(10, stat.numSamples());
    ASSERT_EQUAL_DELTA(4.5, stat.avg(), 1e-14);
    ASSERT_EQUAL_DELTA(::sqrt(55.0/6.0), stat.std(), 1e-14);
}

void
UtilTest::testTime() {
    TimeSampleStatistics stat1;
    TimeSampleStatistics stat2;
    {
        ASSERT_EQUAL(0, stat1.numSamples());
        ASSERT_EQUAL(0, stat2.numSamples());
        ScopedTimeSample ts(stat1);
        ASSERT_EQUAL(0, stat1.numSamples());
        ASSERT_EQUAL(0, stat2.numSamples());
        ScopedTimeSample ts2(stat2);
        ASSERT_EQUAL(0, stat1.numSamples());
        ASSERT_EQUAL(0, stat2.numSamples());
    }
    ASSERT_EQUAL(1, stat1.numSamples());
    ASSERT_EQUAL(1, stat2.numSamples());
    {
        ScopedTimeSample ts(stat1);
        ASSERT_EQUAL(1, stat1.numSamples());
        ASSERT_EQUAL(1, stat2.numSamples());
    }
    ASSERT_EQUAL(2, stat1.numSamples());
    ASSERT_EQUAL(1, stat2.numSamples());
}

void
UtilTest::testRangeSumArray() {
    const int N = 15;
    RangeSumArray<N> arr;
    for (int i = 0; i < N; i++)
        ASSERT_EQUAL(0, arr.get(i));
    for (int i = 0; i < N; i++)
        arr.add(i, 1<<i);
    for (int i = 0; i < N; i++)
        ASSERT_EQUAL(1<<i, arr.get(i));
    for (int i = 0; i <= N; i++) {
        for (int j = i; j <= N; j++) {
            int expected = 0;
            for (int k = i; k < j; k++)
                expected += arr.get(k);
            int sum = arr.sum(i, j);
            ASSERT_EQUAL(expected, sum);
        }
    }
}

cute::suite
UtilTest::getSuite() const {
    cute::suite s;
    s.push_back(CUTE(testUtil));
    s.push_back(CUTE(testSampleStat));
    s.push_back(CUTE(testTime));
    s.push_back(CUTE(testRangeSumArray));
    return s;
}
