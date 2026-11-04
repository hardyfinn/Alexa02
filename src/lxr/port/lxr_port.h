/* Alexa02 port: portable versions of the ARM-specific bits the LXR engine uses. */
#ifndef ALEXA_LXR_PORT_H
#define ALEXA_LXR_PORT_H
#include <stdint.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/* memory placement attributes: meaningless on a computer */
#define INDTCM
#define INDTCMZ
#define IN_ITCM_RAM
#define PROGMEM
#define PSTR(x) (x)
#define pgm_read_byte(p) (*(const uint8_t*)(p))
#define pgm_read_word(p) (*(const uint16_t*)(p))

/* half precision floats (used for the FX delay memory) */
#if !(defined(__aarch64__) || defined(__arm__))
#define __fp16 _Float16
#endif

/* saturating 16 bit maths (ARM DSP instructions) */
static inline int32_t lxr_ssat16(int32_t x) { return x > 32767 ? 32767 : (x < -32768 ? -32768 : x); }
#define __SSAT(x, bits) lxr_ssat_bits((int32_t)(x), (bits))
static inline int32_t lxr_ssat_bits(int32_t x, int bits) {
	const int32_t mx = (1 << (bits - 1)) - 1, mn = -(1 << (bits - 1));
	return x > mx ? mx : (x < mn ? mn : x);
}
static inline uint32_t __QADD16(uint32_t a, uint32_t b) {
	int32_t lo = lxr_ssat16((int32_t)(int16_t)(a & 0xffff) + (int32_t)(int16_t)(b & 0xffff));
	int32_t hi = lxr_ssat16((int32_t)(int16_t)(a >> 16) + (int32_t)(int16_t)(b >> 16));
	return ((uint32_t)(uint16_t)lo) | ((uint32_t)(uint16_t)hi << 16);
}
static inline uint32_t __QSUB16(uint32_t a, uint32_t b) {
	int32_t lo = lxr_ssat16((int32_t)(int16_t)(a & 0xffff) - (int32_t)(int16_t)(b & 0xffff));
	int32_t hi = lxr_ssat16((int32_t)(int16_t)(a >> 16) - (int32_t)(int16_t)(b >> 16));
	return ((uint32_t)(uint16_t)lo) | ((uint32_t)(uint16_t)hi << 16);
}
static inline uint32_t __CLZ(uint32_t x) { return x ? (uint32_t)__builtin_clz(x) : 32u; }
static inline int32_t __QADD(int32_t a, int32_t b) {
	int64_t s = (int64_t)a + b;
	return s > INT32_MAX ? INT32_MAX : (s < INT32_MIN ? INT32_MIN : (int32_t)s);
}
/* pack bottom half of a with b shifted left */
#define __PKHBT(a, b, sh) ((((uint32_t)(int32_t)(a)) & 0x0000FFFFu) | ((((uint32_t)(int32_t)(b)) << (sh)) & 0xFFFF0000u))
#define __USAT(x, bits) lxr_usat_bits((int32_t)(x), (bits))
static inline int32_t lxr_usat_bits(int32_t x, int bits) {
	const int32_t mx = (1 << bits) - 1;
	return x > mx ? mx : (x < 0 ? 0 : x);
}

#ifdef __cplusplus
}
#endif
#endif
