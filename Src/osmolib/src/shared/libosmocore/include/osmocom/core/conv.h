/*
 * conv.h
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

/*! \defgroup conv Convolutional encoding and decoding routines
 *  @{
 */

/*! \file conv.h
 *  \file Osmocom convolutional encoder and decoder
 */

#ifndef __OSMO_CONV_H__
#define __OSMO_CONV_H__

#include <stdint.h>

#include <osmocom/core/bits.h>

/*! \brief possibe termination types
 *
 *  The termination type will determine which state the encoder/decoder
 *  can start/end with. This is mostly taken care of in the high level API
 *  call. So if you use the low level API, you must take care of making the
 *  proper calls yourself.
 */
enum osmo_conv_term {
	CONV_TERM_FLUSH = 0,	/*!< \brief Flush encoder state */
	CONV_TERM_TRUNCATION,	/*!< \brief Direct truncation */
	CONV_TERM_TAIL_BITING,	/*!< \brief Tail biting */
};

/*! \brief structure describing a given convolutional code
 *
 *  The only required fields are N,K and the next_output/next_state arrays. The
 *  other can be left to default value of zero depending on what the code does.
 *  If 'len' is left at 0 then only the low level API can be used.
 */
struct osmo_conv_code {
	int N;				/*!< \brief Inverse of code rate */
	int K;				/*!< \brief Constraint length */
	int len;			/*!< \brief # of data bits */

	enum osmo_conv_term term;	/*!< \brief Termination type */

	const uint8_t (*next_output)[2];/*!< \brief Next output array */
	const uint8_t (*next_state)[2];	/*!< \brief Next state array  */

	const uint8_t *next_term_output;/*!< \brief Flush termination output */
	const uint8_t *next_term_state;	/*!< \brief Flush termination state  */

	const int *puncture;		/*!< \brief Punctured bits indexes */
};


/* Common */

int osmo_conv_get_input_length(const struct osmo_conv_code *code, int len);
int osmo_conv_get_output_length(const struct osmo_conv_code *code, int len);


/* Encoding */

	/* Low level API */

/*! \brief convolutional encoder state */
struct osmo_conv_encoder {
	const struct osmo_conv_code *code; /*!< \brief for which code? */
	int i_idx;	/*!< \brief Next input bit index */
	int p_idx;	/*!< \brief Current puncture index */
	uint8_t state;	/*!< \brief Current state */
};

void osmo_conv_encode_init(struct osmo_conv_encoder *encoder,
                           const struct osmo_conv_code *code);
void osmo_conv_encode_load_state(struct osmo_conv_encoder *encoder,
                                 const ubit_t *input);
int  osmo_conv_encode_raw(struct osmo_conv_encoder *encoder,
                          const ubit_t *input, ubit_t *output, int n);
int  osmo_conv_encode_flush(struct osmo_conv_encoder *encoder, ubit_t *output);

	/* All-in-one */
int  osmo_conv_encode(const struct osmo_conv_code *code,
                      const ubit_t *input, ubit_t *output);


/* Decoding */

	/* Low level API */

/*! \brief convolutional decoder state */
struct osmo_conv_decoder {
	const struct osmo_conv_code *code; /*!< \brief for which code? */

	int n_states;		/*!< \brief number of states */

	int len;		/*!< \brief Max o_idx (excl. termination) */

	int o_idx;		/*!< \brief output index */
	int p_idx;		/*!< \brief puncture index */

	unsigned int *ae;	/*!< \brief accumulated error */
	unsigned int *ae_next;	/*!< \brief next accumulated error (tmp in scan) */
	uint8_t *state_history;	/*!< \brief state history [len][n_states] */
};

void osmo_conv_decode_init(struct osmo_conv_decoder *decoder,
                           const struct osmo_conv_code *code,
                           int len, int start_state);
void osmo_conv_decode_reset(struct osmo_conv_decoder *decoder, int start_state);
void osmo_conv_decode_rewind(struct osmo_conv_decoder *decoder);
void osmo_conv_decode_deinit(struct osmo_conv_decoder *decoder);

int osmo_conv_decode_scan(struct osmo_conv_decoder *decoder,
                          const sbit_t *input, int n);
int osmo_conv_decode_flush(struct osmo_conv_decoder *decoder,
                           const sbit_t *input);
int osmo_conv_decode_get_output(struct osmo_conv_decoder *decoder,
                                ubit_t *output, int has_flush, int end_state);

	/* All-in-one */
int osmo_conv_decode(const struct osmo_conv_code *code,
                     const sbit_t *input, ubit_t *output);


/*! }@ */

#endif /* __OSMO_CONV_H__ */
