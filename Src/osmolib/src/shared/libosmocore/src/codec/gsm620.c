/* GSM 06.20 - GSM HR codec */

/*
 * (C) 2010 Sylvain Munaut <tnt@246tNt.com>
 *
 * All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include <stdint.h>

/* GSM HR unvoiced (mode=0) frames - subjective importance bit ordering */
	/* This array encode mapping between GSM 05.03 Table 3a (bits
	 * ordering before channel coding on TCH) and GSM 06.20 Table B.1
	 * (bit ordering on A-bis */
uint16_t gsm620_unvoiced_bitorder[112] = {
	3,	/* R0:1 */
	25,	/* LPC 3:7 */
	52,	/* GSP 0-1:2 */
	71,	/* GSP 0-2:2 */
	90,	/* GSP 0-3:2 */
	109,	/* GSP 0-4:2 */
	15,	/* LPC 1:0 */
	23,	/* LPC 2:1 */
	22,	/* LPC 2:2 */
	21,	/* LPC 2:3 */
	20,	/* LPC 2:4 */
	19,	/* LPC 2:5 */
	31,	/* LPC 3:1 */
	30,	/* LPC 3:2 */
	29,	/* LPC 3:3 */
	28,	/* LPC 3:4 */
	27,	/* LPC 3:5 */
	26,	/* LPC 3:6 */
	61,	/* Code 1-2:0 */
	68,	/* Code 2-2:0 */
	67,	/* Code 2-2:1 */
	66,	/* Code 2-2:2 */
	65,	/* Code 2-2:3 */
	64,	/* Code 2-2:4 */
	63,	/* Code 2-2:5 */
	62,	/* Code 2-2:6 */
	80,	/* Code 1-3:0 */
	79,	/* Code 1-3:1 */
	78,	/* Code 1-3:2 */
	77,	/* Code 1-3:3 */
	76,	/* Code 1-3:4 */
	75,	/* Code 1-3:5 */
	74,	/* Code 1-3:6 */
	84,	/* Code 2-3:3 */
	83,	/* Code 2-3:4 */
	82,	/* Code 2-3:5 */
	81,	/* Code 2-3:6 */
	32,	/* LPC 3:0 */
	4,	/* R0:0 */
	33,	/* INT-LPC:0 */
	60,	/* Code 1-2:1 */
	59,	/* Code 1-2:2 */
	58,	/* Code 1-2:3 */
	57,	/* Code 1-2:4 */
	56,	/* Code 1-2:5 */
	55,	/* Code 1-2:6 */
	49,	/* Code 2-1:0 */
	48,	/* Code 2-1:1 */
	47,	/* Code 2-1:2 */
	46,	/* Code 2-1:3 */
	45,	/* Code 2-1:4 */
	44,	/* Code 2-1:5 */
	43,	/* Code 2-1:6 */
	42,	/* Code 1-1:0 */
	41,	/* Code 1-1:1 */
	40,	/* Code 1-1:2 */
	39,	/* Code 1-1:3 */
	38,	/* Code 1-1:4 */
	37,	/* Code 1-1:5 */
	36,	/* Code 1-1:6 */
	111,	/* GSP 0-4:0 */
	92,	/* GSP 0-3:0 */
	73,	/* GSP 0-2:0 */
	54,	/* GSP 0-1:0 */
	24,	/* LPC 2:0 */
	110,	/* GSP 0-4:1 */
	91,	/* GSP 0-3:1 */
	72,	/* GSP 0-2:1 */
	53,	/* GSP 0-1:1 */
	14,	/* LPC 1:1 */
	13,	/* LPC 1:2 */
	12,	/* LPC 1:3 */
	11,	/* LPC 1:4 */
	10,	/* LPC 1:5 */
	108,	/* GSP 0-4:3 */
	89,	/* GSP 0-3:3 */
	70,	/* GSP 0-2:3 */
	51,	/* GSP 0-1:3 */
	18,	/* LPC 2:6 */
	17,	/* LPC 2:7 */
	16,	/* LPC 2:8 */
	107,	/* GSP 0-4:4 */
	88,	/* GSP 0-3:4 */
	69,	/* GSP 0-2:4 */
	50,	/* GSP 0-1:4 */
	9,	/* LPC 1:6 */
	8,	/* LPC 1:7 */
	7,	/* LPC 1:8 */
	6,	/* LPC 1:9 */
	2,	/* R0:2 */
	5,	/* LPC 1:10 */
	1,	/* R0:3 */
	0,	/* R0:4 */
	35,	/* Mode:0 */
	34,	/* Mode:1 */
	106,	/* Code 2-4:0 */
	105,	/* Code 2-4:1 */
	104,	/* Code 2-4:2 */
	103,	/* Code 2-4:3 */
	102,	/* Code 2-4:4 */
	101,	/* Code 2-4:5 */
	100,	/* Code 2-4:6 */
	99,	/* Code 1-4:0 */
	98,	/* Code 1-4:1 */
	97,	/* Code 1-4:2 */
	96,	/* Code 1-4:3 */
	95,	/* Code 1-4:4 */
	94,	/* Code 1-4:5 */
	93,	/* Code 1-4:6 */
	87,	/* Code 2-3:0 */
	86,	/* Code 2-3:1 */
	85,	/* Code 2-3:2 */
};

/* GSM HR voiced (mode=1,2,3) frames - subjective importance bit ordering */
	/* This array encode mapping between GSM 05.03 Table 3b (bits
	 * ordering before channel coding on TCH) and GSM 06.20 Table B.2
	 * (bit ordering on A-bis */
uint16_t gsm620_voiced_bitorder[112] = {
	13,	/* LPC 1:2 */
	14,	/* LPC 1:1 */
	20,	/* LPC 2:4 */
	19,	/* LPC 2:5 */
	18,	/* LPC 2:6 */
	53,	/* GSP 0-1:4 */
	71,	/* GSP 0-2:4 */
	89,	/* GSP 0-3:4 */
	107,	/* GSP 0-4:4 */
	54,	/* GSP 0-1:3 */
	72,	/* GSP 0-2:3 */
	90,	/* GSP 0-3:3 */
	108,	/* GSP 0-4:3 */
	55,	/* GSP 0-1:2 */
	73,	/* GSP 0-2:2 */
	91,	/* GSP 0-3:2 */
	109,	/* GSP 0-4:2 */
	52,	/* Code 1:0 */
	51,	/* Code 1:1 */
	50,	/* Code 1:2 */
	49,	/* Code 1:3 */
	48,	/* Code 1:4 */
	47,	/* Code 1:5 */
	46,	/* Code 1:6 */
	45,	/* Code 1:7 */
	44,	/* Code 1:8 */
	65,	/* Code 2:5 */
	64,	/* Code 2:6 */
	63,	/* Code 2:7 */
	62,	/* Code 2:8 */
	70,	/* Code 2:0 */
	69,	/* Code 2:1 */
	68,	/* Code 2:2 */
	80,	/* Code 3:8 */
	66,	/* Code 2:4 */
	67,	/* Code 2:3 */
	56,	/* GSP 0-1:1 */
	74,	/* GSP 0-2:1 */
	92,	/* GSP 0-3:1 */
	110,	/* GSP 0-4:1 */
	57,	/* GSP 0-1:0 */
	75,	/* GSP 0-2:0 */
	93,	/* GSP 0-3:0 */
	111,	/* GSP 0-4:0 */
	33,	/* INT-LPC:0 */
	24,	/* LPC 2:0 */
	32,	/* LPC 3:0 */
	97,	/* LAG 4:0 */
	31,	/* LPC 3:1 */
	23,	/* LPC 2:1 */
	96,	/* LAG 4:1 */
	79,	/* LAG 3:0 */
	61,	/* LAG 2:0 */
	43,	/* LAG 1:0 */
	95,	/* LAG 4:2 */
	78,	/* LAG 3:1 */
	60,	/* LAG 2:1 */
	42,	/* LAG 1:1 */
	30,	/* LPC 3:2 */
	29,	/* LPC 3:3 */
	28,	/* LPC 3:4 */
	22,	/* LPC 2:2 */
	27,	/* LPC 3:5 */
	26,	/* LPC 3:6 */
	21,	/* LPC 2:3 */
	4,	/* R0:0 */
	25,	/* LPC 3:7 */
	15,	/* LPC 1:0 */
	94,	/* LAG 4:3 */
	77,	/* LAG 3:2 */
	59,	/* LAG 2:2 */
	41,	/* LAG 1:2 */
	3,	/* R0:1 */
	76,	/* LAG 3:3 */
	58,	/* LAG 2:3 */
	40,	/* LAG 1:3 */
	39,	/* LAG 1:4 */
	17,	/* LPC 2:7 */
	16,	/* LPC 2:8 */
	12,	/* LPC 1:3 */
	11,	/* LPC 1:4 */
	10,	/* LPC 1:5 */
	9,	/* LPC 1:6 */
	2,	/* R0:2 */
	38,	/* LAG 1:5 */
	37,	/* LAG 1:6 */
	36,	/* LAG 1:7 */
	8,	/* LPC 1:7 */
	7,	/* LPC 1:8 */
	6,	/* LPC 1:9 */
	5,	/* LPC 1:10 */
	1,	/* R0:3 */
	0,	/* R0:4 */
	35,	/* Mode:0 */
	34,	/* Mode:1 */
	106,	/* Code 4:0 */
	105,	/* Code 4:1 */
	104,	/* Code 4:2 */
	103,	/* Code 4:3 */
	102,	/* Code 4:4 */
	101,	/* Code 4:5 */
	100,	/* Code 4:6 */
	99,	/* Code 4:7 */
	98,	/* Code 4:8 */
	88,	/* Code 3:0 */
	87,	/* Code 3:1 */
	86,	/* Code 3:2 */
	85,	/* Code 3:3 */
	84,	/* Code 3:4 */
	83,	/* Code 3:5 */
	82,	/* Code 3:6 */
	81,	/* Code 3:7 */
};
