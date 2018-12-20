#ifndef FIXEDPOINT_INCLUDED
#define FIXEDPOINT_INCLUDED

#include<stdint.h>
/**
 * new data type named fixed point.
 */
typedef int32_t fixed_point;
/**
 * new data type named fixed point extend.
 */
typedef int64_t fixed_point_e;

// representation of fixed point.
#define Fixed_Point_FBits 14

#define FIXED_CONST		((fixed_point)(((fixed_point)1) << Fixed_Point_FBits))

/**
 * multiplies two fixed point numbers.
 */
#define fixed_point_mul(x,y)		((fixed_point)(( ((fixed_point_e)x)  * ((fixed_point_e) y) ) >> Fixed_Point_FBits))
/**
 * divides two fixed point numbers.
 */
#define fixed_point_div(x, y)		((fixed_point)(( ((fixed_point_e)x) << Fixed_Point_FBits ) / ((fixed_point_e)y)) )
/**
 * it converts from integer to fixed point and return a fixed point value.
 */
#define integer_to_fixed_point(x)	((fixed_point)(x << Fixed_Point_FBits))
/**
 * it converts from fixed point to integer and return integer value.
 */
#define fixed_point_to_integer(x)	((int)(x >> Fixed_Point_FBits))
/**
 * rounds fixed point number to nearest integer.
 *
 */
#define round_fixed_point_number_to_integer(x) ((int)((x >= 0)?\
								((x + (FIXED_CONST / 2)) / FIXED_CONST):\
								((x - (FIXED_CONST / 2)) / FIXED_CONST)))

#endif // FIXEDPOINT_INCLUDED
