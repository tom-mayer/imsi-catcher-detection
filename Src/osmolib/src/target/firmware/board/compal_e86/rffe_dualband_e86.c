#include <stdint.h>
#include <stdio.h>

#include <debug.h>
#include <memory.h>
#include <rffe.h>
#include <calypso/tsp.h>
#include <rf/trf6151.h>

/* This is a value that has been measured on the C123 by Harald: 71dBm,
   it is the difference between the input level at the antenna and what
   the DSP reports, subtracted by the total gain of the TRF6151 */
#define SYSTEM_INHERENT_GAIN	71

/* describe how the RF frontend is wired on the Motorola E86 board (C139/C140) */

#define		RITA_RESET	TSPACT(0)	/* Reset of the Rita TRF6151 */
#define		PA_ENABLE	TSPACT(1)	/* Enable the Power Amplifier */
#define		DCS_TX		TSPACT(6)	/* DCS Transmit Enable */
#define		GSM_TX1		TSPACT(8)	/* GSM Transmit Enable 1 */

/* in order to save a transistor, the E86 has a second signal for GSM TX */
#define		GSM_TX2		TSPACT(2)	/* GSM Transmit Enable 2 */

#define		IOTA_STROBE	TSPEN(0)	/* Strobe for the Iota TSP */
#define		RITA_STROBE	TSPEN(2)	/* Strobe for the Rita TSP */

/* switch RF Frontend Mode */
void rffe_mode(enum gsm_band band, int tx)
{
	uint16_t tspact = tsp_act_state();

	/* First we mask off all bits from the state cache */
	tspact &= ~(PA_ENABLE);
	tspact |=  DCS_TX | GSM_TX1 | GSM_TX2;	/* low-active */

#ifdef CONFIG_TX_ENABLE
	/* Then we selectively set the bits on, if required */
	if (tx) {
		if (band == GSM_BAND_850 || band == GSM_BAND_900)
			tspact &= ~(GSM_TX1 | GSM_TX2);
		else
			tspact &= ~DCS_TX;
		tspact |= PA_ENABLE;
	}
#endif /* TRANSMIT_SUPPORT */

	tsp_act_update(tspact);
}

/* Returns RF wiring */
uint32_t rffe_get_rx_ports(void)
{
	return (1 << PORT_LO) | (1 << PORT_DCS1800);
}

uint32_t rffe_get_tx_ports(void)
{
	return (1 << PORT_LO) | (1 << PORT_HI);
}


#define MCU_SW_TRACE	0xfffef00e
#define ARM_CONF_REG	0xfffef006

void rffe_init(void)
{
	uint16_t reg;

	reg = readw(ARM_CONF_REG);
	reg &= ~ (1 << 5);	/* TSPACT6 I/O function, not nCS6 */
	writew(reg, ARM_CONF_REG);

	reg = readw(MCU_SW_TRACE);
	reg &= ~(1 << 5);	/* TSPACT8 I/O function, not nMREQ */
	writew(reg, MCU_SW_TRACE);

	/* Configure the TSPEN which is connected to the TWL3025 */
	tsp_setup(IOTA_STROBE, 1, 0, 0);

	trf6151_init(RITA_STROBE, RITA_RESET);
}

uint8_t rffe_get_gain(void)
{
	return trf6151_get_gain();
}

void rffe_set_gain(uint8_t dbm)
{
	trf6151_set_gain(dbm);
}

const uint8_t system_inherent_gain = SYSTEM_INHERENT_GAIN;

/* Given the expected input level of exp_inp dBm/8 and the target of target_bb
 * dBm8, configure the RF Frontend with the respective gain */
void rffe_compute_gain(int16_t exp_inp, int16_t target_bb)
{
	trf6151_compute_gain(exp_inp, target_bb);
}

void rffe_rx_win_ctrl(int16_t exp_inp, int16_t target_bb)
{
	/* FIXME */
}
