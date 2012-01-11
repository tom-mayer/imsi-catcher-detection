#ifndef OSMO_GSM_PRIM_H
#define OSMO_GSM_PRIM_H

#include <osmocom/core/prim.h>

/* enumeration of GSM related SAPs */
enum osmo_gsm_sap {
	SAP_GSM_PH	= _SAP_GSM_BASE,
	SAP_GSM_DL,
	SAP_GSM_MDL,
};

#endif
