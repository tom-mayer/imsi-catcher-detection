#ifndef _GSMTAP_H
#define _GSMTAP_H

/* gsmtap header, pseudo-header in front of the actua GSM payload */

/* GSMTAP is a generic header format for GSM protocol captures,
 * it uses the IANA-assigned UDP port number 4729 and carries
 * payload in various formats of GSM interfaces such as Um MAC
 * blocks or Um bursts.
 *
 * Example programs generating GSMTAP data are airprobe
 * (http://airprobe.org/) or OsmocomBB (http://bb.osmocom.org/)
 */

#include <stdint.h>

#define GSMTAP_VERSION		0x02

#define GSMTAP_TYPE_UM		0x01
#define GSMTAP_TYPE_ABIS	0x02
#define GSMTAP_TYPE_UM_BURST	0x03	/* raw burst bits */
#define GSMTAP_TYPE_SIM		0x04
#define GSMTAP_TYPE_TETRA_I1		0x05	/* tetra air interface */
#define GSMTAP_TYPE_TETRA_I1_BURST	0x06	/* tetra air interface */

/* sub-types for TYPE_UM_BURST */
#define GSMTAP_BURST_UNKNOWN		0x00
#define GSMTAP_BURST_FCCH		0x01
#define GSMTAP_BURST_PARTIAL_SCH	0x02
#define GSMTAP_BURST_SCH		0x03
#define GSMTAP_BURST_CTS_SCH		0x04
#define GSMTAP_BURST_COMPACT_SCH	0x05
#define GSMTAP_BURST_NORMAL		0x06
#define GSMTAP_BURST_DUMMY		0x07
#define GSMTAP_BURST_ACCESS		0x08
#define GSMTAP_BURST_NONE		0x09

/* sub-types for TYPE_UM */
#define GSMTAP_CHANNEL_UNKNOWN	0x00
#define GSMTAP_CHANNEL_BCCH	0x01
#define GSMTAP_CHANNEL_CCCH	0x02
#define GSMTAP_CHANNEL_RACH	0x03
#define GSMTAP_CHANNEL_AGCH	0x04
#define GSMTAP_CHANNEL_PCH	0x05
#define GSMTAP_CHANNEL_SDCCH	0x06
#define GSMTAP_CHANNEL_SDCCH4	0x07
#define GSMTAP_CHANNEL_SDCCH8	0x08
#define GSMTAP_CHANNEL_TCH_F	0x09
#define GSMTAP_CHANNEL_TCH_H	0x0a
#define GSMTAP_CHANNEL_ACCH	0x80

/* sub-types for TYPE_TETRA_AIR */
#define GSMTAP_TETRA_BSCH	0x01
#define GSMTAP_TETRA_AACH	0x02
#define GSMTAP_TETRA_SCH_HU	0x03
#define GSMTAP_TETRA_SCH_HD	0x04
#define GSMTAP_TETRA_SCH_F	0x05
#define GSMTAP_TETRA_BNCH	0x06
#define GSMTAP_TETRA_STCH	0x07
#define GSMTAP_TETRA_TCH_F	0x08

/* flags for the ARFCN */
#define GSMTAP_ARFCN_F_PCS	0x8000
#define GSMTAP_ARFCN_F_UPLINK	0x4000
#define GSMTAP_ARFCN_MASK	0x3fff

/* IANA-assigned well-known UDP port for GSMTAP messages */
#define GSMTAP_UDP_PORT			4729

struct gsmtap_hdr {
	uint8_t version;	/* version, set to 0x01 currently */
	uint8_t hdr_len;	/* length in number of 32bit words */
	uint8_t type;		/* see GSMTAP_TYPE_* */
	uint8_t timeslot;	/* timeslot (0..7 on Um) */

	uint16_t arfcn;		/* ARFCN (frequency) */
	int8_t signal_dbm;	/* signal level in dBm */
	int8_t snr_db;		/* signal/noise ratio in dB */

	uint32_t frame_number;	/* GSM Frame Number (FN) */

	uint8_t sub_type;	/* Type of burst/channel, see above */
	uint8_t antenna_nr;	/* Antenna Number */
	uint8_t sub_slot;	/* sub-slot within timeslot */
	uint8_t res;		/* reserved for future use (RFU) */

} __attribute__((packed));

#endif /* _GSMTAP_H */
