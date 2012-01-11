#ifndef _OSMOCOM_CODEC_H
#define _OSMOCOM_CODEC_H

#include <stdint.h>

extern uint16_t gsm610_bitorder[];	/* FR */
extern uint16_t gsm620_unvoiced_bitorder[]; /* HR unvoiced */
extern uint16_t gsm620_voiced_bitorder[];   /* HR voiced */
extern uint16_t gsm660_bitorder[];	/* EFR */

extern uint16_t gsm690_12_2_bitorder[];	/* AMR 12.2  kbits */
extern uint16_t gsm690_10_2_bitorder[];	/* AMR 10.2  kbits */
extern uint16_t gsm690_7_95_bitorder[];	/* AMR  7.95 kbits */
extern uint16_t gsm690_7_4_bitorder[];	/* AMR  7.4  kbits */
extern uint16_t gsm690_6_7_bitorder[];	/* AMR  6.7  kbits */
extern uint16_t gsm690_5_9_bitorder[];	/* AMR  5.9  kbits */
extern uint16_t gsm690_5_15_bitorder[];	/* AMR  5.15 kbits */
extern uint16_t gsm690_4_75_bitorder[];	/* AMR  4.75 kbits */

#endif /* _OSMOCOM_CODEC_H */
