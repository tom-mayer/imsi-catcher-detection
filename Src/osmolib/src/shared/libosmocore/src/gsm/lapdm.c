/* GSM LAPDm (TS 04.06) implementation */

/* (C) 2010-2011 by Harald Welte <laforge@gnumonks.org>
 * (C) 2010 by Andreas Eversberg <jolly@eversberg.eu>
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

/*! \addtogroup lapdm
 *  @{
 */

/*! \file lapdm.c */

/*!
 * Notes on Buffering: rcv_buffer, tx_queue, tx_hist, send_buffer, send_queue
 *
 * RX data is stored in the rcv_buffer (pointer). If the message is complete, it
 * is removed from rcv_buffer pointer and forwarded to L3. If the RX data is
 * received while there is an incomplete rcv_buffer, it is appended to it.
 *
 * TX data is stored in the send_queue first. When transmitting a frame,
 * the first message in the send_queue is moved to the send_buffer. There it
 * resides until all fragments are acknowledged. Fragments to be sent by I
 * frames are stored in the tx_hist buffer for resend, if required. Also the
 * current fragment is copied into the tx_queue. There it resides until it is
 * forwarded to layer 1.
 *
 * In case we have SAPI 0, we only have a window size of 1, so the unack-
 * nowledged message resides always in the send_buffer. In case of a suspend,
 * it can be written back to the first position of the send_queue.
 *
 * The layer 1 normally sends a PH-READY-TO-SEND. But because we use
 * asynchronous transfer between layer 1 and layer 2 (serial link), we must
 * send a frame before layer 1 reaches the right timeslot to send it. So we
 * move the tx_queue to layer 1 when there is not already a pending frame, and
 * wait until acknowledge after the frame has been sent. If we receive an
 * acknowledge, we can send the next frame from the buffer, if any.
 *
 * The moving of tx_queue to layer 1 may also trigger T200, if desired. Also it
 * will trigger next I frame, if possible.
 *
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>

#include <osmocom/core/logging.h>
#include <osmocom/core/timer.h>
#include <osmocom/core/msgb.h>
#include <osmocom/core/utils.h>

#include <osmocom/gsm/tlv.h>
#include <osmocom/gsm/rsl.h>
#include <osmocom/gsm/prim.h>
#include <osmocom/gsm/gsm_utils.h>
#include <osmocom/gsm/lapdm.h>

#include <osmocom/gsm/protocol/gsm_04_08.h>
#include <osmocom/gsm/protocol/gsm_08_58.h>

/* TS 04.06 Figure 4 / Section 3.2 */
#define LAPDm_LPD_NORMAL  0
#define LAPDm_LPD_SMSCB	  1
#define LAPDm_SAPI_NORMAL 0
#define LAPDm_SAPI_SMS	  3
#define LAPDm_ADDR(lpd, sapi, cr) ((((lpd) & 0x3) << 5) | (((sapi) & 0x7) << 2) | (((cr) & 0x1) << 1) | 0x1)

#define LAPDm_ADDR_SAPI(addr) (((addr) >> 2) & 0x7)
#define LAPDm_ADDR_CR(addr) (((addr) >> 1) & 0x1)
#define LAPDm_ADDR_EA(addr) ((addr) & 0x1)

/* TS 04.06 Table 3 / Section 3.4.3 */
#define LAPDm_CTRL_I(nr, ns, p)	((((nr) & 0x7) << 5) | (((p) & 0x1) << 4) | (((ns) & 0x7) << 1))
#define LAPDm_CTRL_S(nr, s, p)	((((nr) & 0x7) << 5) | (((p) & 0x1) << 4) | (((s) & 0x3) << 2) | 0x1)
#define LAPDm_CTRL_U(u, p)	((((u) & 0x1c) << (5-2)) | (((p) & 0x1) << 4) | (((u) & 0x3) << 2) | 0x3)

#define LAPDm_CTRL_is_I(ctrl)	(((ctrl) & 0x1) == 0)
#define LAPDm_CTRL_is_S(ctrl)	(((ctrl) & 0x3) == 1)
#define LAPDm_CTRL_is_U(ctrl)	(((ctrl) & 0x3) == 3)

#define LAPDm_CTRL_U_BITS(ctrl)	((((ctrl) & 0xC) >> 2) | ((ctrl) & 0xE0) >> 3)
#define LAPDm_CTRL_PF_BIT(ctrl)	(((ctrl) >> 4) & 0x1)

#define LAPDm_CTRL_S_BITS(ctrl)	(((ctrl) & 0xC) >> 2)

#define LAPDm_CTRL_I_Ns(ctrl)	(((ctrl) & 0xE) >> 1)
#define LAPDm_CTRL_Nr(ctrl)	(((ctrl) & 0xE0) >> 5)

/* TS 04.06 Table 4 / Section 3.8.1 */
#define LAPDm_U_SABM	0x7
#define LAPDm_U_DM	0x3
#define LAPDm_U_UI	0x0
#define LAPDm_U_DISC	0x8
#define LAPDm_U_UA	0xC

#define LAPDm_S_RR	0x0
#define LAPDm_S_RNR	0x1
#define LAPDm_S_REJ	0x2

#define LAPDm_LEN(len)	((len << 2) | 0x1)
#define LAPDm_MORE	0x2

/* TS 04.06 Section 5.8.3 */
#define N201_AB_SACCH		18
#define N201_AB_SDCCH		20
#define N201_AB_FACCH		20
#define N201_Bbis		23
#define N201_Bter_SACCH		21
#define N201_Bter_SDCCH		23
#define N201_Bter_FACCH		23
#define N201_B4			19

/* 5.8.2.1 N200 during establish and release */
#define N200_EST_REL		5
/* 5.8.2.1 N200 during timer recovery state */
#define N200_TR_SACCH		5
#define N200_TR_SDCCH		23
#define N200_TR_FACCH_FR	34
#define N200_TR_EFACCH_FR	48
#define N200_TR_FACCH_HR	29
/* FIXME: this depends on chan type */
#define N200	N200_TR_SACCH

#define CR_MS2BS_CMD	0
#define CR_MS2BS_RESP	1
#define CR_BS2MS_CMD	1
#define CR_BS2MS_RESP	0

/* Set T200 to 1 Second (OpenBTS uses 900ms) */
#define T200	1, 0

/* k value for each SAPI */
static uint8_t k_sapi[] = {1, 1, 1, 1, 1, 1, 1, 1};

enum lapdm_format {
	LAPDm_FMT_A,
	LAPDm_FMT_B,
	LAPDm_FMT_Bbis,
	LAPDm_FMT_Bter,
	LAPDm_FMT_B4,
};

static void lapdm_t200_cb(void *data);
static int rslms_send_i(struct lapdm_msg_ctx *mctx, int line);

/* UTILITY FUNCTIONS */

static inline uint8_t inc_mod8(uint8_t x)
{
	return (x + 1) & 7;
}

static inline uint8_t add_mod8(uint8_t x, uint8_t y)
{
	return (x + y) & 7;
}

static inline uint8_t sub_mod8(uint8_t x, uint8_t y)
{
	return (x - y) & 7; /* handle negative results correctly */
}

static void lapdm_dl_init(struct lapdm_datalink *dl,
			  struct lapdm_entity *entity)
{
	memset(dl, 0, sizeof(*dl));
	INIT_LLIST_HEAD(&dl->send_queue);
	INIT_LLIST_HEAD(&dl->tx_queue);
	dl->state = LAPDm_STATE_IDLE;
	dl->t200.data = dl;
	dl->t200.cb = &lapdm_t200_cb;
	dl->entity = entity;
}

/*! \brief initialize a LAPDm entity and all datalinks inside
 *  \param[in] le LAPDm entity
 *  \param[in] mode \ref lapdm_mode (BTS/MS)
 */
void lapdm_entity_init(struct lapdm_entity *le, enum lapdm_mode mode)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(le->datalink); i++)
		lapdm_dl_init(&le->datalink[i], le);

	lapdm_entity_set_mode(le, mode);
}

/*! \brief initialize a LAPDm channel and all its channels
 *  \param[in] lc \ref lapdm_channel to be initialized
 *  \param[in] mode \ref lapdm_mode (BTS/MS)
 *
 * This really is a convenience wrapper around calling \ref
 * lapdm_entity_init twice.
 */
void lapdm_channel_init(struct lapdm_channel *lc, enum lapdm_mode mode)
{
	lapdm_entity_init(&lc->lapdm_acch, mode);
	lapdm_entity_init(&lc->lapdm_dcch, mode);
}

static void lapdm_dl_flush_send(struct lapdm_datalink *dl)
{
	struct msgb *msg;

	/* Flush send-queue */
	while ((msg = msgb_dequeue(&dl->send_queue)))
		msgb_free(msg);

	/* Clear send-buffer */
	if (dl->send_buffer) {
		msgb_free(dl->send_buffer);
		dl->send_buffer = NULL;
	}
}

static void lapdm_dl_flush_tx(struct lapdm_datalink *dl)
{
	struct msgb *msg;
	unsigned int i;

	while ((msg = msgb_dequeue(&dl->tx_queue)))
		msgb_free(msg);
	for (i = 0; i < 8; i++)
		dl->tx_length[i] = 0;
}

/*! \brief flush and release all resoures in LAPDm entity */
void lapdm_entity_exit(struct lapdm_entity *le)
{
	unsigned int i;
	struct lapdm_datalink *dl;

	for (i = 0; i < ARRAY_SIZE(le->datalink); i++) {
		dl = &le->datalink[i];
		lapdm_dl_flush_tx(dl);
		lapdm_dl_flush_send(dl);
		if (dl->rcv_buffer)
			msgb_free(dl->rcv_buffer);
	}
}

/* \brief lfush and release all resources in LAPDm channel
 *
 * A convenience wrapper calling \ref lapdm_entity_exit on both
 * entities inside the \ref lapdm_channel
 */
void lapdm_channel_exit(struct lapdm_channel *lc)
{
	lapdm_entity_exit(&lc->lapdm_acch);
	lapdm_entity_exit(&lc->lapdm_dcch);
}

static void lapdm_dl_newstate(struct lapdm_datalink *dl, uint32_t state)
{
	LOGP(DLLAPDM, LOGL_INFO, "new state %s -> %s\n",
		lapdm_state_names[dl->state], lapdm_state_names[state]);
	
	dl->state = state;
}

static struct lapdm_datalink *datalink_for_sapi(struct lapdm_entity *le, uint8_t sapi)
{
	switch (sapi) {
	case LAPDm_SAPI_NORMAL:
		return &le->datalink[0];
	case LAPDm_SAPI_SMS:
		return &le->datalink[1];
	default:
		return NULL;
	}
}

/* remove the L2 header from a MSGB */
static inline unsigned char *msgb_pull_l2h(struct msgb *msg)
{
	unsigned char *ret = msgb_pull(msg, msg->l3h - msg->l2h);
	msg->l2h = NULL;
	return ret;
}

/* Append padding (if required) */
static void lapdm_pad_msgb(struct msgb *msg, uint8_t n201)
{
	int pad_len = n201 - msgb_l2len(msg);
	uint8_t *data;

	if (pad_len < 0) {
		LOGP(DLLAPDM, LOGL_ERROR,
		     "cannot pad message that is already too big!\n");
		return;
	}

	data = msgb_put(msg, pad_len);
	memset(data, 0x2B, pad_len);
}

/* input function that L2 calls when sending messages up to L3 */
static int rslms_sendmsg(struct msgb *msg, struct lapdm_entity *le)
{
	if (!le->l3_cb) {
		msgb_free(msg);
		return -EIO;
	}

	/* call the layer2 message handler that is registered */
	return le->l3_cb(msg, le, le->l3_ctx);
}

/* write a frame into the tx queue */
static int tx_ph_data_enqueue(struct lapdm_datalink *dl, struct msgb *msg,
				uint8_t chan_nr, uint8_t link_id, uint8_t n201)
{
	struct lapdm_entity *le = dl->entity;
	struct osmo_phsap_prim pp;

	/* if there is a pending message, queue it */
	if (le->tx_pending || le->flags & LAPDM_ENT_F_POLLING_ONLY) {
		*msgb_push(msg, 1) = n201;
		*msgb_push(msg, 1) = link_id;
		*msgb_push(msg, 1) = chan_nr;
		msgb_enqueue(&dl->tx_queue, msg);
		return -EBUSY;
	}

	osmo_prim_init(&pp.oph, SAP_GSM_PH, PRIM_PH_DATA,
			PRIM_OP_REQUEST, msg);
	pp.u.data.chan_nr = chan_nr;
	pp.u.data.link_id = link_id;

	/* send the frame now */
	le->tx_pending = 0; /* disabled flow control */
	lapdm_pad_msgb(msg, n201);

	return le->l1_prim_cb(&pp.oph, le->l1_ctx);
}

static struct msgb *tx_dequeue_msgb(struct lapdm_entity *le)
{
	struct lapdm_datalink *dl;
	int last = le->last_tx_dequeue;
	int i = last, n = ARRAY_SIZE(le->datalink);
	struct msgb *msg = NULL;

	/* round-robin dequeue */
	do {
		/* next */
		i = (i + 1) % n;
		dl = &le->datalink[i];
		if ((msg = msgb_dequeue(&dl->tx_queue)))
			break;
	} while (i != last);

	if (msg) {
		/* Set last dequeue position */
		le->last_tx_dequeue = i;
	}

	return msg;
}

/*! \brief dequeue a msg that's pending transmission via L1 and wrap it into
 * a osmo_phsap_prim */
int lapdm_phsap_dequeue_prim(struct lapdm_entity *le, struct osmo_phsap_prim *pp)
{
	struct msgb *msg;
	uint8_t n201;

	msg = tx_dequeue_msgb(le);
	if (!msg)
		return -ENODEV;

	/* if we have a message, send PH-DATA.req */
	osmo_prim_init(&pp->oph, SAP_GSM_PH, PRIM_PH_DATA,
			PRIM_OP_REQUEST, msg);

	/* Pull chan_nr and link_id */
	pp->u.data.chan_nr = *msg->data;
	msgb_pull(msg, 1);
	pp->u.data.link_id = *msg->data;
	msgb_pull(msg, 1);
	n201 = *msg->data;
	msgb_pull(msg, 1);

	/* Pad the frame, we can transmit now */
	lapdm_pad_msgb(msg, n201);

	return 0;
}

/* get next frame from the tx queue. because the ms has multiple datalinks,
 * each datalink's queue is read round-robin.
 */
static int l2_ph_data_conf(struct msgb *msg, struct lapdm_entity *le)
{
	struct osmo_phsap_prim pp;

	/* we may send again */
	le->tx_pending = 0;

	/* free confirm message */
	if (msg)
		msgb_free(msg);

	if (lapdm_phsap_dequeue_prim(le, &pp) < 0) {
		/* no message in all queues */

		/* If user didn't request PH-EMPTY_FRAME.req, abort */
		if (!(le->flags & LAPDM_ENT_F_EMPTY_FRAME))
			return 0;

		/* otherwise, send PH-EMPTY_FRAME.req */
		osmo_prim_init(&pp.oph, SAP_GSM_PH,
				PRIM_PH_EMPTY_FRAME,
				PRIM_OP_REQUEST, NULL);
	} else {
		le->tx_pending = 1;
	}

	return le->l1_prim_cb(&pp.oph, le->l1_ctx);
}

/* Create RSLms various RSLms messages */
static int send_rslms_rll_l3(uint8_t msg_type, struct lapdm_msg_ctx *mctx,
			     struct msgb *msg)
{
	/* Add the RSL + RLL header */
	rsl_rll_push_l3(msg, msg_type, mctx->chan_nr, mctx->link_id, 1);

	/* send off the RSLms message to L3 */
	return rslms_sendmsg(msg, mctx->dl->entity);
}

/* Take a B4 format message from L1 and create RSLms UNIT DATA IND */
static int send_rslms_rll_l3_ui(struct lapdm_msg_ctx *mctx, struct msgb *msg)
{
	uint8_t l3_len = msg->tail - (uint8_t *)msgb_l3(msg);
	struct abis_rsl_rll_hdr *rllh;

	/* Add the RSL + RLL header */
	msgb_tv16_push(msg, RSL_IE_L3_INFO, l3_len);
	msgb_push(msg, 2 + 2);
	rsl_rll_push_hdr(msg, RSL_MT_UNIT_DATA_IND, mctx->chan_nr,
		mctx->link_id, 1);
	rllh = (struct abis_rsl_rll_hdr *)msgb_l2(msg);

	rllh->data[0] = RSL_IE_TIMING_ADVANCE;
	rllh->data[1] = mctx->ta_ind;

	rllh->data[2] = RSL_IE_MS_POWER;
	rllh->data[3] = mctx->tx_power_ind;
	
	return rslms_sendmsg(msg, mctx->dl->entity);
}

static int send_rll_simple(uint8_t msg_type, struct lapdm_msg_ctx *mctx)
{
	struct msgb *msg;

	msg = rsl_rll_simple(msg_type, mctx->chan_nr, mctx->link_id, 1);

	/* send off the RSLms message to L3 */
	return rslms_sendmsg(msg, mctx->dl->entity);
}

static int rsl_rll_error(uint8_t cause, struct lapdm_msg_ctx *mctx)
{
	struct msgb *msg;

	LOGP(DLLAPDM, LOGL_NOTICE, "sending MDL-ERROR-IND %d\n", cause);
	msg = rsl_rll_simple(RSL_MT_ERROR_IND, mctx->chan_nr, mctx->link_id, 1);
	msg->l2h = msgb_put(msg, sizeof(struct abis_rsl_rll_hdr));
	msgb_tlv_put(msg, RSL_IE_RLM_CAUSE, 1, &cause);
	return rslms_sendmsg(msg, mctx->dl->entity);
}

static int check_length_ind(struct lapdm_msg_ctx *mctx, uint8_t length_ind)
{
	if (!(length_ind & 0x01)) {
		/* G.4.1 If the EL bit is set to "0", an MDL-ERROR-INDICATION
		 * primitive with cause "frame not implemented" is sent to the
		 * mobile management entity. */
		LOGP(DLLAPDM, LOGL_NOTICE,
			"we don't support multi-octet length\n");
		rsl_rll_error(RLL_CAUSE_FRM_UNIMPL, mctx);
		return -EINVAL;
	}
	return 0;
}

static int lapdm_send_resend(struct lapdm_datalink *dl)
{
	struct msgb *msg = msgb_alloc_headroom(23+10, 10, "LAPDm resend");
	int length;

	/* Resend SABM/DISC from tx_hist */
	length = dl->tx_length[0];
	msg->l2h = msgb_put(msg, length);
	memcpy(msg->l2h, dl->tx_hist[dl->V_send], length);

	return tx_ph_data_enqueue(dl, msg, dl->mctx.chan_nr, dl->mctx.link_id,
			dl->mctx.n201);
}

static int lapdm_send_ua(struct lapdm_msg_ctx *mctx, uint8_t len, uint8_t *data)
{
	uint8_t sapi = mctx->link_id & 7;
	uint8_t f_bit = LAPDm_CTRL_PF_BIT(mctx->ctrl);
	struct msgb *msg = msgb_alloc_headroom(23+10, 10, "LAPDm UA");
	struct lapdm_entity *le = mctx->dl->entity;

	msg->l2h = msgb_put(msg, 3 + len);
	msg->l2h[0] = LAPDm_ADDR(LAPDm_LPD_NORMAL, sapi, le->cr.loc2rem.resp);
	msg->l2h[1] = LAPDm_CTRL_U(LAPDm_U_UA, f_bit);
	msg->l2h[2] = LAPDm_LEN(len);
	if (len)
		memcpy(msg->l2h + 3, data, len);

	return tx_ph_data_enqueue(mctx->dl, msg, mctx->chan_nr, mctx->link_id,
			mctx->n201);
}

static int lapdm_send_dm(struct lapdm_msg_ctx *mctx)
{
	uint8_t sapi = mctx->link_id & 7;
	uint8_t f_bit = LAPDm_CTRL_PF_BIT(mctx->ctrl);
	struct msgb *msg = msgb_alloc_headroom(23+10, 10, "LAPDm DM");
	struct lapdm_entity *le = mctx->dl->entity;

	msg->l2h = msgb_put(msg, 3);
	msg->l2h[0] = LAPDm_ADDR(LAPDm_LPD_NORMAL, sapi, le->cr.loc2rem.resp);
	msg->l2h[1] = LAPDm_CTRL_U(LAPDm_U_DM, f_bit);
	msg->l2h[2] = 0;

	return tx_ph_data_enqueue(mctx->dl, msg, mctx->chan_nr, mctx->link_id,
			mctx->n201);
}

static int lapdm_send_rr(struct lapdm_msg_ctx *mctx, uint8_t f_bit)
{
	uint8_t sapi = mctx->link_id & 7;
	struct msgb *msg = msgb_alloc_headroom(23+10, 10, "LAPDm RR");
	struct lapdm_entity *le = mctx->dl->entity;

	msg->l2h = msgb_put(msg, 3);
	msg->l2h[0] = LAPDm_ADDR(LAPDm_LPD_NORMAL, sapi, le->cr.loc2rem.resp);
	msg->l2h[1] = LAPDm_CTRL_S(mctx->dl->V_recv, LAPDm_S_RR, f_bit);
	msg->l2h[2] = LAPDm_LEN(0);

	return tx_ph_data_enqueue(mctx->dl, msg, mctx->chan_nr, mctx->link_id,
			mctx->n201);
}

static int lapdm_send_rnr(struct lapdm_msg_ctx *mctx, uint8_t f_bit)
{
	uint8_t sapi = mctx->link_id & 7;
	struct msgb *msg = msgb_alloc_headroom(23+10, 10, "LAPDm RNR");
	struct lapdm_entity *le = mctx->dl->entity;

	msg->l2h = msgb_put(msg, 3);
	msg->l2h[0] = LAPDm_ADDR(LAPDm_LPD_NORMAL, sapi, le->cr.loc2rem.resp);
	msg->l2h[1] = LAPDm_CTRL_S(mctx->dl->V_recv, LAPDm_S_RNR, f_bit);
	msg->l2h[2] = LAPDm_LEN(0);

	return tx_ph_data_enqueue(mctx->dl, msg, mctx->chan_nr, mctx->link_id,
			mctx->n201);
}

static int lapdm_send_rej(struct lapdm_msg_ctx *mctx, uint8_t f_bit)
{
	uint8_t sapi = mctx->link_id & 7;
	struct msgb *msg = msgb_alloc_headroom(23+10, 10, "LAPDm REJ");
	struct lapdm_entity *le = mctx->dl->entity;

	msg->l2h = msgb_put(msg, 3);
	msg->l2h[0] = LAPDm_ADDR(LAPDm_LPD_NORMAL, sapi, le->cr.loc2rem.resp);
	msg->l2h[1] = LAPDm_CTRL_S(mctx->dl->V_recv, LAPDm_S_REJ, f_bit);
	msg->l2h[2] = LAPDm_LEN(0);

	return tx_ph_data_enqueue(mctx->dl, msg, mctx->chan_nr, mctx->link_id,
			mctx->n201);
}

/* Timer callback on T200 expiry */
static void lapdm_t200_cb(void *data)
{
	struct lapdm_datalink *dl = data;

	LOGP(DLLAPDM, LOGL_INFO, "lapdm_t200_cb(%p) state=%u\n", dl, dl->state);

	switch (dl->state) {
	case LAPDm_STATE_SABM_SENT:
		/* 5.4.1.3 */
		if (dl->retrans_ctr + 1 >= N200_EST_REL + 1) {
			/* send RELEASE INDICATION to L3 */
			send_rll_simple(RSL_MT_REL_IND, &dl->mctx);
			/* send MDL ERROR INIDCATION to L3 */
			rsl_rll_error(RLL_CAUSE_T200_EXPIRED, &dl->mctx);
			/* flush tx buffers */
			lapdm_dl_flush_tx(dl);
			lapdm_dl_flush_send(dl);
			/* go back to idle state */
			lapdm_dl_newstate(dl, LAPDm_STATE_IDLE);
			/* NOTE: we must not change any other states or buffers
			 * and queues, since we may reconnect after handover
			 * failure. the buffered messages is replaced there */
			break;
		}
		/* retransmit SABM command */
		lapdm_send_resend(dl);
		/* increment re-transmission counter */
		dl->retrans_ctr++;
		/* restart T200 (PH-READY-TO-SEND) */
		osmo_timer_schedule(&dl->t200, T200);
		break;
	case LAPDm_STATE_DISC_SENT:
		/* 5.4.4.3 */
		if (dl->retrans_ctr + 1 >= N200_EST_REL + 1) {
			/* send RELEASE INDICATION to L3 */
			send_rll_simple(RSL_MT_REL_CONF, &dl->mctx);
			/* send MDL ERROR INIDCATION to L3 */
			rsl_rll_error(RLL_CAUSE_T200_EXPIRED, &dl->mctx);
			/* flush buffers */
			lapdm_dl_flush_tx(dl);
			lapdm_dl_flush_send(dl);
			/* go back to idle state */
			lapdm_dl_newstate(dl, LAPDm_STATE_IDLE);
			/* NOTE: we must not change any other states or buffers
			 * and queues, since we may reconnect after handover
			 * failure. the buffered messages is replaced there */
			break;
		}
		/* retransmit DISC command */
		lapdm_send_resend(dl);
		/* increment re-transmission counter */
		dl->retrans_ctr++;
		/* restart T200 (PH-READY-TO-SEND) */
		osmo_timer_schedule(&dl->t200, T200);
		break;
	case LAPDm_STATE_MF_EST:
		/* 5.5.7 */
		dl->retrans_ctr = 0;
		lapdm_dl_newstate(dl, LAPDm_STATE_TIMER_RECOV);
		/* fall through */
	case LAPDm_STATE_TIMER_RECOV:
		dl->retrans_ctr++;
		if (dl->retrans_ctr < N200) {
			/* retransmit I frame (V_s-1) with P=1, if any */
			if (dl->tx_length[sub_mod8(dl->V_send, 1)]) {
				struct msgb *msg;
				int length;

				LOGP(DLLAPDM, LOGL_INFO, "retransmit last frame "
					"V(S)=%d\n", sub_mod8(dl->V_send, 1));
				/* Create I frame (segment) from tx_hist */
				length = dl->tx_length[sub_mod8(dl->V_send, 1)];
				msg = msgb_alloc_headroom(23+10, 10, "LAPDm I");
				msg->l2h = msgb_put(msg, length);
				memcpy(msg->l2h,
					dl->tx_hist[sub_mod8(dl->V_send, 1)],
					length);
				msg->l2h[1] = LAPDm_CTRL_I(dl->V_recv,
					sub_mod8(dl->V_send, 1), 1); /* P=1 */
				tx_ph_data_enqueue(dl, msg, dl->mctx.chan_nr,
					dl->mctx.link_id, dl->mctx.n201);
			} else {
			/* OR send appropriate supervision frame with P=1 */
				if (!dl->own_busy && !dl->seq_err_cond) {
					lapdm_send_rr(&dl->mctx, 1);
					/* NOTE: In case of sequence error
					 * condition, the REJ frame has been
					 * transmitted when entering the
					 * condition, so it has not be done
					 * here
				 	 */
				} else if (dl->own_busy) {
					lapdm_send_rnr(&dl->mctx, 1);
				} else {
					LOGP(DLLAPDM, LOGL_INFO, "unhandled, "
						"pls. fix\n");
				}
			}
			/* restart T200 (PH-READY-TO-SEND) */
			osmo_timer_schedule(&dl->t200, T200);
		} else {
			/* send MDL ERROR INIDCATION to L3 */
			rsl_rll_error(RLL_CAUSE_T200_EXPIRED, &dl->mctx);
		}
		break;
	default:
		LOGP(DLLAPDM, LOGL_INFO, "T200 expired in unexpected "
			"dl->state %u\n", dl->state);
	}
}

/* 5.5.3.1: Common function to acknowlege frames up to the given N(R) value */
static void lapdm_acknowledge(struct lapdm_msg_ctx *mctx)
{
	struct lapdm_datalink *dl = mctx->dl;
	uint8_t nr = LAPDm_CTRL_Nr(mctx->ctrl);
	int s = 0, rej = 0, t200_reset = 0;
	int i;

	/* supervisory frame ? */
	if (LAPDm_CTRL_is_S(mctx->ctrl))
		s = 1;
	/* REJ frame ? */
	if (s && LAPDm_CTRL_S_BITS(mctx->ctrl) == LAPDm_S_REJ)
	 	rej = 1;

	/* Flush all transmit buffers of acknowledged frames */
	for (i = dl->V_ack; i != nr; i = inc_mod8(i)) {
		if (dl->tx_length[i]) {
			dl->tx_length[i] = 0;
			LOGP(DLLAPDM, LOGL_INFO, "ack frame %d\n", i);
		}
	}

	if (dl->state != LAPDm_STATE_TIMER_RECOV) {
		/* When not in the timer recovery condition, the data
		 * link layer entity shall reset the timer T200 on
		 * receipt of a valid I frame with N(R) higher than V(A),
		 * or an REJ with an N(R) equal to V(A). */
		if ((!rej && nr != dl->V_ack)
		 || (rej && nr == dl->V_ack)) {
			LOGP(DLLAPDM, LOGL_INFO, "reset t200\n");
		 	t200_reset = 1;
			osmo_timer_del(&dl->t200);
			/* 5.5.3.1 Note 1 + 2 imply timer recovery cond. */
		}
		/* 5.7.4: N(R) sequence error
		 * N(R) is called valid, if and only if
		 * (N(R)-V(A)) mod 8 <= (V(S)-V(A)) mod 8.
		 */
		if (sub_mod8(nr, dl->V_ack) > sub_mod8(dl->V_send, dl->V_ack)) {
			LOGP(DLLAPDM, LOGL_NOTICE, "N(R) sequence error\n");
			rsl_rll_error(RLL_CAUSE_SEQ_ERR, mctx);
		}
	}

	/* V(A) shall be set to the value of N(R) */
	dl->V_ack = nr;

	/* If T200 has been reset by the receipt of an I, RR or RNR frame,
	 * and if there are outstanding I frames, restart T200 */
	if (t200_reset && !rej) {
		if (dl->tx_length[dl->V_send - 1]) {
			LOGP(DLLAPDM, LOGL_INFO, "start T200, due to unacked I "
				"frame(s)\n");
			osmo_timer_schedule(&dl->t200, T200);
		}
	}
}

/* L1 -> L2 */

/* Receive a LAPDm U (Unnumbered) message from L1 */
static int lapdm_rx_u(struct msgb *msg, struct lapdm_msg_ctx *mctx)
{
	struct lapdm_datalink *dl = mctx->dl;
	struct lapdm_entity *le = dl->entity;
	uint8_t length;
	int rc;
	int rsl_msg;

	switch (LAPDm_CTRL_U_BITS(mctx->ctrl)) {
	case LAPDm_U_SABM:
		rsl_msg = RSL_MT_EST_IND;

		LOGP(DLLAPDM, LOGL_INFO, "SABM received\n");
		/* 5.7.1 */
		dl->seq_err_cond = 0;
		/* G.2.2 Wrong value of the C/R bit */
		if (LAPDm_ADDR_CR(mctx->addr) == le->cr.rem2loc.resp) {
			LOGP(DLLAPDM, LOGL_NOTICE, "SABM response error\n");
			msgb_free(msg);
			rsl_rll_error(RLL_CAUSE_FRM_UNIMPL, mctx);
			return -EINVAL;
		}

		length = msg->l2h[2] >> 2;
		/* G.4.5 If SABM is received with L>N201 or with M bit
		 * set, AN MDL-ERROR-INDICATION is sent to MM.
		 */
		if ((msg->l2h[2] & LAPDm_MORE) || length + 3 > mctx->n201) {
			LOGP(DLLAPDM, LOGL_NOTICE, "SABM too large error\n");
			msgb_free(msg);
			rsl_rll_error(RLL_CAUSE_UFRM_INC_PARAM, mctx);
			return -EIO;
		}

		/* Must be Format B */
		rc = check_length_ind(mctx, msg->l2h[2]);
		if (rc < 0) {
			msgb_free(msg);
			return rc;
		}
		switch (dl->state) {
		case LAPDm_STATE_IDLE:
			/* Set chan_nr and link_id for established connection */
			memset(&dl->mctx, 0, sizeof(dl->mctx));
			dl->mctx.dl = dl;
			dl->mctx.chan_nr = mctx->chan_nr;
			dl->mctx.link_id = mctx->link_id;
			dl->mctx.n201 = mctx->n201;
			break;
		case LAPDm_STATE_MF_EST:
			if (length == 0) {
				rsl_msg = RSL_MT_EST_CONF;
				break;
			}
			LOGP(DLLAPDM, LOGL_INFO, "SABM command, multiple "
				"frame established state\n");
			/* check for contention resoultion */
			if (dl->tx_hist[0][2] >> 2) {
				LOGP(DLLAPDM, LOGL_NOTICE, "SABM not allowed "
					"during contention resolution\n");
				rsl_rll_error(RLL_CAUSE_SABM_INFO_NOTALL, mctx);
			}
			msgb_free(msg);
			return 0;
		case LAPDm_STATE_DISC_SENT:
			/* 5.4.6.2 send DM with F=P */
			lapdm_send_dm(mctx);
			/* reset Timer T200 */
			osmo_timer_del(&dl->t200);
			msgb_free(msg);
			return send_rll_simple(RSL_MT_REL_CONF, mctx);
		default:
			lapdm_send_ua(mctx, length, msg->l2h + 3);
			msgb_free(msg);
			return 0;
		}
		/* send UA response */
		lapdm_send_ua(mctx, length, msg->l2h + 3);
		/* set Vs, Vr and Va to 0 */
		dl->V_send = dl->V_recv = dl->V_ack = 0;
		/* clear tx_hist */
		dl->tx_length[0] = 0;
		/* enter multiple-frame-established state */
		lapdm_dl_newstate(dl, LAPDm_STATE_MF_EST);
		/* send notification to L3 */
		if (length == 0) {
			/* 5.4.1.2 Normal establishment procedures */
			rc = send_rll_simple(rsl_msg, mctx);
			msgb_free(msg);
		} else {
			/* 5.4.1.4 Contention resolution establishment */
			msg->l3h = msg->l2h + 3;
			msgb_pull_l2h(msg);
			rc = send_rslms_rll_l3(rsl_msg, mctx, msg);
		}
		break;
	case LAPDm_U_DM:
		LOGP(DLLAPDM, LOGL_INFO, "DM received\n");
		/* G.2.2 Wrong value of the C/R bit */
		if (LAPDm_ADDR_CR(mctx->addr) == le->cr.rem2loc.cmd) {
			LOGP(DLLAPDM, LOGL_NOTICE, "DM command error\n");
			msgb_free(msg);
			rsl_rll_error(RLL_CAUSE_FRM_UNIMPL, mctx);
			return -EINVAL;
		}
		if (!LAPDm_CTRL_PF_BIT(mctx->ctrl)) {
			/* 5.4.1.2 DM responses with the F bit set to "0"
			 * shall be ignored.
			 */
			msgb_free(msg);
			return 0;
		}
		switch (dl->state) {
		case LAPDm_STATE_SABM_SENT:
			break;
		case LAPDm_STATE_MF_EST:
			if (LAPDm_CTRL_PF_BIT(mctx->ctrl) == 1) {
				LOGP(DLLAPDM, LOGL_INFO, "unsolicited DM "
					"response\n");
				rsl_rll_error(RLL_CAUSE_UNSOL_DM_RESP, mctx);
			} else {
				LOGP(DLLAPDM, LOGL_INFO, "unsolicited DM "
					"response, multiple frame established "
					"state\n");
				rsl_rll_error(RLL_CAUSE_UNSOL_DM_RESP_MF, mctx);
			}
			msgb_free(msg);
			return 0;
		case LAPDm_STATE_TIMER_RECOV:
			/* DM is normal in case PF = 1 */
			if (LAPDm_CTRL_PF_BIT(mctx->ctrl) == 0) {
				LOGP(DLLAPDM, LOGL_INFO, "unsolicited DM "
					"response, multiple frame established "
					"state\n");
				rsl_rll_error(RLL_CAUSE_UNSOL_DM_RESP_MF, mctx);
				msgb_free(msg);
				return 0;
			}
			break;
		case LAPDm_STATE_DISC_SENT:
			/* reset Timer T200 */
			osmo_timer_del(&dl->t200);
			/* go to idle state */
			lapdm_dl_flush_tx(dl);
			lapdm_dl_flush_send(dl);
			lapdm_dl_newstate(dl, LAPDm_STATE_IDLE);
			rc = send_rll_simple(RSL_MT_REL_CONF, mctx);
			msgb_free(msg);
			return 0;
		case LAPDm_STATE_IDLE:
			/* 5.4.5 all other frame types shall be discarded */
		default:
			LOGP(DLLAPDM, LOGL_INFO, "unsolicited DM response! "
				"(discarding)\n");
			msgb_free(msg);
			return 0;
		}
		/* reset T200 */
		osmo_timer_del(&dl->t200);
		rc = send_rll_simple(RSL_MT_REL_IND, mctx);
		msgb_free(msg);
		break;
	case LAPDm_U_UI:
		LOGP(DLLAPDM, LOGL_INFO, "UI received\n");
		/* G.2.2 Wrong value of the C/R bit */
		if (LAPDm_ADDR_CR(mctx->addr) == le->cr.rem2loc.resp) {
			LOGP(DLLAPDM, LOGL_NOTICE, "UI indicates response "
				"error\n");
			msgb_free(msg);
			rsl_rll_error(RLL_CAUSE_FRM_UNIMPL, mctx);
			return -EINVAL;
		}

		length = msg->l2h[2] >> 2;
		/* FIXME: G.4.5 If UI is received with L>N201 or with M bit
		 * set, AN MDL-ERROR-INDICATION is sent to MM.
		 */

		if (mctx->lapdm_fmt == LAPDm_FMT_B4) {
			length = N201_B4;
			msg->l3h = msg->l2h + 2;
		} else {
			rc = check_length_ind(mctx, msg->l2h[2]);
			if (rc < 0) {
				msgb_free(msg);
				return rc;
			}
			length = msg->l2h[2] >> 2;
			msg->l3h = msg->l2h + 3;
		}
		/* do some length checks */
		if (length == 0) {
			/* 5.3.3 UI frames received with the length indicator
			 * set to "0" shall be ignored
			 */
			LOGP(DLLAPDM, LOGL_INFO, "length=0 (discarding)\n");
			msgb_free(msg);
			return 0;
		}
		switch (LAPDm_ADDR_SAPI(mctx->addr)) {
		case LAPDm_SAPI_NORMAL:
		case LAPDm_SAPI_SMS:
			break;
		default:
			/* 5.3.3 UI frames with invalid SAPI values shall be
			 * discarded
			 */
			LOGP(DLLAPDM, LOGL_INFO, "sapi=%u (discarding)\n",
				LAPDm_ADDR_SAPI(mctx->addr));
			msgb_free(msg);
			return 0;
		}
		msgb_pull_l2h(msg);
		rc = send_rslms_rll_l3_ui(mctx, msg);
		break;
	case LAPDm_U_DISC:
		rsl_msg = RSL_MT_REL_IND;

		LOGP(DLLAPDM, LOGL_INFO, "DISC received\n");
		/* flush buffers */
		lapdm_dl_flush_tx(dl);
		lapdm_dl_flush_send(dl);
		/* 5.7.1 */
		dl->seq_err_cond = 0;
		/* G.2.2 Wrong value of the C/R bit */
		if (LAPDm_ADDR_CR(mctx->addr) == le->cr.rem2loc.resp) {
			LOGP(DLLAPDM, LOGL_NOTICE, "DISC response error\n");
			msgb_free(msg);
			rsl_rll_error(RLL_CAUSE_FRM_UNIMPL, mctx);
			return -EINVAL;
		}
		length = msg->l2h[2] >> 2;
		if (length > 0 || msg->l2h[2] & 0x02) {
			/* G.4.4 If a DISC or DM frame is received with L>0 or
			 * with the M bit set to "1", an MDL-ERROR-INDICATION
			 * primitive with cause "U frame with incorrect
			 * parameters" is sent to the mobile management entity.
			 */
			LOGP(DLLAPDM, LOGL_NOTICE,
				"U frame iwth incorrect parameters ");
			msgb_free(msg);
			rsl_rll_error(RLL_CAUSE_UFRM_INC_PARAM, mctx);
			return -EIO;
		}
		switch (dl->state) {
		case LAPDm_STATE_IDLE:
			LOGP(DLLAPDM, LOGL_INFO, "DISC in idle state\n");
			/* send DM with F=P */
			msgb_free(msg);
			return lapdm_send_dm(mctx);
		case LAPDm_STATE_SABM_SENT:
			LOGP(DLLAPDM, LOGL_INFO, "DISC in SABM state\n");
			/* 5.4.6.2 send DM with F=P */
			lapdm_send_dm(mctx);
			/* reset Timer T200 */
			osmo_timer_del(&dl->t200);
			msgb_free(msg);
			return send_rll_simple(RSL_MT_REL_IND, mctx);
		case LAPDm_STATE_MF_EST:
		case LAPDm_STATE_TIMER_RECOV:
			LOGP(DLLAPDM, LOGL_INFO, "DISC in est state\n");
			break;
		case LAPDm_STATE_DISC_SENT:
			LOGP(DLLAPDM, LOGL_INFO, "DISC in disc state\n");
			rsl_msg = RSL_MT_REL_CONF;
			break;
		default:
			lapdm_send_ua(mctx, length, msg->l2h + 3);
			msgb_free(msg);
			return 0;
		}
		/* send UA response */
		lapdm_send_ua(mctx, length, msg->l2h + 3);
		/* reset Timer T200 */
		osmo_timer_del(&dl->t200);
		/* enter idle state */
		lapdm_dl_flush_tx(dl);
		lapdm_dl_flush_send(dl);
		lapdm_dl_newstate(dl, LAPDm_STATE_IDLE);
		/* send notification to L3 */
		rc = send_rll_simple(rsl_msg, mctx);
		msgb_free(msg);
		break;
	case LAPDm_U_UA:
		LOGP(DLLAPDM, LOGL_INFO, "UA received\n");
		/* G.2.2 Wrong value of the C/R bit */
		if (LAPDm_ADDR_CR(mctx->addr) == le->cr.rem2loc.cmd) {
			LOGP(DLLAPDM, LOGL_NOTICE, "UA indicates command "
				"error\n");
			msgb_free(msg);
			rsl_rll_error(RLL_CAUSE_FRM_UNIMPL, mctx);
			return -EINVAL;
		}

		length = msg->l2h[2] >> 2;
		/* G.4.5 If UA is received with L>N201 or with M bit
		 * set, AN MDL-ERROR-INDICATION is sent to MM.
		 */
		if ((msg->l2h[2] & LAPDm_MORE) || length + 3 > mctx->n201) {
			LOGP(DLLAPDM, LOGL_NOTICE, "UA too large error\n");
			msgb_free(msg);
			rsl_rll_error(RLL_CAUSE_UFRM_INC_PARAM, mctx);
			return -EIO;
		}

		if (!LAPDm_CTRL_PF_BIT(mctx->ctrl)) {
			/* 5.4.1.2 A UA response with the F bit set to "0"
			 * shall be ignored.
			 */
			LOGP(DLLAPDM, LOGL_INFO, "F=0 (discarding)\n");
			msgb_free(msg);
			return 0;
		}
		switch (dl->state) {
		case LAPDm_STATE_SABM_SENT:
			break;
		case LAPDm_STATE_MF_EST:
		case LAPDm_STATE_TIMER_RECOV:
			LOGP(DLLAPDM, LOGL_INFO, "unsolicited UA response! "
				"(discarding)\n");
			rsl_rll_error(RLL_CAUSE_UNSOL_UA_RESP, mctx);
			msgb_free(msg);
			return 0;
		case LAPDm_STATE_DISC_SENT:
			LOGP(DLLAPDM, LOGL_INFO, "UA in disconnect state\n");
			/* reset Timer T200 */
			osmo_timer_del(&dl->t200);
			/* go to idle state */
			lapdm_dl_flush_tx(dl);
			lapdm_dl_flush_send(dl);
			lapdm_dl_newstate(dl, LAPDm_STATE_IDLE);
			rc = send_rll_simple(RSL_MT_REL_CONF, mctx);
			msgb_free(msg);
			return 0;
		case LAPDm_STATE_IDLE:
			/* 5.4.5 all other frame types shall be discarded */
		default:
			LOGP(DLLAPDM, LOGL_INFO, "unsolicited UA response! "
				"(discarding)\n");
			msgb_free(msg);
			return 0;
		}
		LOGP(DLLAPDM, LOGL_INFO, "UA in SABM state\n");
		/* reset Timer T200 */
		osmo_timer_del(&dl->t200);
		/* compare UA with SABME if contention resolution is applied */
		if (dl->tx_hist[0][2] >> 2) {
			rc = check_length_ind(mctx, msg->l2h[2]);
			if (rc < 0) {
				rc = send_rll_simple(RSL_MT_REL_IND, mctx);
				msgb_free(msg);
				/* go to idle state */
				lapdm_dl_flush_tx(dl);
				lapdm_dl_flush_send(dl);
				lapdm_dl_newstate(dl, LAPDm_STATE_IDLE);
				return 0;
			}
			length = msg->l2h[2] >> 2;
			if (length != (dl->tx_hist[0][2] >> 2)
			 || !!memcmp(dl->tx_hist[0] + 3, msg->l2h + 3,
			 		length)) {
				LOGP(DLLAPDM, LOGL_INFO, "**** UA response "
					"mismatches ****\n");
				rc = send_rll_simple(RSL_MT_REL_IND, mctx);
				msgb_free(msg);
				/* go to idle state */
				lapdm_dl_flush_tx(dl);
				lapdm_dl_flush_send(dl);
				lapdm_dl_newstate(dl, LAPDm_STATE_IDLE);
				return 0;
			}
		}
		/* set Vs, Vr and Va to 0 */
		dl->V_send = dl->V_recv = dl->V_ack = 0;
		/* clear tx_hist */
		dl->tx_length[0] = 0;
		/* enter multiple-frame-established state */
		lapdm_dl_newstate(dl, LAPDm_STATE_MF_EST);
		/* send outstanding frames, if any (resume / reconnect) */
		rslms_send_i(mctx, __LINE__);
		/* send notification to L3 */
		rc = send_rll_simple(RSL_MT_EST_CONF, mctx);
		msgb_free(msg);
		break;
	default:
		/* G.3.1 */
		LOGP(DLLAPDM, LOGL_NOTICE, "Unnumbered frame not allowed.\n");
		msgb_free(msg);
		rsl_rll_error(RLL_CAUSE_FRM_UNIMPL, mctx);
		return -EINVAL;
	}
	return rc;
}

/* Receive a LAPDm S (Supervisory) message from L1 */
static int lapdm_rx_s(struct msgb *msg, struct lapdm_msg_ctx *mctx)
{
	struct lapdm_datalink *dl = mctx->dl;
	struct lapdm_entity *le = dl->entity;
	uint8_t length;

	length = msg->l2h[2] >> 2;
	if (length > 0 || msg->l2h[2] & 0x02) {
		/* G.4.3 If a supervisory frame is received with L>0 or
		 * with the M bit set to "1", an MDL-ERROR-INDICATION
		 * primitive with cause "S frame with incorrect
		 * parameters" is sent to the mobile management entity. */
		LOGP(DLLAPDM, LOGL_NOTICE,
				"S frame with incorrect parameters\n");
		msgb_free(msg);
		rsl_rll_error(RLL_CAUSE_SFRM_INC_PARAM, mctx);
		return -EIO;
	}

	if (LAPDm_ADDR_CR(mctx->addr) == le->cr.rem2loc.resp
	 && LAPDm_CTRL_PF_BIT(mctx->ctrl)
	 && dl->state != LAPDm_STATE_TIMER_RECOV) {
		/* 5.4.2.2: Inidcate error on supervisory reponse F=1 */
		LOGP(DLLAPDM, LOGL_NOTICE, "S frame response with F=1 error\n");
		rsl_rll_error(RLL_CAUSE_UNSOL_SPRV_RESP, mctx);
	}

	switch (dl->state) {
	case LAPDm_STATE_IDLE:
		/* if P=1, respond DM with F=1 (5.2.2) */
		/* 5.4.5 all other frame types shall be discarded */
		if (LAPDm_CTRL_PF_BIT(mctx->ctrl))
			lapdm_send_dm(mctx); /* F=P */
		/* fall though */
	case LAPDm_STATE_SABM_SENT:
	case LAPDm_STATE_DISC_SENT:
		LOGP(DLLAPDM, LOGL_NOTICE, "S frame ignored in this state\n");
		msgb_free(msg);
		return 0;
	}
	switch (LAPDm_CTRL_S_BITS(mctx->ctrl)) {
	case LAPDm_S_RR:
		LOGP(DLLAPDM, LOGL_INFO, "RR received\n");
		/* 5.5.3.1: Acknowlege all tx frames up the the N(R)-1 */
		lapdm_acknowledge(mctx);

		/* 5.5.3.2 */
		if (LAPDm_ADDR_CR(mctx->addr) == le->cr.rem2loc.cmd
		 && LAPDm_CTRL_PF_BIT(mctx->ctrl)) {
		 	if (!dl->own_busy && !dl->seq_err_cond) {
				LOGP(DLLAPDM, LOGL_NOTICE, "RR frame command "
					"with polling bit set and we are not "
					"busy, so we reply with RR frame\n");
				lapdm_send_rr(mctx, 1);
				/* NOTE: In case of sequence error condition,
				 * the REJ frame has been transmitted when
				 * entering the condition, so it has not be
				 * done here
				 */
			} else if (dl->own_busy) {
				LOGP(DLLAPDM, LOGL_NOTICE, "RR frame command "
					"with polling bit set and we are busy, "
					"so we reply with RR frame\n");
				lapdm_send_rnr(mctx, 1);
			}
		} else if (LAPDm_ADDR_CR(mctx->addr) == le->cr.rem2loc.resp
			&& LAPDm_CTRL_PF_BIT(mctx->ctrl)
			&& dl->state == LAPDm_STATE_TIMER_RECOV) {
			LOGP(DLLAPDM, LOGL_INFO, "RR response with F==1, "
				"and we are in timer recovery state, so "
				"we leave that state\n");
			/* V(S) to the N(R) in the RR frame */
			dl->V_send = LAPDm_CTRL_Nr(mctx->ctrl);
			/* reset Timer T200 */
			osmo_timer_del(&dl->t200);
			/* 5.5.7 Clear timer recovery condition */
			lapdm_dl_newstate(dl, LAPDm_STATE_MF_EST);
		}
		/* Send message, if possible due to acknowledged data */
		rslms_send_i(mctx, __LINE__);

		break;
	case LAPDm_S_RNR:
		LOGP(DLLAPDM, LOGL_INFO, "RNR received\n");
		/* 5.5.3.1: Acknowlege all tx frames up the the N(R)-1 */
		lapdm_acknowledge(mctx);

		/* 5.5.5 */
		/* Set peer receiver busy condition */
		dl->peer_busy = 1;

		if (LAPDm_CTRL_PF_BIT(mctx->ctrl)) {
			if (LAPDm_ADDR_CR(mctx->addr) == le->cr.rem2loc.cmd) {
				if (!dl->own_busy) {
					LOGP(DLLAPDM, LOGL_INFO, "RNR poll "
						"command and we are not busy, "
						"so we reply with RR final "
						"response\n");
					/* Send RR with F=1 */
					lapdm_send_rr(mctx, 1);
				} else {
					LOGP(DLLAPDM, LOGL_INFO, "RNR poll "
						"command and we are busy, so "
						"we reply with RNR final "
						"response\n");
					/* Send RNR with F=1 */
					lapdm_send_rnr(mctx, 1);
				}
			} else if (dl->state == LAPDm_STATE_TIMER_RECOV) {
				LOGP(DLLAPDM, LOGL_INFO, "RNR poll response "
					"and we in timer recovery state, so "
					"we leave that state\n");
				/* 5.5.7 Clear timer recovery condition */
				lapdm_dl_newstate(dl, LAPDm_STATE_MF_EST);
				/* V(S) to the N(R) in the RNR frame */
				dl->V_send = LAPDm_CTRL_Nr(mctx->ctrl);
			}
		} else
			LOGP(DLLAPDM, LOGL_INFO, "RNR not polling/final state "
				"received\n");

		/* Send message, if possible due to acknowledged data */
		rslms_send_i(mctx, __LINE__);

		break;
	case LAPDm_S_REJ:
		LOGP(DLLAPDM, LOGL_INFO, "REJ received\n");
		/* 5.5.3.1: Acknowlege all tx frames up the the N(R)-1 */
		lapdm_acknowledge(mctx);

		/* 5.5.4.1 */
		if (dl->state != LAPDm_STATE_TIMER_RECOV) {
			/* Clear an existing peer receiver busy condition */
			dl->peer_busy = 0;
			/* V(S) and V(A) to the N(R) in the REJ frame */
			dl->V_send = dl->V_ack = LAPDm_CTRL_Nr(mctx->ctrl);
			/* reset Timer T200 */
			osmo_timer_del(&dl->t200);
			/* 5.5.3.2 */
			if (LAPDm_ADDR_CR(mctx->addr) == le->cr.rem2loc.cmd
			 && LAPDm_CTRL_PF_BIT(mctx->ctrl)) {
				if (!dl->own_busy && !dl->seq_err_cond) {
					LOGP(DLLAPDM, LOGL_INFO, "REJ poll "
						"command not in timer recovery "
						"state and not in own busy "
						"condition received, so we "
						"respond with RR final "
						"response\n");
					lapdm_send_rr(mctx, 1);
					/* NOTE: In case of sequence error
					 * condition, the REJ frame has been
					 * transmitted when entering the
					 * condition, so it has not be done
					 * here
				 	 */
				} else if (dl->own_busy) {
					LOGP(DLLAPDM, LOGL_INFO, "REJ poll "
						"command not in timer recovery "
						"state and in own busy "
						"condition received, so we "
						"respond with RNR final "
						"response\n");
					lapdm_send_rnr(mctx, 1);
				}
			} else
				LOGP(DLLAPDM, LOGL_INFO, "REJ response or not "
					"polling command not in timer recovery "
					"state received\n");
			/* send MDL ERROR INIDCATION to L3 */
			if (LAPDm_ADDR_CR(mctx->addr) == le->cr.rem2loc.resp
			 && LAPDm_CTRL_PF_BIT(mctx->ctrl)) {
				rsl_rll_error(RLL_CAUSE_UNSOL_SPRV_RESP, mctx);
			}

		} else if (LAPDm_ADDR_CR(mctx->addr) == le->cr.rem2loc.resp
			&& LAPDm_CTRL_PF_BIT(mctx->ctrl)) {
			LOGP(DLLAPDM, LOGL_INFO, "REJ poll response in timer "
				"recovery state received\n");
			/* Clear an existing peer receiver busy condition */
			dl->peer_busy = 0;
			/* 5.5.7 Clear timer recovery condition */
			lapdm_dl_newstate(dl, LAPDm_STATE_MF_EST);
			/* V(S) and V(A) to the N(R) in the REJ frame */
			dl->V_send = dl->V_ack = LAPDm_CTRL_Nr(mctx->ctrl);
			/* reset Timer T200 */
			osmo_timer_del(&dl->t200);
		} else {
			/* Clear an existing peer receiver busy condition */
			dl->peer_busy = 0;
			/* V(S) and V(A) to the N(R) in the REJ frame */
			dl->V_send = dl->V_ack = LAPDm_CTRL_Nr(mctx->ctrl);
			/* 5.5.3.2 */
			if (LAPDm_ADDR_CR(mctx->addr) == le->cr.rem2loc.cmd
			 && LAPDm_CTRL_PF_BIT(mctx->ctrl)) {
				if (!dl->own_busy && !dl->seq_err_cond) {
					LOGP(DLLAPDM, LOGL_INFO, "REJ poll "
						"command in timer recovery "
						"state and not in own busy "
						"condition received, so we "
						"respond with RR final "
						"response\n");
					lapdm_send_rr(mctx, 1);
					/* NOTE: In case of sequence error
					 * condition, the REJ frame has been
					 * transmitted when entering the
					 * condition, so it has not be done
					 * here
				 	 */
				} else if (dl->own_busy) {
					LOGP(DLLAPDM, LOGL_INFO, "REJ poll "
						"command in timer recovery "
						"state and in own busy "
						"condition received, so we "
						"respond with RNR final "
						"response\n");
					lapdm_send_rnr(mctx, 1);
				}
			} else
				LOGP(DLLAPDM, LOGL_INFO, "REJ response or not "
					"polling command in timer recovery "
					"state received\n");
		}

		/* FIXME: 5.5.4.2 2) */

		/* Send message, if possible due to acknowledged data */
		rslms_send_i(mctx, __LINE__);

		break;
	default:
		/* G.3.1 */
		LOGP(DLLAPDM, LOGL_NOTICE, "Supervisory frame not allowed.\n");
		msgb_free(msg);
		rsl_rll_error(RLL_CAUSE_FRM_UNIMPL, mctx);
		return -EINVAL;
	}
	msgb_free(msg);
	return 0;
}

/* Receive a LAPDm I (Information) message from L1 */
static int lapdm_rx_i(struct msgb *msg, struct lapdm_msg_ctx *mctx)
{
	struct lapdm_datalink *dl = mctx->dl;
	struct lapdm_entity *le = dl->entity;
	//uint8_t nr = LAPDm_CTRL_Nr(mctx->ctrl);
	uint8_t ns = LAPDm_CTRL_I_Ns(mctx->ctrl);
	uint8_t length;
	int rc;

	LOGP(DLLAPDM, LOGL_NOTICE, "I received\n");
		
	/* G.2.2 Wrong value of the C/R bit */
	if (LAPDm_ADDR_CR(mctx->addr) == le->cr.rem2loc.resp) {
		LOGP(DLLAPDM, LOGL_NOTICE, "I frame response not allowed\n");
		msgb_free(msg);
		rsl_rll_error(RLL_CAUSE_FRM_UNIMPL, mctx);
		return -EINVAL;
	}

	length = msg->l2h[2] >> 2;
	if (length == 0 || length + 3 > mctx->n201) {
		/* G.4.2 If the length indicator of an I frame is set
		 * to a numerical value L>N201 or L=0, an MDL-ERROR-INDICATION
		 * primitive with cause "I frame with incorrect length"
		 * is sent to the mobile management entity. */
		LOGP(DLLAPDM, LOGL_NOTICE, "I frame length not allowed\n");
		msgb_free(msg);
		rsl_rll_error(RLL_CAUSE_IFRM_INC_LEN, mctx);
		return -EIO;
	}

	/* G.4.2 If the numerical value of L is L<N201 and the M
	 * bit is set to "1", then an MDL-ERROR-INDICATION primitive with
	 * cause "I frame with incorrect use of M bit" is sent to the
	 * mobile management entity. */
	if ((msg->l2h[2] & LAPDm_MORE) && length + 3 < mctx->n201) {
		LOGP(DLLAPDM, LOGL_NOTICE, "I frame with M bit too short\n");
		msgb_free(msg);
		rsl_rll_error(RLL_CAUSE_IFRM_INC_MBITS, mctx);
		return -EIO;
	}

	switch (dl->state) {
	case LAPDm_STATE_IDLE:
		/* if P=1, respond DM with F=1 (5.2.2) */
		/* 5.4.5 all other frame types shall be discarded */
		if (LAPDm_CTRL_PF_BIT(mctx->ctrl))
			lapdm_send_dm(mctx); /* F=P */
		/* fall though */
	case LAPDm_STATE_SABM_SENT:
	case LAPDm_STATE_DISC_SENT:
		LOGP(DLLAPDM, LOGL_NOTICE, "I frame ignored in this state\n");
		msgb_free(msg);
		return 0;
	}

	/* 5.7.1: N(s) sequence error */
	if (ns != dl->V_recv) {
		LOGP(DLLAPDM, LOGL_NOTICE, "N(S) sequence error: N(S)=%u, "
		     "V(R)=%u\n", ns, dl->V_recv);
		/* discard data */
		msgb_free(msg);
		if (!dl->seq_err_cond) {
			/* FIXME: help me understand what exactly todo here
			dl->seq_err_cond = 1;
			*/
			lapdm_send_rej(mctx, LAPDm_CTRL_PF_BIT(mctx->ctrl));
		} else {
		}
		return -EIO;
	}
	dl->seq_err_cond = 0;

	/* Increment receiver state */
	dl->V_recv = inc_mod8(dl->V_recv);
	LOGP(DLLAPDM, LOGL_NOTICE, "incrementing V(R) to %u\n", dl->V_recv);

	/* 5.5.3.1: Acknowlege all transmitted frames up the the N(R)-1 */
	lapdm_acknowledge(mctx); /* V(A) is also set here */

	/* Only if we are not in own receiver busy condition */
	if (!dl->own_busy) {
		/* if the frame carries a complete segment */
		if (!(msg->l2h[2] & LAPDm_MORE)
		 && !dl->rcv_buffer) {
			LOGP(DLLAPDM, LOGL_INFO, "message in single I frame\n");
			/* send a DATA INDICATION to L3 */
			msg->l3h = msg->l2h + 3;
			msgb_pull_l2h(msg);
			msg->len = length;
			msg->tail = msg->data + length;
			rc = send_rslms_rll_l3(RSL_MT_DATA_IND, mctx, msg);
		} else {
			/* create rcv_buffer */
			if (!dl->rcv_buffer) {
				LOGP(DLLAPDM, LOGL_INFO, "message in multiple I "
					"frames (first message)\n");
				dl->rcv_buffer = msgb_alloc_headroom(200+56, 56,
								"LAPDm RX");
				dl->rcv_buffer->l3h = dl->rcv_buffer->data;
			}
			/* concat. rcv_buffer */
			if (msgb_l3len(dl->rcv_buffer) + length > 200) {
				LOGP(DLLAPDM, LOGL_NOTICE, "Received frame "
					"overflow!\n");
			} else {
				memcpy(msgb_put(dl->rcv_buffer, length),
					msg->l2h + 3, length);
			}
			/* if the last segment was received */
			if (!(msg->l2h[2] & LAPDm_MORE)) {
				LOGP(DLLAPDM, LOGL_INFO, "message in multiple I "
					"frames (last message)\n");
				rc = send_rslms_rll_l3(RSL_MT_DATA_IND, mctx,
					dl->rcv_buffer);
				dl->rcv_buffer = NULL;
			} else
				LOGP(DLLAPDM, LOGL_INFO, "message in multiple I "
					"frames (next message)\n");
			msgb_free(msg);

		}
	} else
		LOGP(DLLAPDM, LOGL_INFO, "I frame ignored during own receiver "
			"busy condition\n");

	/* Check for P bit */
	if (LAPDm_CTRL_PF_BIT(mctx->ctrl)) {
		/* 5.5.2.1 */
		/* check if we are not in own receiver busy */
		if (!dl->own_busy) {
			LOGP(DLLAPDM, LOGL_INFO, "we are not busy, send RR\n");
			/* Send RR with F=1 */
			rc = lapdm_send_rr(mctx, 1);
		} else {
			LOGP(DLLAPDM, LOGL_INFO, "we are busy, send RNR\n");
			/* Send RNR with F=1 */
			rc = lapdm_send_rnr(mctx, 1);
		}
	} else {
		/* 5.5.2.2 */
		/* check if we are not in own receiver busy */
		if (!dl->own_busy) {
			/* NOTE: V(R) is already set above */
			rc = rslms_send_i(mctx, __LINE__);
			if (rc) {
				LOGP(DLLAPDM, LOGL_INFO, "we are not busy and "
					"have no pending data, send RR\n");
				/* Send RR with F=0 */
				return lapdm_send_rr(mctx, 0);
			}
			/* all I or one RR is sent, we are done */
			return 0;
		} else {
			LOGP(DLLAPDM, LOGL_INFO, "we are busy, send RNR\n");
			/* Send RNR with F=0 */
			rc = lapdm_send_rnr(mctx, 0);
		}
	}

	/* Send message, if possible due to acknowledged data */
	rslms_send_i(mctx, __LINE__);

	return rc;
}

/* Receive a LAPDm message from L1 */
static int lapdm_ph_data_ind(struct msgb *msg, struct lapdm_msg_ctx *mctx)
{
	int rc;

	/* G.2.3 EA bit set to "0" is not allowed in GSM */
	if (!LAPDm_ADDR_EA(mctx->addr)) {
		LOGP(DLLAPDM, LOGL_NOTICE, "EA bit 0 is not allowed in GSM\n");
		msgb_free(msg);
		rsl_rll_error(RLL_CAUSE_FRM_UNIMPL, mctx);
		return -EINVAL;
	}

	if (LAPDm_CTRL_is_U(mctx->ctrl))
		rc = lapdm_rx_u(msg, mctx);
	else if (LAPDm_CTRL_is_S(mctx->ctrl))
		rc = lapdm_rx_s(msg, mctx);
	else if (LAPDm_CTRL_is_I(mctx->ctrl))
		rc = lapdm_rx_i(msg, mctx);
	else {
		LOGP(DLLAPDM, LOGL_NOTICE, "unknown LAPDm format\n");
		msgb_free(msg);
		rc = -EINVAL;
	}
	return rc;
}

/* input into layer2 (from layer 1) */
static int l2_ph_data_ind(struct msgb *msg, struct lapdm_entity *le, uint8_t chan_nr, uint8_t link_id)
{
	uint8_t cbits = chan_nr >> 3;
	uint8_t sapi; /* we cannot take SAPI from link_id, as L1 has no clue */
	struct lapdm_msg_ctx mctx;
	int rc = 0;

	/* when we reach here, we have a msgb with l2h pointing to the raw
	 * 23byte mac block. The l1h has already been purged. */

	mctx.chan_nr = chan_nr;
	mctx.link_id = link_id;
	mctx.addr = mctx.ctrl = 0;

	/* check for L1 chan_nr/link_id and determine LAPDm hdr format */
	if (cbits == 0x10 || cbits == 0x12) {
		/* Format Bbis is used on BCCH and CCCH(PCH, NCH and AGCH) */
		mctx.lapdm_fmt = LAPDm_FMT_Bbis;
		mctx.n201 = N201_Bbis;
		sapi = 0;
	} else {
		if (mctx.link_id & 0x40) {
			/* It was received from network on SACCH */

			/* If sent by BTS, lapdm_fmt must be B4 */
			if (le->mode == LAPDM_MODE_MS) {
				mctx.lapdm_fmt = LAPDm_FMT_B4;
				mctx.n201 = N201_B4;
				LOGP(DLLAPDM, LOGL_INFO, "fmt=B4\n");
			} else {
				mctx.lapdm_fmt = LAPDm_FMT_B;
				mctx.n201 = N201_AB_SACCH;
				LOGP(DLLAPDM, LOGL_INFO, "fmt=B\n");
			}
			/* SACCH frames have a two-byte L1 header that
			 * OsmocomBB L1 doesn't strip */
			mctx.tx_power_ind = msg->l2h[0] & 0x1f;
			mctx.ta_ind = msg->l2h[1];
			msgb_pull(msg, 2);
			msg->l2h += 2;
			sapi = (msg->l2h[0] >> 2) & 7;
		} else {
			mctx.lapdm_fmt = LAPDm_FMT_B;
			LOGP(DLLAPDM, LOGL_INFO, "fmt=B\n");
			mctx.n201 = 23; // FIXME: select correct size by chan.
			sapi = (msg->l2h[0] >> 2) & 7;
		}
	}

	mctx.dl = datalink_for_sapi(le, sapi);
	/* G.2.1 No action on frames containing an unallocated SAPI. */
	if (!mctx.dl) {
		LOGP(DLLAPDM, LOGL_NOTICE, "Received frame for unsupported "
			"SAPI %d!\n", sapi);
		msgb_free(msg);
		return -EIO;
	}

	switch (mctx.lapdm_fmt) {
	case LAPDm_FMT_A:
	case LAPDm_FMT_B:
	case LAPDm_FMT_B4:
		mctx.addr = msg->l2h[0];
		if (!(mctx.addr & 0x01)) {
			LOGP(DLLAPDM, LOGL_ERROR, "we don't support "
				"multibyte addresses (discarding)\n");
			msgb_free(msg);
			return -EINVAL;
		}
		mctx.ctrl = msg->l2h[1];
		/* obtain SAPI from address field */
		mctx.link_id |= LAPDm_ADDR_SAPI(mctx.addr);
		rc = lapdm_ph_data_ind(msg, &mctx);
		break;
	case LAPDm_FMT_Bter:
		/* FIXME */
		msgb_free(msg);
		break;
	case LAPDm_FMT_Bbis:
		/* directly pass up to layer3 */
		LOGP(DLLAPDM, LOGL_INFO, "fmt=Bbis UI\n");
		msg->l3h = msg->l2h;
		msgb_pull_l2h(msg);
		rc = send_rslms_rll_l3(RSL_MT_UNIT_DATA_IND, &mctx, msg);
		break;
	default:
		msgb_free(msg);
	}

	return rc;
}

/* input into layer2 (from layer 1) */
static int l2_ph_rach_ind(struct lapdm_entity *le, uint8_t ra, uint32_t fn, uint8_t acc_delay)
{
	struct abis_rsl_cchan_hdr *ch;
	struct gsm48_req_ref req_ref;
	struct gsm_time gt;
	struct msgb *msg = msgb_alloc_headroom(512, 64, "RSL CHAN RQD");

	msg->l2h = msgb_push(msg, sizeof(*ch));
	ch = (struct abis_rsl_cchan_hdr *)msg->l2h;
	rsl_init_cchan_hdr(ch, RSL_MT_CHAN_RQD);
	ch->chan_nr = RSL_CHAN_RACH;

	/* generate a RSL CHANNEL REQUIRED message */
	gsm_fn2gsmtime(&gt, fn);
	req_ref.ra = ra;
	req_ref.t1 = gt.t1; /* FIXME: modulo? */
	req_ref.t2 = gt.t2;
	req_ref.t3_low = gt.t3 & 7;
	req_ref.t3_high = gt.t3 >> 3;

	msgb_tv_fixed_put(msg, RSL_IE_REQ_REFERENCE, 3, (uint8_t *) &req_ref);
	msgb_tv_put(msg, RSL_IE_ACCESS_DELAY, acc_delay);

	return rslms_sendmsg(msg, le);
}

static int l2_ph_chan_conf(struct msgb *msg, struct lapdm_entity *le, uint32_t frame_nr);

/*! \brief Receive a PH-SAP primitive from L1 */
int lapdm_phsap_up(struct osmo_prim_hdr *oph, struct lapdm_entity *le)
{
	struct osmo_phsap_prim *pp = (struct osmo_phsap_prim *) oph;
	int rc = 0;

	if (oph->sap != SAP_GSM_PH) {
		LOGP(DLLAPDM, LOGL_ERROR, "primitive for unknown SAP %u\n",
			oph->sap);
		return -ENODEV;
	}

	switch (oph->primitive) {
	case PRIM_PH_DATA:
		if (oph->operation != PRIM_OP_INDICATION) {
			LOGP(DLLAPDM, LOGL_ERROR, "PH_DATA is not INDICATION %u\n",
				oph->operation);
			return -ENODEV;
		}
		rc = l2_ph_data_ind(oph->msg, le, pp->u.data.chan_nr,
				    pp->u.data.link_id);
		break;
	case PRIM_PH_RTS:
		if (oph->operation != PRIM_OP_INDICATION) {
			LOGP(DLLAPDM, LOGL_ERROR, "PH_RTS is not INDICATION %u\n",
				oph->operation);
			return -ENODEV;
		}
		rc = l2_ph_data_conf(oph->msg, le);
		break;
	case PRIM_PH_RACH:
		switch (oph->operation) {
		case PRIM_OP_INDICATION:
			rc = l2_ph_rach_ind(le, pp->u.rach_ind.ra, pp->u.rach_ind.fn,
					    pp->u.rach_ind.acc_delay);
			break;
		case PRIM_OP_CONFIRM:
			rc = l2_ph_chan_conf(oph->msg, le, pp->u.rach_ind.fn);
			break;
		default:
			return -EIO;
		}
		break;
	}

	return rc;
}


/* L3 -> L2 / RSLMS -> LAPDm */

/* L3 requests establishment of data link */
static int rslms_rx_rll_est_req(struct msgb *msg, struct lapdm_datalink *dl)
{
	struct lapdm_entity *le = dl->entity;
	struct abis_rsl_rll_hdr *rllh = msgb_l2(msg);
	uint8_t chan_nr = rllh->chan_nr;
	uint8_t link_id = rllh->link_id;
	uint8_t sapi = rllh->link_id & 7;
	struct tlv_parsed tv;
	uint8_t length;
	uint8_t n201 = 23; //FIXME

	/* Set chan_nr and link_id for established connection */
	memset(&dl->mctx, 0, sizeof(dl->mctx));
	dl->mctx.dl = dl;
	dl->mctx.n201 = n201;
	dl->mctx.chan_nr = chan_nr;
	dl->mctx.link_id = link_id;

	rsl_tlv_parse(&tv, rllh->data, msgb_l2len(msg)-sizeof(*rllh));
	if (TLVP_PRESENT(&tv, RSL_IE_L3_INFO)) {
		msg->l3h = TLVP_VAL(&tv, RSL_IE_L3_INFO);
		/* contention resolution establishment procedure */
		if (sapi != 0) {
			/* According to clause 6, the contention resolution
			 * procedure is only permitted with SAPI value 0 */
			LOGP(DLLAPDM, LOGL_ERROR, "SAPI != 0 but contention"
				"resolution (discarding)\n");
			msgb_free(msg);
			return send_rll_simple(RSL_MT_REL_IND, &dl->mctx);
		}
		/* transmit a SABM command with the P bit set to "1". The SABM
		 * command shall contain the layer 3 message unit */
		length = TLVP_LEN(&tv, RSL_IE_L3_INFO);
		LOGP(DLLAPDM, LOGL_INFO, "perform establishment with content "
			"(SABM)\n");
	} else {
		/* normal establishment procedure */
		length = 0;
		LOGP(DLLAPDM, LOGL_INFO, "perform normal establishm. (SABM)\n");
	}

	/* check if the layer3 message length exceeds N201 */
	if (length + 3 > 21) { /* FIXME: do we know the channel N201? */
		LOGP(DLLAPDM, LOGL_ERROR, "frame too large: %d > N201(%d) "
			"(discarding)\n", length + 3, 21);
		msgb_free(msg);
		return send_rll_simple(RSL_MT_REL_IND, &dl->mctx);
	}

	/* Flush send-queue */
	/* Clear send-buffer */
	lapdm_dl_flush_send(dl);

	/* Discard partly received L3 message */
	if (dl->rcv_buffer) {
		msgb_free(dl->rcv_buffer);
		dl->rcv_buffer = NULL;
	}

	/* Remove RLL header from msgb */
	msgb_pull_l2h(msg);

	/* Push LAPDm header on msgb */
	msg->l2h = msgb_push(msg, 3);
	msg->l2h[0] = LAPDm_ADDR(LAPDm_LPD_NORMAL, sapi, le->cr.loc2rem.cmd);
	msg->l2h[1] = LAPDm_CTRL_U(LAPDm_U_SABM, 1);
	msg->l2h[2] = LAPDm_LEN(length);
	/* Transmit-buffer carries exactly one segment */
	memcpy(dl->tx_hist[0], msg->l2h, 3 + length);
	dl->tx_length[0] = 3 + length;
	/* set Vs to 0, because it is used as index when resending SABM */
	dl->V_send = 0;
	
	/* Set states */
	dl->own_busy = dl->peer_busy = 0;
	dl->retrans_ctr = 0;
	lapdm_dl_newstate(dl, LAPDm_STATE_SABM_SENT);

	/* Tramsmit and start T200 */
	osmo_timer_schedule(&dl->t200, T200);
	return tx_ph_data_enqueue(dl, msg, chan_nr, link_id, n201);
}

/* L3 requests transfer of unnumbered information */
static int rslms_rx_rll_udata_req(struct msgb *msg, struct lapdm_datalink *dl)
{
	struct lapdm_entity *le = dl->entity;
	struct abis_rsl_rll_hdr *rllh = msgb_l2(msg);
	uint8_t chan_nr = rllh->chan_nr;
	uint8_t link_id = rllh->link_id;
	uint8_t sapi = link_id & 7;
	struct tlv_parsed tv;
	int length;
	uint8_t n201 = 23; //FIXME
	uint8_t ta = 0, tx_power = 0;

	/* check if the layer3 message length exceeds N201 */

	rsl_tlv_parse(&tv, rllh->data, msgb_l2len(msg)-sizeof(*rllh));

	if (TLVP_PRESENT(&tv, RSL_IE_TIMING_ADVANCE)) {
		ta = *TLVP_VAL(&tv, RSL_IE_TIMING_ADVANCE);
	}
	if (TLVP_PRESENT(&tv, RSL_IE_MS_POWER)) {
		tx_power = *TLVP_VAL(&tv, RSL_IE_MS_POWER);
	}
	if (!TLVP_PRESENT(&tv, RSL_IE_L3_INFO)) {
		LOGP(DLLAPDM, LOGL_ERROR, "unit data request without message "
			"error\n");
		msgb_free(msg);
		return -EINVAL;
	}
	msg->l3h = TLVP_VAL(&tv, RSL_IE_L3_INFO);
	length = TLVP_LEN(&tv, RSL_IE_L3_INFO);
	/* check if the layer3 message length exceeds N201 */
	if (length + 5 > 23) { /* FIXME: do we know the channel N201? */
		LOGP(DLLAPDM, LOGL_ERROR, "frame too large: %d > N201(%d) "
			"(discarding)\n", length + 5, 23);
		msgb_free(msg);
		return -EIO;
	}

	LOGP(DLLAPDM, LOGL_INFO, "sending unit data (tx_power=%d, ta=%d)\n",
		tx_power, ta);

	/* Remove RLL header from msgb */
	msgb_pull_l2h(msg);

	/* Push L1 + LAPDm header on msgb */
	msg->l2h = msgb_push(msg, 2 + 3);
	msg->l2h[0] = tx_power;
	msg->l2h[1] = ta;
	msg->l2h[2] = LAPDm_ADDR(LAPDm_LPD_NORMAL, sapi, le->cr.loc2rem.cmd);
	msg->l2h[3] = LAPDm_CTRL_U(LAPDm_U_UI, 0);
	msg->l2h[4] = LAPDm_LEN(length);
	// FIXME: short L2 header support

	/* Tramsmit */
	return tx_ph_data_enqueue(dl, msg, chan_nr, link_id, n201);
}

/* L3 requests transfer of acknowledged information */
static int rslms_rx_rll_data_req(struct msgb *msg, struct lapdm_datalink *dl)
{
	struct abis_rsl_rll_hdr *rllh = msgb_l2(msg);
	struct tlv_parsed tv;

	rsl_tlv_parse(&tv, rllh->data, msgb_l2len(msg)-sizeof(*rllh));
	if (!TLVP_PRESENT(&tv, RSL_IE_L3_INFO)) {
		LOGP(DLLAPDM, LOGL_ERROR, "data request without message "
			"error\n");
		msgb_free(msg);
		return -EINVAL;
	}
	msg->l3h = TLVP_VAL(&tv, RSL_IE_L3_INFO);

	LOGP(DLLAPDM, LOGL_INFO, "writing message to send-queue\n");

	/* Remove the RSL/RLL header */
	msgb_pull_l2h(msg);

	/* Write data into the send queue */
	msgb_enqueue(&dl->send_queue, msg);

	/* Send message, if possible */
	rslms_send_i(&dl->mctx, __LINE__);
	return 0;
}

/* Send next I frame from queued/buffered data */
static int rslms_send_i(struct lapdm_msg_ctx *mctx, int line)
{
	struct lapdm_datalink *dl = mctx->dl;
	struct lapdm_entity *le = dl->entity;
	uint8_t chan_nr = mctx->chan_nr;
	uint8_t link_id = mctx->link_id;
	uint8_t sapi = link_id & 7;
	int k = k_sapi[sapi];
	struct msgb *msg;
	int length, left;
	int rc = - 1; /* we sent nothing */

	LOGP(DLLAPDM, LOGL_INFO, "%s() called from line %d\n", __func__, line);

	next_frame:

	if (dl->peer_busy) {
		LOGP(DLLAPDM, LOGL_INFO, "peer busy, not sending\n");
		return rc;
	}

	if (dl->state == LAPDm_STATE_TIMER_RECOV) {
		LOGP(DLLAPDM, LOGL_INFO, "timer recovery, not sending\n");
		return rc;
	}

	/* If the send state variable V(S) is equal to V(A) plus k
	 * (where k is the maximum number of outstanding I frames - see
	 * subclause 5.8.4), the data link layer entity shall not transmit any
	 * new I frames, but shall retransmit an I frame as a result
	 * of the error recovery procedures as described in subclauses 5.5.4 and
	 * 5.5.7. */
	if (dl->V_send == add_mod8(dl->V_ack, k)) {
		LOGP(DLLAPDM, LOGL_INFO, "k frames outstanding, not sending "
			"more (k=%u V(S)=%u V(A)=%u)\n", k, dl->V_send,
			dl->V_ack);
		return rc;
	}

	/* if we have no tx_hist yet, we create it */
	if (!dl->tx_length[dl->V_send]) {
		/* Get next message into send-buffer, if any */
		if (!dl->send_buffer) {
			next_message:
			dl->send_out = 0;
			dl->send_buffer = msgb_dequeue(&dl->send_queue);
			/* No more data to be sent */
			if (!dl->send_buffer)
				return rc;
			LOGP(DLLAPDM, LOGL_INFO, "get message from "
				"send-queue\n");
		}

		/* How much is left in the send-buffer? */
		left = msgb_l3len(dl->send_buffer) - dl->send_out;
		/* Segment, if data exceeds N201 */
		length = left;
		if (length > mctx->n201 - 3)
			length = mctx->n201 - 3;
		LOGP(DLLAPDM, LOGL_INFO, "msg-len %d sent %d left %d N201 %d "
			"length %d first byte %02x\n",
			msgb_l3len(dl->send_buffer), dl->send_out, left,
			mctx->n201, length, dl->send_buffer->l3h[0]);
		/* If message in send-buffer is completely sent */
		if (left == 0) {
			msgb_free(dl->send_buffer);
			dl->send_buffer = NULL;
			goto next_message;
		}

		LOGP(DLLAPDM, LOGL_INFO, "send I frame %sV(S)=%d\n",
			(left > length) ? "segment " : "", dl->V_send);

		/* Create I frame (segment) and transmit-buffer content */
		msg = msgb_alloc_headroom(23+10, 10, "LAPDm I");
		msg->l2h = msgb_put(msg, 3 + length);
		msg->l2h[0] = LAPDm_ADDR(LAPDm_LPD_NORMAL, sapi, le->cr.loc2rem.cmd);
		msg->l2h[1] = LAPDm_CTRL_I(dl->V_recv, dl->V_send, 0);
		msg->l2h[2] = LAPDm_LEN(length);
		if (left > length)
			msg->l2h[2] |= LAPDm_MORE;
		memcpy(msg->l2h + 3, dl->send_buffer->l3h + dl->send_out,
			length);
		memcpy(dl->tx_hist[dl->V_send], msg->l2h, 3 + length);
		dl->tx_length[dl->V_send] = 3 + length;
		/* Add length to track how much is already in the tx buffer */
		dl->send_out += length;
	} else {
		LOGP(DLLAPDM, LOGL_INFO, "resend I frame from tx buffer "
			"V(S)=%d\n", dl->V_send);

		/* Create I frame (segment) from tx_hist */
		length = dl->tx_length[dl->V_send];
		msg = msgb_alloc_headroom(23+10, 10, "LAPDm I");
		msg->l2h = msgb_put(msg, length);
		memcpy(msg->l2h, dl->tx_hist[dl->V_send], length);
		msg->l2h[1] = LAPDm_CTRL_I(dl->V_recv, dl->V_send, 0);
	}

	/* The value of the send state variable V(S) shall be incremented by 1
	 * at the end of the transmission of the I frame */
	dl->V_send = inc_mod8(dl->V_send);

	/* If timer T200 is not running at the time right before transmitting a
	 * frame, when the PH-READY-TO-SEND primitive is received from the
	 * physical layer., it shall be set. */
	if (!osmo_timer_pending(&dl->t200))
		osmo_timer_schedule(&dl->t200, T200);

	tx_ph_data_enqueue(dl, msg, chan_nr, link_id, mctx->n201);

	rc = 0; /* we sent something */
	goto next_frame;
}

/* L3 requests suspension of data link */
static int rslms_rx_rll_susp_req(struct msgb *msg, struct lapdm_datalink *dl)
{
	struct abis_rsl_rll_hdr *rllh = msgb_l2(msg);
	uint8_t sapi = rllh->link_id & 7;

	if (sapi != 0) {
		LOGP(DLLAPDM, LOGL_ERROR, "SAPI != 0 while suspending\n");
		msgb_free(msg);
		return -EINVAL;
	}

	LOGP(DLLAPDM, LOGL_INFO, "perform suspension\n");

	/* put back the send-buffer to the send-queue (first position) */
	if (dl->send_buffer) {
		LOGP(DLLAPDM, LOGL_INFO, "put frame in sendbuffer back to "
			"queue\n");
		llist_add(&dl->send_buffer->list, &dl->send_queue);
		dl->send_buffer = NULL;
	} else
		LOGP(DLLAPDM, LOGL_INFO, "no frame in sendbuffer\n");

	/* Clear transmit buffer, but keep send buffer */
	lapdm_dl_flush_tx(dl);

	msgb_free(msg);

	return send_rll_simple(RSL_MT_SUSP_CONF, &dl->mctx);
}

/* L3 requests resume of data link */
static int rslms_rx_rll_res_req(struct msgb *msg, struct lapdm_datalink *dl)
{
	struct lapdm_entity *le = dl->entity;
	struct abis_rsl_rll_hdr *rllh = msgb_l2(msg);
	uint8_t chan_nr = rllh->chan_nr;
	uint8_t link_id = rllh->link_id;
	uint8_t sapi = rllh->link_id & 7;
	struct tlv_parsed tv;
	uint8_t length;
	uint8_t n201 = 23; //FIXME

	/* Set chan_nr and link_id for established connection */
	memset(&dl->mctx, 0, sizeof(dl->mctx));
	dl->mctx.dl = dl;
	dl->mctx.n201 = n201;
	dl->mctx.chan_nr = chan_nr;
	dl->mctx.link_id = link_id;

	rsl_tlv_parse(&tv, rllh->data, msgb_l2len(msg)-sizeof(*rllh));
	if (!TLVP_PRESENT(&tv, RSL_IE_L3_INFO)) {
		LOGP(DLLAPDM, LOGL_ERROR, "resume without message error\n");
		msgb_free(msg);
		return send_rll_simple(RSL_MT_REL_IND, &dl->mctx);
	}
	length = TLVP_LEN(&tv, RSL_IE_L3_INFO);

	LOGP(DLLAPDM, LOGL_INFO, "perform re-establishment (SABM) length=%d\n",
		length);
	
	/* Replace message in the send-buffer (reconnect) */
	if (dl->send_buffer)
		msgb_free(dl->send_buffer);
	dl->send_out = 0;
	if (length) {
		/* Remove the RSL/RLL header */
		msgb_pull_l2h(msg);
		/* Write data into the send buffer, to be sent first */
		dl->send_buffer = msg;
	}

	/* Discard partly received L3 message */
	if (dl->rcv_buffer) {
		msgb_free(dl->rcv_buffer);
		dl->rcv_buffer = NULL;
	}

	/* Create new msgb (old one is now free) */
	msg = msgb_alloc_headroom(23+10, 10, "LAPDm SABM");
	msg->l2h = msgb_put(msg, 3);
	msg->l2h[0] = LAPDm_ADDR(LAPDm_LPD_NORMAL, sapi, le->cr.loc2rem.cmd);
	msg->l2h[1] = LAPDm_CTRL_U(LAPDm_U_SABM, 1);
	msg->l2h[2] = LAPDm_LEN(0);
	/* Transmit-buffer carries exactly one segment */
	memcpy(dl->tx_hist[0], msg->l2h, 3);
	dl->tx_length[0] = 3;
	/* set Vs to 0, because it is used as index when resending SABM */
	dl->V_send = 0;

	/* Set states */
	dl->own_busy = dl->peer_busy = 0;
	dl->retrans_ctr = 0;
	lapdm_dl_newstate(dl, LAPDm_STATE_SABM_SENT);

	/* Tramsmit and start T200 */
	osmo_timer_schedule(&dl->t200, T200);
	return tx_ph_data_enqueue(dl, msg, chan_nr, link_id, n201);
}

/* L3 requests release of data link */
static int rslms_rx_rll_rel_req(struct msgb *msg, struct lapdm_datalink *dl)
{
	struct lapdm_entity *le = dl->entity;
	struct abis_rsl_rll_hdr *rllh = msgb_l2(msg);
	uint8_t chan_nr = rllh->chan_nr;
	uint8_t link_id = rllh->link_id;
	uint8_t sapi = rllh->link_id & 7;
	uint8_t mode = 0;

	/* get release mode */
	if (rllh->data[0] == RSL_IE_RELEASE_MODE)
		mode = rllh->data[1] & 1;

	/* local release */
	if (mode) {
		LOGP(DLLAPDM, LOGL_INFO, "perform local release\n");
		msgb_free(msg);
		/* reset Timer T200 */
		osmo_timer_del(&dl->t200);
		/* enter idle state */
		lapdm_dl_newstate(dl, LAPDm_STATE_IDLE);
		/* flush buffers */
		lapdm_dl_flush_tx(dl);
		lapdm_dl_flush_send(dl);
		/* send notification to L3 */
		return send_rll_simple(RSL_MT_REL_CONF, &dl->mctx);
	}

	/* in case we are already disconnecting */
	if (dl->state == LAPDm_STATE_DISC_SENT)
		return -EBUSY;

	LOGP(DLLAPDM, LOGL_INFO, "perform normal release (DISC)\n");

	/* Pull rllh */
	msgb_pull(msg, msg->tail - msg->l2h);

	/* Push LAPDm header on msgb */
	msg->l2h = msgb_push(msg, 3);
	msg->l2h[0] = LAPDm_ADDR(LAPDm_LPD_NORMAL, sapi, le->cr.loc2rem.cmd);
	msg->l2h[1] = LAPDm_CTRL_U(LAPDm_U_DISC, 1);
	msg->l2h[2] = LAPDm_LEN(0);
	/* Transmit-buffer carries exactly one segment */
	memcpy(dl->tx_hist[0], msg->l2h, 3);
	dl->tx_length[0] = 3;
	
	/* Set states */
	dl->own_busy = dl->peer_busy = 0;
	dl->retrans_ctr = 0;
	lapdm_dl_newstate(dl, LAPDm_STATE_DISC_SENT);

	/* Tramsmit and start T200 */
	osmo_timer_schedule(&dl->t200, T200);
	return tx_ph_data_enqueue(dl, msg, chan_nr, link_id, dl->mctx.n201);
}

/* L3 requests release in idle state */
static int rslms_rx_rll_rel_req_idle(struct msgb *msg, struct lapdm_datalink *dl)
{
	msgb_free(msg);

	/* send notification to L3 */
	return send_rll_simple(RSL_MT_REL_CONF, &dl->mctx);
}

/* L3 requests channel in idle state */
static int rslms_rx_chan_rqd(struct lapdm_channel *lc, struct msgb *msg)
{
	struct abis_rsl_cchan_hdr *cch = msgb_l2(msg);
	void *l1ctx = lc->lapdm_dcch.l1_ctx;
	struct osmo_phsap_prim pp;

	osmo_prim_init(&pp.oph, SAP_GSM_PH, PRIM_PH_RACH,
			PRIM_OP_REQUEST, NULL);

	if (msgb_l2len(msg) < sizeof(*cch) + 4 + 2 + 2) {
		LOGP(DLLAPDM, LOGL_ERROR, "Message too short for CHAN RQD!\n");
		return -EINVAL;
	}
	if (cch->data[0] != RSL_IE_REQ_REFERENCE) {
		LOGP(DLLAPDM, LOGL_ERROR, "Missing REQ REFERENCE IE\n");
		return -EINVAL;
	}
	pp.u.rach_req.ra = cch->data[1];
	pp.u.rach_req.offset = ((cch->data[2] & 0x7f) << 8) | cch->data[3];
	pp.u.rach_req.is_combined_ccch = cch->data[2] >> 7;

	if (cch->data[4] != RSL_IE_ACCESS_DELAY) {
		LOGP(DLLAPDM, LOGL_ERROR, "Missing ACCESS_DELAY IE\n");
		return -EINVAL;
	}
	/* TA = 0 - delay */
	pp.u.rach_req.ta = 0 - cch->data[5];

	if (cch->data[6] != RSL_IE_MS_POWER) {
		LOGP(DLLAPDM, LOGL_ERROR, "Missing MS POWER IE\n");
		return -EINVAL;
	}
	pp.u.rach_req.tx_power = cch->data[7];

	msgb_free(msg);

	return lc->lapdm_dcch.l1_prim_cb(&pp.oph, l1ctx);
}

/* L1 confirms channel request */
static int l2_ph_chan_conf(struct msgb *msg, struct lapdm_entity *le, uint32_t frame_nr)
{
	struct abis_rsl_cchan_hdr *ch;
	struct gsm_time tm;
	struct gsm48_req_ref *ref;

	gsm_fn2gsmtime(&tm, frame_nr);

	msgb_pull_l2h(msg);
	msg->l2h = msgb_push(msg, sizeof(*ch) + sizeof(*ref));
	ch = (struct abis_rsl_cchan_hdr *)msg->l2h;
	rsl_init_cchan_hdr(ch, RSL_MT_CHAN_CONF);
	ch->chan_nr = RSL_CHAN_RACH;
	ch->data[0] = RSL_IE_REQ_REFERENCE;
	ref = (struct gsm48_req_ref *) (ch->data + 1);
	ref->t1 = tm.t1;
	ref->t2 = tm.t2;
	ref->t3_low = tm.t3 & 0x7;
	ref->t3_high = tm.t3 >> 3;
	
	return rslms_sendmsg(msg, le);
}

const char *lapdm_state_names[] = {
	"LAPDm_STATE_NULL",
	"LAPDm_STATE_IDLE",
	"LAPDm_STATE_SABM_SENT",
	"LAPDm_STATE_MF_EST",
	"LAPDm_STATE_TIMER_RECOV",
	"LAPDm_STATE_DISC_SENT",
};

/* statefull handling for RSLms RLL messages from L3 */
static struct l2downstate {
	uint32_t	states;
	int		type;
	int		(*rout) (struct msgb *msg, struct lapdm_datalink *dl);
} l2downstatelist[] = {
	/* create and send UI command */
	{ALL_STATES,
	 RSL_MT_UNIT_DATA_REQ, rslms_rx_rll_udata_req},

	/* create and send SABM command */
	{SBIT(LAPDm_STATE_IDLE),
	 RSL_MT_EST_REQ, rslms_rx_rll_est_req},

	/* create and send I command */
	{SBIT(LAPDm_STATE_MF_EST) |
	 SBIT(LAPDm_STATE_TIMER_RECOV),
	 RSL_MT_DATA_REQ, rslms_rx_rll_data_req},

	/* suspend datalink */
	{SBIT(LAPDm_STATE_MF_EST) |
	 SBIT(LAPDm_STATE_TIMER_RECOV),
	 RSL_MT_SUSP_REQ, rslms_rx_rll_susp_req},

	/* create and send SABM command (resume) */
	{SBIT(LAPDm_STATE_MF_EST) |
	 SBIT(LAPDm_STATE_TIMER_RECOV),
	 RSL_MT_RES_REQ, rslms_rx_rll_res_req},

	/* create and send SABM command (reconnect) */
	{SBIT(LAPDm_STATE_IDLE) |
	 SBIT(LAPDm_STATE_MF_EST) |
	 SBIT(LAPDm_STATE_TIMER_RECOV),
	 RSL_MT_RECON_REQ, rslms_rx_rll_res_req},

	/* create and send DISC command */
	{SBIT(LAPDm_STATE_SABM_SENT) |
	 SBIT(LAPDm_STATE_MF_EST) |
	 SBIT(LAPDm_STATE_TIMER_RECOV) |
	 SBIT(LAPDm_STATE_DISC_SENT),
	 RSL_MT_REL_REQ, rslms_rx_rll_rel_req},

	/* release in idle state */
	{SBIT(LAPDm_STATE_IDLE),
	 RSL_MT_REL_REQ, rslms_rx_rll_rel_req_idle},
};

#define L2DOWNSLLEN \
	(sizeof(l2downstatelist) / sizeof(struct l2downstate))

/* incoming RSLms RLL message from L3 */
static int rslms_rx_rll(struct msgb *msg, struct lapdm_channel *lc)
{
	struct abis_rsl_rll_hdr *rllh = msgb_l2(msg);
	int msg_type = rllh->c.msg_type;
	uint8_t sapi = rllh->link_id & 7;
	struct lapdm_entity *le;
	struct lapdm_datalink *dl;
	int i, supported = 0;
	int rc = 0;

	if (msgb_l2len(msg) < sizeof(*rllh)) {
		LOGP(DLLAPDM, LOGL_ERROR, "Message too short for RLL hdr!\n");
		return -EINVAL;
	}

	if (rllh->link_id & 0x40)
		le = &lc->lapdm_acch;
	else
		le = &lc->lapdm_dcch;

	/* G.2.1 No action schall be taken on frames containing an unallocated
	 * SAPI.
	 */
	dl = datalink_for_sapi(le, sapi);
	if (!dl) {
		LOGP(DLLAPDM, LOGL_ERROR, "No instance for SAPI %d!\n", sapi);
		return -EINVAL;
	}

	LOGP(DLLAPDM, LOGL_INFO, "(%p) RLL Message '%s' received in state %s\n",
		lc->name, rsl_msg_name(msg_type), lapdm_state_names[dl->state]);

	/* find function for current state and message */
	for (i = 0; i < L2DOWNSLLEN; i++) {
		if (msg_type == l2downstatelist[i].type)
			supported = 1;
		if ((msg_type == l2downstatelist[i].type)
		 && ((1 << dl->state) & l2downstatelist[i].states))
			break;
	}
	if (!supported) {
		LOGP(DLLAPDM, LOGL_NOTICE, "Message unsupported.\n");
		msgb_free(msg);
		return 0;
	}
	if (i == L2DOWNSLLEN) {
		LOGP(DLLAPDM, LOGL_NOTICE, "Message unhandled at this state.\n");
		msgb_free(msg);
		return 0;
	}

	rc = l2downstatelist[i].rout(msg, dl);

	return rc;
}

/* incoming RSLms COMMON CHANNEL message from L3 */
static int rslms_rx_com_chan(struct msgb *msg, struct lapdm_channel *lc)
{
	struct abis_rsl_cchan_hdr *cch = msgb_l2(msg);
	int msg_type = cch->c.msg_type;
	int rc = 0;

	if (msgb_l2len(msg) < sizeof(*cch)) {
		LOGP(DLLAPDM, LOGL_ERROR, "Message too short for COM CHAN hdr!\n");
		return -EINVAL;
	}

	switch (msg_type) {
	case RSL_MT_CHAN_RQD:
		/* create and send RACH request */
		rc = rslms_rx_chan_rqd(lc, msg);
		break;
	default:
		LOGP(DLLAPDM, LOGL_NOTICE, "Unknown COMMON CHANNEL msg %d!\n",
			msg_type);
		msgb_free(msg);
		return 0;
	}

	return rc;
}

/*! \brief Receive a RSLms \ref msgb from Layer 3 */
int lapdm_rslms_recvmsg(struct msgb *msg, struct lapdm_channel *lc)
{
	struct abis_rsl_common_hdr *rslh = msgb_l2(msg);
	int rc = 0;

	if (msgb_l2len(msg) < sizeof(*rslh)) {
		LOGP(DLLAPDM, LOGL_ERROR, "Message too short RSL hdr!\n");
		return -EINVAL;
	}

	switch (rslh->msg_discr & 0xfe) {
	case ABIS_RSL_MDISC_RLL:
		rc = rslms_rx_rll(msg, lc);
		break;
	case ABIS_RSL_MDISC_COM_CHAN:
		rc = rslms_rx_com_chan(msg, lc);
		break;
	default:
		LOGP(DLLAPDM, LOGL_ERROR, "unknown RSLms message "
			"discriminator 0x%02x", rslh->msg_discr);
		msgb_free(msg);
		return -EINVAL;
	}

	return rc;
}

/*! \brief Set the \ref lapdm_mode of a LAPDm entity */
int lapdm_entity_set_mode(struct lapdm_entity *le, enum lapdm_mode mode)
{
	switch (mode) {
	case LAPDM_MODE_MS:
		le->cr.loc2rem.cmd = CR_MS2BS_CMD;
		le->cr.loc2rem.resp = CR_MS2BS_RESP;
		le->cr.rem2loc.cmd = CR_BS2MS_CMD;
		le->cr.rem2loc.resp = CR_BS2MS_RESP;
		break;
	case LAPDM_MODE_BTS:
		le->cr.loc2rem.cmd = CR_BS2MS_CMD;
		le->cr.loc2rem.resp = CR_BS2MS_RESP;
		le->cr.rem2loc.cmd = CR_MS2BS_CMD;
		le->cr.rem2loc.resp = CR_MS2BS_RESP;
		break;
	default:
		return -EINVAL;
	}

	le->mode = mode;

	return 0;
}

/*! \brief Set the \ref lapdm_mode of a LAPDm channel*/
int lapdm_channel_set_mode(struct lapdm_channel *lc, enum lapdm_mode mode)
{
	int rc;

	rc = lapdm_entity_set_mode(&lc->lapdm_dcch, mode);
	if (rc < 0)
		return rc;

	return lapdm_entity_set_mode(&lc->lapdm_acch, mode);
}

/*! \brief Set the L1 callback and context of a LAPDm channel */
void lapdm_channel_set_l1(struct lapdm_channel *lc, osmo_prim_cb cb, void *ctx)
{
	lc->lapdm_dcch.l1_prim_cb = cb;
	lc->lapdm_acch.l1_prim_cb = cb;
	lc->lapdm_dcch.l1_ctx = ctx;
	lc->lapdm_acch.l1_ctx = ctx;
}

/*! \brief Set the L3 callback and context of a LAPDm channel */
void lapdm_channel_set_l3(struct lapdm_channel *lc, lapdm_cb_t cb, void *ctx)
{
	lc->lapdm_dcch.l3_cb = cb;
	lc->lapdm_acch.l3_cb = cb;
	lc->lapdm_dcch.l3_ctx = ctx;
	lc->lapdm_acch.l3_ctx = ctx;
}

/*! \brief Reset an entire LAPDm entity and all its datalinks */
void lapdm_entity_reset(struct lapdm_entity *le)
{
	struct lapdm_datalink *dl;
	int i;

	for (i = 0; i < ARRAY_SIZE(le->datalink); i++) {
		dl = &le->datalink[i];
		if (dl->state == LAPDm_STATE_IDLE)
			continue;
		LOGP(DLLAPDM, LOGL_INFO, "Resetting LAPDm instance\n");
		/* reset Timer T200 */
		osmo_timer_del(&dl->t200);
		/* enter idle state */
		dl->state = LAPDm_STATE_IDLE;
		/* flush buffer */
		lapdm_dl_flush_tx(dl);
		lapdm_dl_flush_send(dl);
	}
}

/*! \brief Reset a LAPDm channel with all its entities */
void lapdm_channel_reset(struct lapdm_channel *lc)
{
	lapdm_entity_reset(&lc->lapdm_dcch);
	lapdm_entity_reset(&lc->lapdm_acch);
}

/*! \brief Set the flags of a LAPDm entity */
void lapdm_entity_set_flags(struct lapdm_entity *le, unsigned int flags)
{
	le->flags = flags;
}

/*! \brief Set the flags of all LAPDm entities in a LAPDm channel */
void lapdm_channel_set_flags(struct lapdm_channel *lc, unsigned int flags)
{
	lapdm_entity_set_flags(&lc->lapdm_dcch, flags);
	lapdm_entity_set_flags(&lc->lapdm_acch, flags);
}

/*! }@ */
