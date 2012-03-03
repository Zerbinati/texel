/*
 * util.cpp
 *
 *  Created on: Mar 2, 2012
 *      Author: petero
 */

#include "util.hpp"

#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>

U64 currentTimeMillis() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}
