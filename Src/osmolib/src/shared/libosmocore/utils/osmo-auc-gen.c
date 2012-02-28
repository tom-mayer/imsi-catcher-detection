/* GSM/GPRS/3G authentication testing tool */

/* (C) 2010-2011 by Harald Welte <laforge@gnumonks.org>
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


#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>

#include <osmocom/crypt/auth.h>
#include <osmocom/core/utils.h>

static void dump_auth_vec(struct osmo_auth_vector *vec)
{
	printf("RAND:\t%s\n", osmo_hexdump(vec->rand, sizeof(vec->rand)));

	if (vec->auth_types & OSMO_AUTH_TYPE_UMTS) {
		printf("AUTN:\t%s\n", osmo_hexdump(vec->autn, sizeof(vec->autn)));
		printf("IK:\t%s\n", osmo_hexdump(vec->ik, sizeof(vec->ik)));
		printf("CK:\t%s\n", osmo_hexdump(vec->ck, sizeof(vec->ck)));
		printf("RES:\t%s\n", osmo_hexdump(vec->res, vec->res_len));
	}

	if (vec->auth_types & OSMO_AUTH_TYPE_GSM) {
		printf("SRES:\t%s\n", osmo_hexdump(vec->sres, sizeof(vec->sres)));
		printf("Kc:\t%s\n", osmo_hexdump(vec->kc, sizeof(vec->kc)));
	}
}

static struct osmo_sub_auth_data test_aud = {
	.type = OSMO_AUTH_TYPE_NONE,
	.algo = OSMO_AUTH_ALG_NONE,
};

int main(int argc, char **argv)
{
	struct osmo_auth_vector _vec;
	struct osmo_auth_vector *vec = &_vec;
	uint8_t _rand[16];
	int rc, option_index;
	int rand_is_set = 0;

	printf("osmo-auc-gen (C) 2011 by Harald Welte\n");
	printf("This is FREE SOFTWARE with ABSOLUTELY NO WARRANTY\n\n");

	while (1) {
		int c;
		unsigned long ul;
		static struct option long_options[] = {
			{ "2g", 0, 0, '2' },
			{ "3g", 0, 0, '3' },
			{ "algorithm", 1, 0, 'a' },
			{ "key", 1, 0, 'k' },
			{ "opc", 1, 0, 'o' },
			{ "amf", 1, 0, 'f' },
			{ "sqn", 1, 0, 's' },
			{ "rand", 1, 0, 'r' },
			{ 0, 0, 0, 0 }
		};

		rc = 0;

		c = getopt_long(argc, argv, "23a:k:o:f:s:r:", long_options,
				&option_index);

		if (c == -1)
			break;

		switch (c) {
		case '2':
			test_aud.type = OSMO_AUTH_TYPE_GSM;
			break;
		case '3':
			test_aud.type = OSMO_AUTH_TYPE_UMTS;
			break;
		case 'a':
			rc = osmo_auth_alg_parse(optarg);
			if (rc < 0)
				break;
			test_aud.algo = rc;
			break;
		case 'k':
			switch (test_aud.type) {
			case OSMO_AUTH_TYPE_GSM:
				rc = osmo_hexparse(optarg, test_aud.u.gsm.ki,
						   sizeof(test_aud.u.gsm.ki));
				break;
			case OSMO_AUTH_TYPE_UMTS:
				rc = osmo_hexparse(optarg, test_aud.u.umts.k,
						   sizeof(test_aud.u.umts.k));
				break;
			default:
				fprintf(stderr, "please specify 2g/3g first!\n");
			}
			break;
		case 'o':
			if (test_aud.type != OSMO_AUTH_TYPE_UMTS) {
				fprintf(stderr, "Only UMTS has OPC\n");
				exit(2);
			}
			rc = osmo_hexparse(optarg, test_aud.u.umts.opc,
					   sizeof(test_aud.u.umts.opc));
			break;
		case 'f':
			if (test_aud.type != OSMO_AUTH_TYPE_UMTS) {
				fprintf(stderr, "Only UMTS has AMF\n");
				exit(2);
			}
			rc = osmo_hexparse(optarg, test_aud.u.umts.amf,
					   sizeof(test_aud.u.umts.amf));
			break;
		case 's':
			if (test_aud.type != OSMO_AUTH_TYPE_UMTS) {
				fprintf(stderr, "Only UMTS has SQN\n");
				exit(2);
			}
			ul = strtoul(optarg, 0, 10);
			test_aud.u.umts.sqn = ul;
			break;
		case 'r':
			rc = osmo_hexparse(optarg, _rand, sizeof(_rand));
			rand_is_set = 1;
			break;
		}

		if (rc < 0) {
			fprintf(stderr, "Error parsing argument of option `%c'\n", c);
			exit(2);
		}
	}

	if (!rand_is_set) {
		printf("WARNING: We're using really weak random numbers!\n\n");
		srand(time(NULL));
		*(uint32_t *)&_rand[0] = rand();
		*(uint32_t *)(&_rand[4]) = rand();
		*(uint32_t *)(&_rand[8]) = rand();
		*(uint32_t *)(&_rand[12]) = rand();
	}

	memset(vec, 0, sizeof(*vec));

	rc = osmo_auth_gen_vec(vec, &test_aud, _rand);
	if (rc < 0) {
		fprintf(stderr, "error generating auth vector\n");
		exit(1);
	}

	dump_auth_vec(vec);
#if 0
	const uint8_t auts[14] = { 0x87, 0x11, 0xa0, 0xec, 0x9e, 0x16, 0x37, 0xdf,
			     0x17, 0xf8, 0x0b, 0x38, 0x4e, 0xe4 };

	rc = osmo_auth_gen_vec_auts(vec, &test_aud, auts, _rand, _rand);
	if (rc < 0) {
		printf("AUTS failed\n");
	} else {
		printf("AUTS success: SEQ.MS = %lu\n", test_aud.u.umts.sqn);
	}
#endif
	exit(0);

}
