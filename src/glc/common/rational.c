/**
 * \file glcs_rational.c
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

#include "rational.h"
#include <math.h>
#include <limits.h>
#include <assert.h>

#define GLCSABS(a) ((a) >= 0 ? (a) : (-(a)))
#define GLCSMAX(a,b) ((a) > (b) ? (a) : (b))
#define GLCSMIN(a,b) ((a) < (b) ? (a) : (b))

/*
 * As an interesting side note, the original code was recursive:
 *
 * int64_t av_gcd(int64_t a, int64_t b){
 *     if(b) return av_gcd(b, a%b);
 *     else  return a;
 * }
 *
 * I just checked the emitted code by gcc with
 * gcc -O2 -S -c
 *
 * and the compiler optimize out the recursivity and emits identical
 * iterative code either with the original code or the new version.
 */
static int64_t glcs_gcd(int64_t u, int64_t v)
{
	while (v) {
		int64_t tmp = v;
		v = u%v;
		u = tmp;
	}
	return u;
}

int glcs_reduce(int *dst_num, int *dst_den,
              int64_t num, int64_t den, int64_t max)
{
	glcs_Rational_t a0 = { 0, 1 }, a1 = { 1, 0 };
	int sign = (num < 0) ^ (den < 0);
	int64_t gcd = glcs_gcd(GLCSABS(num), GLCSABS(den));

	if (gcd) {
		num = GLCSABS(num) / gcd;
		den = GLCSABS(den) / gcd;
	}
	if (num <= max && den <= max) {
		a1 = (glcs_Rational_t) { num, den };
		den = 0;
	}

	while (den) {
		uint64_t x        = num / den;
		int64_t next_den  = num - den * x;
		int64_t a2n       = x * a1.num + a0.num;
		int64_t a2d       = x * a1.den + a0.den;

		if (a2n > max || a2d > max) {
			if (a1.num) x =          (max - a0.num) / a1.num;
			if (a1.den) x = GLCSMIN(x, (max - a0.den) / a1.den);

			if (den * (2 * x * a1.den + a0.den) > num * a1.den)
				a1 = (glcs_Rational_t) { x * a1.num + a0.num, x * a1.den + a0.den };
			break;
        	}

		a0  = a1;
		a1  = (glcs_Rational_t) { a2n, a2d };
		num = den;
		den = next_den;
	}
    assert(glcs_gcd(a1.num, a1.den) <= 1U);

    *dst_num = sign ? -a1.num : a1.num;
    *dst_den = a1.den;

    return den == 0;
}

glcs_Rational_t glcs_mul_q(glcs_Rational_t b, glcs_Rational_t c)
{
    glcs_reduce(&b.num, &b.den,
               b.num * (int64_t) c.num,
               b.den * (int64_t) c.den, INT_MAX);
    return b;
}

glcs_Rational_t glcs_div_q(glcs_Rational_t b, glcs_Rational_t c)
{
    return glcs_mul_q(b, (glcs_Rational_t) { c.den, c.num });
}

glcs_Rational_t glcs_d2q(double d, int max)
{
	glcs_Rational_t a;
#define LOG2  0.69314718055994530941723212145817656807550013436025
	int exponent;
	int64_t den;
	if (isnan(d))
		return (glcs_Rational_t) { 0,0 };
    if (fabs(d) > INT_MAX + 3LL)
        return (glcs_Rational_t) { d < 0 ? -1 : 1, 0 };
    exponent = GLCSMAX( (int)(log(fabs(d) + 1e-20)/LOG2), 0);
    den = 1LL << (61 - exponent);
    // (int64_t)rint() and llrint() do not work with gcc on ia64 and sparc64
    glcs_reduce(&a.num, &a.den, floor(d * den + 0.5), den, max);
    if ((!a.num || !a.den) && d && max>0 && max<INT_MAX)
        glcs_reduce(&a.num, &a.den, floor(d * den + 0.5), den, INT_MAX);

    return a;
}

