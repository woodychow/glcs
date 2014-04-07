/**
 * \file glcs_rational.h
 * \brief Straight from ffmpeg libavutil/rational written by Michael Niedermayer <michaelni@gmx.at>
 *  I did just prefer to cut & paste this very small subset than requiring to depend on ffmpeg.
 * \author Olivier Langlois <olivier@trillion01.com>
 * \date 2014

    Copyright 2014 Olivier Langlois

    This file is part of glcs.

    glcs is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    glcs is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with glcs.  If not, see <http://www.gnu.org/licenses/>.

 */

#ifndef __GLCS_RATIONAL_H__
#define __GLCS_RATIONAL_H__

#include <stdint.h>

/**
 * rational number numerator/denominator
 */
typedef struct glcs_Rational{
	int num; ///< numerator
	int den; ///< denominator
} glcs_Rational_t;

/**
 * Reduce a fraction.
 * This is useful for framerate calculations.
 * @param dst_num destination numerator
 * @param dst_den destination denominator
 * @param num source numerator
 * @param den source denominator
 * @param max the maximum allowed for dst_num & dst_den
 * @return 1 if exact, 0 otherwise
 */
int glcs_reduce(int *dst_num, int *dst_den, int64_t num, int64_t den, int64_t max);

/**
 * Multiply two rationals.
 * @param b first rational
 * @param c second rational
 * @return b*c
 */
glcs_Rational_t glcs_mul_q(glcs_Rational_t b, glcs_Rational_t c);

/**
 * Divide one rational by another.
 * @param b first rational
 * @param c second rational
 * @return b/c
 */
glcs_Rational_t glcs_div_q(glcs_Rational_t b, glcs_Rational_t c);

/**
 * Convert a double precision floating point number to a rational.
 * inf is expressed as {1,0} or {-1,0} depending on the sign.
 *
 * @param d double to convert
 * @param max the maximum allowed numerator and denominator
 * @return (AVRational) d
 */
glcs_Rational_t glcs_d2q(double d, int max);

#endif

