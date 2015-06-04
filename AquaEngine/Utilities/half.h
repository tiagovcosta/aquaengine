#ifndef HALF_H
#define HALF_H

#include <stdint.h>

float half_to_float( uint16_t h );
uint16_t half_from_float( float f );
uint16_t half_add( uint16_t arg0, uint16_t arg1 );
uint16_t half_mul( uint16_t arg0, uint16_t arg1 );

#if _MSC_VER
static __inline uint16_t half_sub( uint16_t ha, uint16_t hb ) 
#else
static inline uint16_t half_sub(uint16_t ha, uint16_t hb)
#endif
{
  // (a-b) is the same as (a+(-b))
  return half_add( ha, hb ^ 0x8000 );
}
/*
#if _MSC_VER
static __inline uint16_t half_from_float(float f)
#else
static inline uint16_t half_from_float(float f)
#endif
{
	union
	{
		float f;
		uint32_t u;
	} helper;

	helper.f = f;

	return half_from_float(helper.u);
}
*/
#endif /* HALF_H */
