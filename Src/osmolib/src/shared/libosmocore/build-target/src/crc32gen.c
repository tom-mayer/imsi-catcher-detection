/*
 * crc32gen.c
 *
 * Generic CRC routines (for max 32 bits poly)
 *
 * Copyright (C) 2011  Sylvain Munaut <tnt@246tNt.com>
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
 */

/*! \addtogroup crcgen
 *  @{
 */

/*! \file crc32gen.c
 *  \file Osmocom generic CRC routines (for max 32 bits poly)
 */

#include <stdint.h>

#include <osmocom/core/bits.h>
#include <osmocom/core/crc32gen.h>


/*! \brief Compute the CRC value of a given array of hard-bits
 *  \param[in] code The CRC code description to apply
 *  \param[in] in Array of hard bits
 *  \param[in] len Length of the array of hard bits
 *  \returns The CRC value
 */
uint32_t
osmo_crc32gen_compute_bits(const struct osmo_crc32gen_code *code,
                           const ubit_t *in, int len)
{
	const uint32_t poly = code->poly;
	uint32_t crc = code->init;
	int i, n = code->bits-1;

	for (i=0; i<len; i++) {
		uint32_t bit = in[i] & 1;
		crc ^= (bit << n);
		if (crc & (1 << n)) {
			crc <<= 1;
			crc ^= poly;
		} else {
			crc <<= 1;
		}
		crc &= (1ULL << code->bits) - 1;
	}

	crc ^= code->remainder;

	return crc;
}


/*! \brief Checks the CRC value of a given array of hard-bits
 *  \param[in] code The CRC code description to apply
 *  \param[in] in Array of hard bits
 *  \param[in] len Length of the array of hard bits
 *  \param[in] crc_bits Array of hard bits with the alleged CRC
 *  \returns 0 if CRC matches. 1 in case of error.
 *
 * The crc_bits array must have a length of code->len
 */
int
osmo_crc32gen_check_bits(const struct osmo_crc32gen_code *code,
                         const ubit_t *in, int len, const ubit_t *crc_bits)
{
	uint32_t crc;
	int i;

	crc = osmo_crc32gen_compute_bits(code, in, len);

	for (i=0; i<code->bits; i++)
		if (crc_bits[i] ^ ((crc >> (code->bits-i-1)) & 1))
			return 1;

	return 0;
}


/*! \brief Computes and writes the CRC value of a given array of bits
 *  \param[in] code The CRC code description to apply
 *  \param[in] in Array of hard bits
 *  \param[in] len Length of the array of hard bits
 *  \param[in] crc_bits Array of hard bits to write the computed CRC to
 *
 * The crc_bits array must have a length of code->len
 */
void
osmo_crc32gen_set_bits(const struct osmo_crc32gen_code *code,
                       const ubit_t *in, int len, ubit_t *crc_bits)
{
	uint32_t crc;
	int i;

	crc = osmo_crc32gen_compute_bits(code, in, len);

	for (i=0; i<code->bits; i++)
		crc_bits[i] = ((crc >> (code->bits-i-1)) & 1);
}

/*! }@ */

/* vim: set syntax=c: */
