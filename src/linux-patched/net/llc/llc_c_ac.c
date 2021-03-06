/*
 * llc_c_ac.c - actions performed during connection state transition.
 *
 * Description:
 *   Functions in this module are implementation of connection component actions
 *   Details of actions can be found in IEEE-802.2 standard document.
 *   All functions have one connection and one event as input argument. All of
 *   them return 0 On success and 1 otherwise.
 *
 * Copyright (c) 1997 by Procom Technology, Inc.
 * 		 2001 by Arnaldo Carvalho de Melo <acme@conectiva.com.br>
 *
 * This program can be redistributed or modified under the terms of the
 * GNU General Public License as published by the Free Software Foundation.
 * This program is distributed without any warranty or implied warranty
 * of merchantability or fitness for a particular purpose.
 *
 * See the GNU General Public License for more details.
 */
#include <linux/netdevice.h>
#include <net/llc_conn.h>
#include <net/llc_sap.h>
#include <net/sock.h>
#include <net/llc_main.h>
#include <net/llc_c_ev.h>
#include <net/llc_c_ac.h>
#include <net/llc_c_st.h>
#include <net/llc_pdu.h>
#include <net/llc_mac.h>

static void llc_conn_pf_cycle_tmr_cb(unsigned long timeout_data);
static void llc_conn_ack_tmr_cb(unsigned long timeout_data);
static void llc_conn_rej_tmr_cb(unsigned long timeout_data);
static void llc_conn_busy_tmr_cb(unsigned long timeout_data);
static int llc_conn_ac_inc_vs_by_1(struct sock *sk,
				   struct llc_conn_state_ev *ev);
static void llc_process_tmr_ev(struct sock *sk, struct llc_conn_state_ev *ev);
static int llc_conn_ac_data_confirm(struct sock *sk,
				    struct llc_conn_state_ev *ev);

#define INCORRECT 0

int llc_conn_ac_clear_remote_busy(struct sock *sk, struct llc_conn_state_ev *ev)
{
	struct llc_opt *llc = llc_sk(sk);

	if (llc->remote_busy_flag) {
		u8 nr;
		llc_pdu_sn_t *rx_pdu = (llc_pdu_sn_t *)ev->data.pdu.skb->nh.raw;

		llc->remote_busy_flag = 0;
		del_timer(&llc->busy_state_timer.timer);
		llc->busy_state_timer.running = 0;
		nr = LLC_I_GET_NR(rx_pdu);
		llc_conn_resend_i_pdu_as_cmd(sk, nr, 0);
	}
	return 0;
}

int llc_conn_ac_conn_ind(struct sock *sk, struct llc_conn_state_ev *ev)
{
	int rc = 1;
	struct sk_buff *skb = ev->data.pdu.skb;
	union llc_u_prim_data *prim_data = llc_ind_prim.data;
	struct llc_prim_if_block *prim = &llc_ind_prim;
	struct llc_sap *sap;
	struct llc_opt *llc = llc_sk(sk);

	llc_pdu_decode_dsap(skb, &prim_data->conn.daddr.lsap);
	sap = llc_sap_find(prim_data->conn.daddr.lsap);
	if (sap) {
		llc_pdu_decode_sa(skb, llc->daddr.mac);
		llc_pdu_decode_da(skb, llc->laddr.mac);
		llc->dev = skb->dev;
		prim_data->conn.pri = 0;
		prim_data->conn.sk  = sk;
		prim_data->conn.dev = skb->dev;
		memcpy(&prim_data->conn.daddr, &llc->laddr, sizeof(llc->laddr));
		memcpy(&prim_data->conn.saddr, &llc->daddr, sizeof(llc->daddr));
		prim->data   = prim_data;
		prim->prim   = LLC_CONN_PRIM;
		prim->sap    = llc->sap;
		ev->flag     = 1;
		ev->ind_prim = prim;
		rc = 0;
	}
	return rc;
}

int llc_conn_ac_conn_confirm(struct sock *sk, struct llc_conn_state_ev *ev)
{
	union llc_u_prim_data *prim_data = llc_cfm_prim.data;
	struct sk_buff *skb = ev->data.pdu.skb;
	/* FIXME: wtf, this is global, so the whole thing is really
	 *	  non reentrant...
	 */
	struct llc_prim_if_block *prim = &llc_cfm_prim;
	struct llc_opt *llc = llc_sk(sk);
	struct llc_sap *sap = llc->sap;

	prim_data->conn.sk     = sk;
	prim_data->conn.pri    = 0;
	prim_data->conn.status = ev->status;
	prim_data->conn.link   = llc->link;
	if (skb)
		prim_data->conn.dev    = skb->dev;
	else
		printk(KERN_ERR __FUNCTION__ "ev->data.pdu.skb == NULL\n");
	prim->data   = prim_data;
	prim->prim   = LLC_CONN_PRIM;
	prim->sap    = sap;
	ev->flag     = 1;
	ev->cfm_prim = prim;
	return 0;
}

static int llc_conn_ac_data_confirm(struct sock *sk,
				    struct llc_conn_state_ev *ev)
{
	struct llc_prim_if_block *prim = &llc_cfm_prim;
	union llc_u_prim_data *prim_data = llc_cfm_prim.data;

	prim_data->data.sk     = sk;
	prim_data->data.pri    = 0;
	prim_data->data.link   = llc_sk(sk)->link;
	prim_data->data.status = LLC_STATUS_RECEIVED;
	prim_data->data.skb    = NULL;
	prim->data	       = prim_data;
	prim->prim	       = LLC_DATA_PRIM;
	prim->sap	       = llc_sk(sk)->sap;
	ev->flag	       = 1;
	ev->cfm_prim	       = prim;
	return 0;
}

int llc_conn_ac_data_ind(struct sock *sk, struct llc_conn_state_ev *ev)
{
	llc_conn_rtn_pdu(sk, ev->data.pdu.skb, ev);
	return 0;
}

int llc_conn_ac_disc_ind(struct sock *sk, struct llc_conn_state_ev *ev)
{
	u8 reason = 0;
	int rc = 1;
	union llc_u_prim_data *prim_data = llc_ind_prim.data;
	struct llc_prim_if_block *prim = &llc_ind_prim;

	if (ev->type == LLC_CONN_EV_TYPE_PDU) {
		llc_pdu_un_t *rx_pdu = (llc_pdu_un_t *)ev->data.pdu.skb->nh.raw;

		if (!LLC_PDU_IS_RSP(rx_pdu) &&
		    !LLC_PDU_TYPE_IS_U(rx_pdu) &&
		    LLC_U_PDU_RSP(rx_pdu) == LLC_2_PDU_RSP_DM) {
			reason = LLC_DISC_REASON_RX_DM_RSP_PDU;
			rc = 0;
		} else if (!LLC_PDU_IS_CMD(rx_pdu) &&
			   !LLC_PDU_TYPE_IS_U(rx_pdu) &&
			   LLC_U_PDU_CMD(rx_pdu) == LLC_2_PDU_CMD_DISC) {
			reason = LLC_DISC_REASON_RX_DISC_CMD_PDU;
			rc = 0;
		}
	} else if (ev->type == LLC_CONN_EV_TYPE_ACK_TMR) {
		reason = LLC_DISC_REASON_ACK_TMR_EXP;
		rc = 0;
	} else {
		reason = 0;
		rc = 1;
	}
	if (!rc) {
		prim_data->disc.sk     = sk;
		prim_data->disc.reason = reason;
		prim_data->disc.link   = llc_sk(sk)->link;
		prim->data	       = prim_data;
		prim->prim	       = LLC_DISC_PRIM;
		prim->sap	       = llc_sk(sk)->sap;
		ev->flag	       = 1;
		ev->ind_prim	       = prim;
	}
	return rc;
}

int llc_conn_ac_disc_confirm(struct sock *sk, struct llc_conn_state_ev *ev)
{
	union llc_u_prim_data *prim_data = llc_cfm_prim.data;
	struct llc_prim_if_block *prim = &llc_cfm_prim;

	prim_data->disc.sk     = sk;
	prim_data->disc.reason = ev->status;
	prim_data->disc.link   = llc_sk(sk)->link;
	prim->data	       = prim_data;
	prim->prim	       = LLC_DISC_PRIM;
	prim->sap	       = llc_sk(sk)->sap;
	ev->flag	       = 1;
	ev->cfm_prim	       = prim;
	return 0;
}

int llc_conn_ac_rst_ind(struct sock *sk, struct llc_conn_state_ev *ev)
{
	u8 reason = 0;
	int rc = 1;
	llc_pdu_un_t *rx_pdu = (llc_pdu_un_t *)ev->data.pdu.skb->nh.raw;
	union llc_u_prim_data *prim_data = llc_ind_prim.data;
	struct llc_prim_if_block *prim = &llc_ind_prim;
	struct llc_opt *llc = llc_sk(sk);

	switch (ev->type) {
		case LLC_CONN_EV_TYPE_PDU:
			if (!LLC_PDU_IS_RSP(rx_pdu) &&
			    !LLC_PDU_TYPE_IS_U(rx_pdu) &&
			    LLC_U_PDU_RSP(rx_pdu) == LLC_2_PDU_RSP_FRMR) {
				reason = LLC_RESET_REASON_LOCAL;
				rc = 0;
			} else if (!LLC_PDU_IS_CMD(rx_pdu) &&
				   !LLC_PDU_TYPE_IS_U(rx_pdu) &&
				   LLC_U_PDU_CMD(rx_pdu) ==
				   			LLC_2_PDU_CMD_SABME) {
				reason = LLC_RESET_REASON_REMOTE;
				rc = 0;
			} else {
				reason = 0;
				rc  = 1;
			}
			break;
		case LLC_CONN_EV_TYPE_ACK_TMR:
		case LLC_CONN_EV_TYPE_P_TMR:
		case LLC_CONN_EV_TYPE_REJ_TMR:
		case LLC_CONN_EV_TYPE_BUSY_TMR:
			if (llc->retry_count > llc->n2) {
				reason = LLC_RESET_REASON_LOCAL;
				rc = 0;
			} else
				rc = 1;
			break;
	}
	if (!rc) {
		prim_data->res.sk     = sk;
		prim_data->res.reason = reason;
		prim_data->res.link   = llc->link;
		prim->data	      = prim_data;
		prim->prim	      = LLC_RESET_PRIM;
		prim->sap	      = llc->sap;
		ev->flag	      = 1;
		ev->ind_prim	      = prim;
	}
	return rc;
}

int llc_conn_ac_rst_confirm(struct sock *sk, struct llc_conn_state_ev *ev)
{
	union llc_u_prim_data *prim_data = llc_cfm_prim.data;
	struct llc_prim_if_block *prim = &llc_cfm_prim;

	prim_data->res.sk   = sk;
	prim_data->res.link = llc_sk(sk)->link;
	prim->data	    = prim_data;
	prim->prim	    = LLC_RESET_PRIM;
	prim->sap	    = llc_sk(sk)->sap;
	ev->flag	    = 1;
	ev->cfm_prim	    = prim;
	return 0;
}

int llc_conn_ac_report_status(struct sock *sk, struct llc_conn_state_ev *ev)
{
	return 0;
}

int llc_conn_ac_clear_remote_busy_if_f_eq_1(struct sock *sk,
					struct llc_conn_state_ev *ev)
{
	llc_pdu_sn_t *rx_pdu = (llc_pdu_sn_t *)ev->data.pdu.skb->nh.raw;

	if (!LLC_PDU_IS_RSP(rx_pdu) &&
	    !LLC_PDU_TYPE_IS_I(rx_pdu) &&
	    !LLC_I_PF_IS_1(rx_pdu) && llc_sk(sk)->ack_pf)
		llc_conn_ac_clear_remote_busy(sk, ev);
	return 0;
}

int llc_conn_ac_stop_rej_tmr_if_data_flag_eq_2(struct sock *sk,
						 struct llc_conn_state_ev *ev)
{
	if (llc_sk(sk)->data_flag == 2) {
		del_timer(&llc_sk(sk)->rej_sent_timer.timer);
		llc_sk(sk)->rej_sent_timer.running = 0;
	}
	return 0;
}

int llc_conn_ac_send_disc_cmd_p_set_x(struct sock *sk,
				      struct llc_conn_state_ev *ev)
{
	int rc = 1;
	struct sk_buff *skb = llc_alloc_frame();

	if (skb) {
		u8 p_bit = 1;
		struct llc_opt *llc = llc_sk(sk);
		struct llc_sap *sap = llc->sap;

		skb->dev = llc->dev;
		llc_pdu_header_init(skb, LLC_PDU_TYPE_U, sap->laddr.lsap,
				    llc->daddr.lsap, LLC_PDU_CMD);
		llc_pdu_init_as_disc_cmd(skb, p_bit);
		lan_hdrs_init(skb, llc->dev->dev_addr, llc->daddr.mac);
		rc = 0;
		llc_conn_send_pdu(sk, skb);
	}
	llc_conn_ac_set_p_flag_1(sk, ev);
	return rc;
}

int llc_conn_ac_send_dm_rsp_f_set_p(struct sock *sk,
				    struct llc_conn_state_ev *ev)
{
	int rc = 1;
	struct sk_buff *skb = llc_alloc_frame();

	if (skb) {
		struct llc_opt *llc = llc_sk(sk);
		struct llc_sap *sap = llc->sap;
		struct sk_buff *rx_skb = ev->data.pdu.skb;
		u8 f_bit;

		skb->dev = llc->dev;
		llc_pdu_decode_pf_bit(rx_skb, &f_bit);
		llc_pdu_header_init(skb, LLC_PDU_TYPE_U, sap->laddr.lsap,
				    llc->daddr.lsap, LLC_PDU_RSP);
		llc_pdu_init_as_dm_rsp(skb, f_bit);
		lan_hdrs_init(skb, llc->dev->dev_addr, llc->daddr.mac);
		rc = 0;
		llc_conn_send_pdu(sk, skb);
	}
	return rc;
}

int llc_conn_ac_send_dm_rsp_f_set_1(struct sock *sk,
				    struct llc_conn_state_ev *ev)
{
	int rc = 1;
	struct sk_buff *skb = llc_alloc_frame();

	if (skb) {
		struct llc_opt *llc = llc_sk(sk);
		struct llc_sap *sap = llc->sap;
		u8 f_bit = 1;

		skb->dev = llc->dev;
		llc_pdu_header_init(skb, LLC_PDU_TYPE_U, sap->laddr.lsap,
				    llc->daddr.lsap, LLC_PDU_RSP);
		llc_pdu_init_as_dm_rsp(skb, f_bit);
		lan_hdrs_init(skb, llc->dev->dev_addr, llc->daddr.mac);
		rc = 0;
		llc_conn_send_pdu(sk, skb);
	}
	return rc;
}

int llc_conn_ac_send_dm_rsp_f_set_f_flag(struct sock *sk,
					 struct llc_conn_state_ev *ev)
{
	int rc = 1;
	struct sk_buff *skb = llc_alloc_frame();

	if (skb) {
		struct llc_opt *llc = llc_sk(sk);
		struct llc_sap *sap = llc->sap;
		u8 f_bit = llc->f_flag;

		skb->dev = llc->dev;
		llc_pdu_header_init(skb, LLC_PDU_TYPE_U, sap->laddr.lsap,
				    llc->daddr.lsap, LLC_PDU_RSP);
		llc_pdu_init_as_dm_rsp(skb, f_bit);
		lan_hdrs_init(skb, llc->dev->dev_addr, llc->daddr.mac);
		rc = 0;
		llc_conn_send_pdu(sk, skb);
	}
	return rc;
}

int llc_conn_ac_send_frmr_rsp_f_set_x(struct sock *sk,
				      struct llc_conn_state_ev *ev)
{
	u8 f_bit;
	int rc = 1;
	struct sk_buff *skb, *ev_skb = ev->data.pdu.skb;
	llc_pdu_sn_t *rx_pdu = (llc_pdu_sn_t *)ev_skb->nh.raw;
	struct llc_opt *llc = llc_sk(sk);

	llc->rx_pdu_hdr = (u32)*((u32 *)rx_pdu);
	if (!LLC_PDU_IS_CMD(rx_pdu))
		llc_pdu_decode_pf_bit(ev_skb, &f_bit);
	else
		f_bit = 0;
	skb = llc_alloc_frame();
	if (skb) {
		struct llc_sap *sap = llc->sap;

		skb->dev = llc->dev;
		llc_pdu_header_init(skb, LLC_PDU_TYPE_U, sap->laddr.lsap,
				    llc->daddr.lsap, LLC_PDU_RSP);
		llc_pdu_init_as_frmr_rsp(skb, rx_pdu, f_bit, llc->vS,
					 llc->vR, INCORRECT);
		lan_hdrs_init(skb, llc->dev->dev_addr, llc->daddr.mac);
		rc = 0;
		llc_conn_send_pdu(sk, skb);
	}
	return rc;
}

int llc_conn_ac_resend_frmr_rsp_f_set_0(struct sock *sk,
					struct llc_conn_state_ev *ev)
{
	int rc = 1;
	struct sk_buff *skb = llc_alloc_frame();

	if (skb) {
		u8 f_bit = 0;
		struct llc_opt *llc = llc_sk(sk);
		struct llc_sap *sap = llc->sap;
		llc_pdu_sn_t *rx_pdu = (llc_pdu_sn_t *)&llc->rx_pdu_hdr;

		skb->dev = llc->dev;
		llc_pdu_header_init(skb, LLC_PDU_TYPE_U, sap->laddr.lsap,
				    llc->daddr.lsap, LLC_PDU_RSP);
		llc_pdu_init_as_frmr_rsp(skb, rx_pdu, f_bit, llc->vS,
					 llc->vR, INCORRECT);
		lan_hdrs_init(skb, llc->dev->dev_addr, llc->daddr.mac);
		rc = 0;
		llc_conn_send_pdu(sk, skb);
	}
	return rc;
}

int llc_conn_ac_resend_frmr_rsp_f_set_p(struct sock *sk,
					struct llc_conn_state_ev *ev)
{
	u8 f_bit;
	int rc = 1;
	struct sk_buff *skb;

	llc_pdu_decode_pf_bit(ev->data.pdu.skb, &f_bit);
	skb = llc_alloc_frame();
	if (skb) {
		struct llc_opt *llc = llc_sk(sk);
		struct llc_sap *sap = llc->sap;
		llc_pdu_sn_t *rx_pdu = (llc_pdu_sn_t *)ev->data.pdu.skb->nh.raw;

		skb->dev = llc->dev;
		llc_pdu_header_init(skb, LLC_PDU_TYPE_U, sap->laddr.lsap,
				    llc->daddr.lsap, LLC_PDU_RSP);
		llc_pdu_init_as_frmr_rsp(skb, rx_pdu, f_bit, llc->vS,
					 llc->vR, INCORRECT);
		lan_hdrs_init(skb, llc->dev->dev_addr, llc->daddr.mac);
		rc = 0;
		llc_conn_send_pdu(sk, skb);
	}
	return rc;
}

int llc_conn_ac_send_i_cmd_p_set_1(struct sock *sk,
				   struct llc_conn_state_ev *ev)
{
	u8 p_bit = 1;
	struct sk_buff *skb = ev->data.prim.data->data->data.skb;
	struct llc_opt *llc = llc_sk(sk);
	struct llc_sap *sap = llc->sap;

	llc_pdu_header_init(skb, LLC_PDU_TYPE_I, sap->laddr.lsap,
			    llc->daddr.lsap, LLC_PDU_CMD);
	llc_pdu_init_as_i_cmd(skb, p_bit, llc->vS, llc->vR);
	lan_hdrs_init(skb, llc->dev->dev_addr, llc->daddr.mac);
	llc_conn_send_pdu(sk, skb);
	llc_conn_ac_inc_vs_by_1(sk, ev);
	return 0;
}

int llc_conn_ac_send_i_cmd_p_set_0(struct sock *sk,
				   struct llc_conn_state_ev *ev)
{
	u8 p_bit = 0;
	struct sk_buff *skb = ev->data.prim.data->data->data.skb;
	struct llc_opt *llc = llc_sk(sk);
	struct llc_sap *sap = llc->sap;

	llc_pdu_header_init(skb, LLC_PDU_TYPE_I, sap->laddr.lsap,
			    llc->daddr.lsap, LLC_PDU_CMD);
	llc_pdu_init_as_i_cmd(skb, p_bit, llc->vS, llc->vR);
	lan_hdrs_init(skb, llc->dev->dev_addr, llc->daddr.mac);
	llc_conn_send_pdu(sk, skb);
	llc_conn_ac_inc_vs_by_1(sk, ev);
	return 0;
}

int llc_conn_ac_resend_i_cmd_p_set_1(struct sock *sk,
				     struct llc_conn_state_ev *ev)
{
	llc_pdu_sn_t *rx_pdu = (llc_pdu_sn_t *)ev->data.pdu.skb->nh.raw;

	u8 nr = LLC_I_GET_NR(rx_pdu);

	llc_conn_resend_i_pdu_as_cmd(sk, nr, 1);
	return 0;
}

int llc_conn_ac_resend_i_cmd_p_set_1_or_send_rr(struct sock *sk,
						struct llc_conn_state_ev *ev)
{
	llc_pdu_sn_t *rx_pdu = (llc_pdu_sn_t *)ev->data.pdu.skb->nh.raw;

	u8 nr = LLC_I_GET_NR(rx_pdu);
	int rc = llc_conn_ac_send_rr_cmd_p_set_1(sk, ev);

	if (!rc)
		llc_conn_resend_i_pdu_as_cmd(sk, nr, 0);
	return rc;
}

int llc_conn_ac_send_i_xxx_x_set_0(struct sock *sk,
				   struct llc_conn_state_ev *ev)
{
	u8 p_bit = 0;
	struct sk_buff *skb = ev->data.prim.data->data->data.skb;
	struct llc_opt *llc = llc_sk(sk);
	struct llc_sap *sap = llc->sap;

	llc_pdu_header_init(skb, LLC_PDU_TYPE_I, sap->laddr.lsap,
			    llc->daddr.lsap, LLC_PDU_CMD);
	llc_pdu_init_as_i_cmd(skb, p_bit, llc->vS, llc->vR);
	lan_hdrs_init(skb, llc->dev->dev_addr, llc->daddr.mac);
	llc_conn_send_pdu(sk, skb);
	llc_conn_ac_inc_vs_by_1(sk, ev);
	return 0;
}

int llc_conn_ac_resend_i_xxx_x_set_0(struct sock *sk,
				     struct llc_conn_state_ev *ev)
{
	llc_pdu_sn_t *rx_pdu = (llc_pdu_sn_t *)ev->data.pdu.skb->nh.raw;
	u8 nr = LLC_I_GET_NR(rx_pdu);

	llc_conn_resend_i_pdu_as_cmd(sk, nr, 0);
	return 0;
}

int llc_conn_ac_resend_i_xxx_x_set_0_or_send_rr(struct sock *sk,
						struct llc_conn_state_ev *ev)
{
	u8 nr;
	u8 f_bit = 0;
	llc_pdu_sn_t *rx_pdu = (llc_pdu_sn_t *)ev->data.pdu.skb->nh.raw;
	int rc = 1;
	struct sk_buff *skb = llc_alloc_frame();

	if (skb) {
		struct llc_opt *llc = llc_sk(sk);
		struct llc_sap *sap = llc->sap;

		skb->dev = llc->dev;
		llc_pdu_header_init(skb, LLC_PDU_TYPE_U, sap->laddr.lsap,
				    llc->daddr.lsap, LLC_PDU_RSP);
		llc_pdu_init_as_rr_rsp(skb, f_bit, llc->vR);
		lan_hdrs_init(skb, llc->dev->dev_addr, llc->daddr.mac);
		rc = 0;
		llc_conn_send_pdu(sk, skb);
	}
	if (rc) {
		nr = LLC_I_GET_NR(rx_pdu);
		rc = 0;
		llc_conn_resend_i_pdu_as_cmd(sk, nr, f_bit);
	}
	return rc;
}

int llc_conn_ac_resend_i_rsp_f_set_1(struct sock *sk,
				     struct llc_conn_state_ev *ev)
{
	llc_pdu_sn_t *rx_pdu = (llc_pdu_sn_t *)ev->data.pdu.skb->nh.raw;
	u8 nr = LLC_I_GET_NR(rx_pdu);

	llc_conn_resend_i_pdu_as_rsp(sk, nr, 1);
	return 0;
}

int llc_conn_ac_send_rej_cmd_p_set_1(struct sock *sk,
				     struct llc_conn_state_ev *ev)
{
	int rc = 1;
	struct sk_buff *skb = llc_alloc_frame();

	if (skb) {
		struct llc_opt *llc = llc_sk(sk);
		struct llc_sap *sap = llc->sap;
		u8 p_bit = 1;

		skb->dev = llc->dev;
		llc_pdu_header_init(skb, LLC_PDU_TYPE_S, sap->laddr.lsap,
				    llc->daddr.lsap, LLC_PDU_CMD);
		llc_pdu_init_as_rej_cmd(skb, p_bit, llc->vR);
		lan_hdrs_init(skb, llc->dev->dev_addr, llc->daddr.mac);
		rc = 0;
		llc_conn_send_pdu(sk, skb);
	}
	return rc;
}

int llc_conn_ac_send_rej_rsp_f_set_1(struct sock *sk,
				     struct llc_conn_state_ev *ev)
{
	int rc = 1;
	struct sk_buff *skb = llc_alloc_frame();

	if (skb) {
		u8 f_bit = 1;
		struct llc_opt *llc = llc_sk(sk);
		struct llc_sap *sap = llc->sap;

		skb->dev = llc->dev;
		llc_pdu_header_init(skb, LLC_PDU_TYPE_S, sap->laddr.lsap,
				    llc->daddr.lsap, LLC_PDU_RSP);
		llc_pdu_init_as_rej_rsp(skb, f_bit, llc->vR);
		lan_hdrs_init(skb, llc->dev->dev_addr, llc->daddr.mac);
		rc = 0;
		llc_conn_send_pdu(sk, skb);
	}
	return rc;
}

int llc_conn_ac_send_rej_xxx_x_set_0(struct sock *sk,
				     struct llc_conn_state_ev *ev)
{
	int rc = 1;
	struct sk_buff *skb = llc_alloc_frame();

	if (skb) {
		struct llc_opt *llc = llc_sk(sk);
		struct llc_sap *sap = llc->sap;
		u8 f_bit = 0;

		skb->dev = llc->dev;
		llc_pdu_header_init(skb, LLC_PDU_TYPE_S, sap->laddr.lsap,
				    llc->daddr.lsap, LLC_PDU_RSP);
		llc_pdu_init_as_rej_rsp(skb, f_bit, llc->vR);
		lan_hdrs_init(skb, llc->dev->dev_addr, llc->daddr.mac);
		rc = 0;
		llc_conn_send_pdu(sk, skb);
	}
	return rc;
}

int llc_conn_ac_send_rnr_cmd_p_set_1(struct sock *sk,
				     struct llc_conn_state_ev *ev)
{
	int rc = 1;
	struct sk_buff *skb = llc_alloc_frame();

	if (skb) {
		struct llc_opt *llc = llc_sk(sk);
		struct llc_sap *sap = llc->sap;
		u8 p_bit = 1;

		skb->dev = llc->dev;
		llc_pdu_header_init(skb, LLC_PDU_TYPE_S, sap->laddr.lsap,
				    llc->daddr.lsap, LLC_PDU_CMD);
		llc_pdu_init_as_rnr_cmd(skb, p_bit, llc->vR);
		lan_hdrs_init(skb, llc->dev->dev_addr, llc->daddr.mac);
		rc = 0;
		llc_conn_send_pdu(sk, skb);
	}
	return rc;
}

int llc_conn_ac_send_rnr_rsp_f_set_1(struct sock *sk,
				     struct llc_conn_state_ev *ev)
{
	int rc = 1;
	struct sk_buff *skb = llc_alloc_frame();

	if (skb) {
		struct llc_opt *llc = llc_sk(sk);
		struct llc_sap *sap = llc->sap;
		u8 f_bit = 1;

		skb->dev = llc->dev;
		llc_pdu_header_init(skb, LLC_PDU_TYPE_S, sap->laddr.lsap,
				    llc->daddr.lsap, LLC_PDU_RSP);
		llc_pdu_init_as_rnr_rsp(skb, f_bit, llc->vR);
		lan_hdrs_init(skb, llc->dev->dev_addr, llc->daddr.mac);
		rc = 0;
		llc_conn_send_pdu(sk, skb);
	}
	return rc;
}

int llc_conn_ac_send_rnr_xxx_x_set_0(struct sock *sk,
				     struct llc_conn_state_ev *ev)
{
	int rc = 1;
	struct sk_buff *skb = llc_alloc_frame();

	if (skb) {
		u8 f_bit = 0;
		struct llc_opt *llc = llc_sk(sk);
		struct llc_sap *sap = llc->sap;

		skb->dev = llc->dev;
		llc_pdu_header_init(skb, LLC_PDU_TYPE_S, sap->laddr.lsap,
				    llc->daddr.lsap, LLC_PDU_RSP);
		llc_pdu_init_as_rnr_rsp(skb, f_bit, llc->vR);
		lan_hdrs_init(skb, llc->dev->dev_addr, llc->daddr.mac);
		rc = 0;
		llc_conn_send_pdu(sk, skb);
	}
	return rc;
}

int llc_conn_ac_set_remote_busy(struct sock *sk, struct llc_conn_state_ev *ev)
{
	struct llc_opt *llc = llc_sk(sk);

	if (!llc->remote_busy_flag) {
		llc->remote_busy_flag = 1;
		llc->busy_state_timer.timer.expires = jiffies +
					llc->busy_state_timer.expire * HZ;
		llc->busy_state_timer.timer.data     = (unsigned long)sk;
		llc->busy_state_timer.timer.function = llc_conn_busy_tmr_cb;
		add_timer(&llc->busy_state_timer.timer);
		llc->busy_state_timer.running = 1;
	}
	return 0;
}

int llc_conn_ac_opt_send_rnr_xxx_x_set_0(struct sock *sk,
					 struct llc_conn_state_ev *ev)
{
	int rc = 1;
	struct sk_buff *skb = llc_alloc_frame();

	if (skb) {
		struct llc_opt *llc = llc_sk(sk);
		struct llc_sap *sap = llc->sap;
		u8 f_bit = 0;

		skb->dev = llc->dev;
		llc_pdu_header_init(skb, LLC_PDU_TYPE_S, sap->laddr.lsap,
				    llc->daddr.lsap, LLC_PDU_RSP);
		llc_pdu_init_as_rnr_rsp(skb, f_bit, llc->vR);
		lan_hdrs_init(skb, llc->dev->dev_addr, llc->daddr.mac);
		rc = 0;
		llc_conn_send_pdu(sk, skb);
	}
	return rc;
}

int llc_conn_ac_send_rr_cmd_p_set_1(struct sock *sk,
				    struct llc_conn_state_ev *ev)
{
	int rc = 1;
	struct sk_buff *skb = llc_alloc_frame();

	if (skb) {
		u8 p_bit = 1;
		struct llc_opt *llc = llc_sk(sk);
		struct llc_sap *sap = llc->sap;

		skb->dev = llc->dev;
		llc_pdu_header_init(skb, LLC_PDU_TYPE_S, sap->laddr.lsap,
				    llc->daddr.lsap, LLC_PDU_CMD);
		llc_pdu_init_as_rr_cmd(skb, p_bit, llc->vR);
		lan_hdrs_init(skb, llc->dev->dev_addr, llc->daddr.mac);
		rc = 0;
		llc_conn_send_pdu(sk, skb);
	}
	return rc;
}

int llc_conn_ac_send_ack_cmd_p_set_1(struct sock *sk,
				     struct llc_conn_state_ev *ev)
{
	int rc = 1;
	struct sk_buff *skb = llc_alloc_frame();

	if (skb) {
		u8 p_bit = 1;
		struct llc_opt *llc = llc_sk(sk);
		struct llc_sap *sap = llc->sap;

		skb->dev = llc->dev;
		llc_pdu_header_init(skb, LLC_PDU_TYPE_S, sap->laddr.lsap,
				    llc->daddr.lsap, LLC_PDU_CMD);
		llc_pdu_init_as_rr_cmd(skb, p_bit, llc->vR);
		lan_hdrs_init(skb, llc->dev->dev_addr, llc->daddr.mac);
		rc = 0;
		llc_conn_send_pdu(sk, skb);
	}
	return rc;
}

int llc_conn_ac_send_rr_rsp_f_set_1(struct sock *sk,
				    struct llc_conn_state_ev *ev)
{
	int rc = 1;
	struct sk_buff *skb = llc_alloc_frame();

	if (skb) {
		struct llc_opt *llc = llc_sk(sk);
		struct llc_sap *sap = llc->sap;
		u8 f_bit = 1;

		skb->dev = llc->dev;
		llc_pdu_header_init(skb, LLC_PDU_TYPE_S, sap->laddr.lsap,
				    llc->daddr.lsap, LLC_PDU_RSP);
		llc_pdu_init_as_rr_rsp(skb, f_bit, llc->vR);
		lan_hdrs_init(skb, llc->dev->dev_addr, llc->daddr.mac);
		rc = 0;
		llc_conn_send_pdu(sk, skb);
	}
	return rc;
}

int llc_conn_ac_send_ack_rsp_f_set_1(struct sock *sk,
				     struct llc_conn_state_ev *ev)
{
	int rc = 1;
	struct sk_buff *skb = llc_alloc_frame();

	if (skb) {
		struct llc_opt *llc = llc_sk(sk);
		struct llc_sap *sap = llc->sap;
		u8 f_bit = 1;

		skb->dev = llc->dev;
		llc_pdu_header_init(skb, LLC_PDU_TYPE_S, sap->laddr.lsap,
				    llc->daddr.lsap, LLC_PDU_RSP);
		llc_pdu_init_as_rr_rsp(skb, f_bit, llc->vR);
		lan_hdrs_init(skb, llc->dev->dev_addr, llc->daddr.mac);
		rc = 0;
		llc_conn_send_pdu(sk, skb);
	}
	return rc;
}

int llc_conn_ac_send_rr_xxx_x_set_0(struct sock *sk,
				    struct llc_conn_state_ev *ev)
{
	int rc = 1;
	struct sk_buff *skb = llc_alloc_frame();

	if (skb) {
		struct llc_opt *llc = llc_sk(sk);
		struct llc_sap *sap = llc->sap;
		u8 f_bit = 0;

		skb->dev = llc->dev;
		llc_pdu_header_init(skb, LLC_PDU_TYPE_S, sap->laddr.lsap,
				    llc->daddr.lsap, LLC_PDU_RSP);
		llc_pdu_init_as_rr_rsp(skb, f_bit, llc->vR);
		lan_hdrs_init(skb, llc->dev->dev_addr, llc->daddr.mac);
		rc = 0;
		llc_conn_send_pdu(sk, skb);
	}
	return rc;
}

int llc_conn_ac_send_ack_xxx_x_set_0(struct sock *sk,
				     struct llc_conn_state_ev *ev)
{
	int rc = 1;
	struct sk_buff *skb = llc_alloc_frame();

	if (skb) {
		struct llc_opt *llc = llc_sk(sk);
		struct llc_sap *sap = llc->sap;
		u8 f_bit = 0;

		skb->dev = llc->dev;
		llc_pdu_header_init(skb, LLC_PDU_TYPE_S, sap->laddr.lsap,
				    llc->daddr.lsap, LLC_PDU_RSP);
		llc_pdu_init_as_rr_rsp(skb, f_bit, llc->vR);
		lan_hdrs_init(skb, llc->dev->dev_addr, llc->daddr.mac);
		rc = 0;
		llc_conn_send_pdu(sk, skb);
	}
	return rc;
}

int llc_conn_ac_send_sabme_cmd_p_set_x(struct sock *sk,
				       struct llc_conn_state_ev *ev)
{
	int rc = 1;
	struct sk_buff *skb = llc_alloc_frame();
	struct llc_opt *llc = llc_sk(sk);
	u8 p_bit = 1;

	if (skb) {
		struct llc_sap *sap = llc->sap;
		u8 *dmac = llc->daddr.mac;

		if (llc->dev->flags & IFF_LOOPBACK)
			dmac = llc->dev->dev_addr;
		skb->dev = llc->dev;
		llc_pdu_header_init(skb, LLC_PDU_TYPE_U, sap->laddr.lsap,
				    llc->daddr.lsap, LLC_PDU_CMD);
		llc_pdu_init_as_sabme_cmd(skb, p_bit);
		lan_hdrs_init(skb, llc->dev->dev_addr, dmac);
		rc = 0;
		llc_conn_send_pdu(sk, skb);
	}
	llc->p_flag = p_bit;
	return rc;
}

int llc_conn_ac_send_ua_rsp_f_set_f_flag(struct sock *sk,
					 struct llc_conn_state_ev *ev)
{
	int rc = 1;
	struct sk_buff *skb = llc_alloc_frame();

	if (skb) {
		struct llc_opt *llc = llc_sk(sk);
		struct llc_sap *sap = llc->sap;
		u8 f_bit = llc->f_flag;

		skb->dev = llc->dev;
		llc_pdu_header_init(skb, LLC_PDU_TYPE_U, sap->laddr.lsap,
				    llc->daddr.lsap, LLC_PDU_RSP);
		llc_pdu_init_as_ua_rsp(skb, f_bit);
		lan_hdrs_init(skb, llc->dev->dev_addr, llc->daddr.mac);
		rc = 0;
		llc_conn_send_pdu(sk, skb);
	}
	return rc;
}

int llc_conn_ac_send_ua_rsp_f_set_p(struct sock *sk,
				    struct llc_conn_state_ev *ev)
{
	u8 f_bit;
	int rc = 1;
	struct sk_buff *rx_skb = ev->data.pdu.skb;
	struct sk_buff *skb;

	llc_pdu_decode_pf_bit(rx_skb, &f_bit);
	skb = llc_alloc_frame();
	if (skb) {
		struct llc_opt *llc = llc_sk(sk);
		struct llc_sap *sap = llc->sap;

		skb->dev = llc->dev;
		llc_pdu_header_init(skb, LLC_PDU_TYPE_U, sap->laddr.lsap,
				    llc->daddr.lsap, LLC_PDU_RSP);
		llc_pdu_init_as_ua_rsp(skb, f_bit);
		lan_hdrs_init(skb, llc->dev->dev_addr, llc->daddr.mac);
		rc = 0;
		llc_conn_send_pdu(sk, skb);
	}
	return rc;
}

int llc_conn_ac_set_s_flag_0(struct sock *sk, struct llc_conn_state_ev *ev)
{
	llc_sk(sk)->s_flag = 0;
	return 0;
}

int llc_conn_ac_set_s_flag_1(struct sock *sk, struct llc_conn_state_ev *ev)
{
	llc_sk(sk)->s_flag = 1;
	return 0;
}

int llc_conn_ac_start_p_timer(struct sock *sk, struct llc_conn_state_ev *ev)
{
	struct llc_opt *llc = llc_sk(sk);

	llc->p_flag = 1;
	del_timer(&llc->pf_cycle_timer.timer);
	llc->pf_cycle_timer.timer.expires  = jiffies +
						llc->pf_cycle_timer.expire * HZ;
	llc->pf_cycle_timer.timer.data     = (unsigned long)sk;
	llc->pf_cycle_timer.timer.function = llc_conn_pf_cycle_tmr_cb;
	add_timer(&llc->pf_cycle_timer.timer);
	llc->pf_cycle_timer.running = 1;
	return 0;
}

/**
 *	llc_conn_ac_send_ack_if_needed - check if ack is needed
 *	@sk: current connection structure
 *	@ev: current event
 *
 *	Checks number of received PDUs which have not been acknowledged, yet,
 *	If number of them reaches to "npta"(Number of PDUs To Acknowledge) then
 *	sends an RR response as acknowledgement for them.  Returns 0 for
 *	success, 1 otherwise.
 */
int llc_conn_ac_send_ack_if_needed(struct sock *sk,
				   struct llc_conn_state_ev *ev)
{
	u8 pf_bit;
	struct sk_buff *skb = ev->data.pdu.skb;
	struct llc_opt *llc = llc_sk(sk);

	llc_pdu_decode_pf_bit(skb, &pf_bit);
	llc->ack_pf |= pf_bit & 1;
	if (!llc->ack_must_be_send) {
		llc->first_pdu_Ns = llc->vR;
		llc->ack_must_be_send = 1;
		llc->ack_pf = pf_bit & 1;
	}
	if (((llc->vR - llc->first_pdu_Ns + 129) % 128) >= llc->npta) {
		llc_conn_ac_send_rr_rsp_f_set_ackpf(sk, ev);
		llc->ack_must_be_send	= 0;
		llc->ack_pf		= 0;
		llc_conn_ac_inc_npta_value(sk, ev);
	}
	return 0;
}

/**
 *	llc_conn_ac_rst_sendack_flag - resets ack_must_be_send flag
 *	@sk: current connection structure
 *	@ev: current event
 *
 *	This action resets ack_must_be_send flag of given connection, this flag
 *	indicates if there is any PDU which has not been acknowledged yet.
 *	Returns 0 for success, 1 otherwise.
 */
int llc_conn_ac_rst_sendack_flag(struct sock *sk,
				   struct llc_conn_state_ev *ev)
{
	llc_sk(sk)->ack_must_be_send = llc_sk(sk)->ack_pf = 0;
	return 0;
}

/**
 *	llc_conn_ac_send_i_rsp_f_set_ackpf - acknowledge received PDUs
 *	@sk: current connection structure
 *	@ev: current event
 *
 *	Sends an I response PDU with f-bit set to ack_pf flag as acknowledge to
 *	all received PDUs which have not been acknowledged, yet. ack_pf flag is
 *	set to one if one PDU with p-bit set to one is received.  Returns 0 for
 *	success, 1 otherwise.
 */
int llc_conn_ac_send_i_rsp_f_set_ackpf(struct sock *sk,
				       struct llc_conn_state_ev *ev)
{
	struct sk_buff *skb = ev->data.prim.data->data->data.skb;
	struct llc_opt *llc = llc_sk(sk);
	u8 p_bit = llc->ack_pf;
	struct llc_sap *sap = llc->sap;

	llc_pdu_header_init(skb, LLC_PDU_TYPE_I, sap->laddr.lsap,
			    llc->daddr.lsap, LLC_PDU_RSP);
	llc_pdu_init_as_i_cmd(skb, p_bit, llc->vS, llc->vR);
	lan_hdrs_init(skb, llc->dev->dev_addr, llc->daddr.mac);
	llc_conn_send_pdu(sk, skb);
	llc_conn_ac_inc_vs_by_1(sk, ev);
	return 0;
}

/**
 *	llc_conn_ac_send_i_as_ack - sends an I-format PDU to acknowledge rx PDUs
 *	@sk: current connection structure.
 *	@ev: current event.
 *
 *	This action sends an I-format PDU as acknowledge to received PDUs which
 *	have not been acknowledged, yet, if there is any. By using of this
 *	action number of acknowledgements decreases, this technic is called
 *	piggy backing. Returns 0 for success, 1 otherwise.
 */
int llc_conn_ac_send_i_as_ack(struct sock *sk, struct llc_conn_state_ev *ev)
{
	struct llc_opt *llc = llc_sk(sk);

	if (llc->ack_must_be_send) {
		llc_conn_ac_send_i_rsp_f_set_ackpf(sk, ev);
		llc->ack_must_be_send = 0 ;
		llc->ack_pf = 0;
	} else
		llc_conn_ac_send_i_cmd_p_set_0(sk, ev);
	return 0;
}

/**
 *	llc_conn_ac_send_rr_rsp_f_set_ackpf - ack all rx PDUs not yet acked
 *	@sk: current connection structure.
 *	@ev: current event.
 *
 *	This action sends an RR response with f-bit set to ack_pf flag as
 *	acknowledge to all received PDUs which have not been acknowledged, yet,
 *	if there is any. ack_pf flag indicates if a PDU has been received with
 *	p-bit set to one. Returns 0 for success, 1 otherwise.
 */
int llc_conn_ac_send_rr_rsp_f_set_ackpf(struct sock *sk,
					struct llc_conn_state_ev *ev)
{
	int rc = 1;
	struct sk_buff *skb = llc_alloc_frame();

	if (skb) {
		struct llc_opt *llc = llc_sk(sk);
		struct llc_sap *sap = llc->sap;
		u8 f_bit = llc->ack_pf;

		skb->dev = llc->dev;
		llc_pdu_header_init(skb, LLC_PDU_TYPE_S, sap->laddr.lsap,
				    llc->daddr.lsap, LLC_PDU_RSP);
		llc_pdu_init_as_rr_rsp(skb, f_bit, llc->vR);
		lan_hdrs_init(skb, llc->dev->dev_addr, llc->daddr.mac);
		rc = 0;
		llc_conn_send_pdu(sk, skb);
	}
	return rc;
}

/**
 *	llc_conn_ac_inc_npta_value - tries to make value of npta greater
 *	@sk: current connection structure.
 *	@ev: current event.
 *
 *	After "inc_cntr" times calling of this action, "npta" increase by one.
 *	this action tries to make vale of "npta" greater as possible; number of
 *	acknowledgements decreases by increasing of "npta". Returns 0 for
 *	success, 1 otherwise.
 */
int llc_conn_ac_inc_npta_value(struct sock *sk, struct llc_conn_state_ev *ev)
{
	struct llc_opt *llc = llc_sk(sk);

	if (!llc->inc_cntr) {
		llc->dec_step = 0;
		llc->dec_cntr = llc->inc_cntr = 2;
		++llc->npta;
		if (llc->npta > 127)
			llc->npta = 127 ;
	} else
		--llc->inc_cntr;
	return 0;
}

/**
 *	llc_conn_ac_adjust_npta_by_rr - decreases "npta" by one
 *	@sk: current connection structure.
 *	@ev: current event.
 *
 *	After receiving "dec_cntr" times RR command, this action decreases
 *	"npta" by one. Returns 0 for success, 1 otherwise.
 */
int llc_conn_ac_adjust_npta_by_rr(struct sock *sk, struct llc_conn_state_ev *ev)
{
	struct llc_opt *llc = llc_sk(sk);

	if (!llc->connect_step && !llc->remote_busy_flag) {
		if (!llc->dec_step) {
			if (!llc->dec_cntr) {
				llc->inc_cntr = llc->dec_cntr = 2;
				if (llc->npta > 0)
					llc->npta = llc->npta - 1;
			} else
				llc->dec_cntr -=1;
		}
	} else
		llc->connect_step = 0 ;
	return 0;
}

/**
 *	llc_conn_ac_adjust_npta_by_rnr - decreases "npta" by one
 *	@sk: current connection structure.
 *	@ev: current event.
 *
 *	After receiving "dec_cntr" times RNR command, this action decreases
 *	"npta" by one. Returns 0 for success, 1 otherwise.
 */
int llc_conn_ac_adjust_npta_by_rnr(struct sock *sk,
				   struct llc_conn_state_ev *ev)
{
	struct llc_opt *llc = llc_sk(sk);

	if (llc->remote_busy_flag)
		if (!llc->dec_step) {
			if (!llc->dec_cntr) {
				llc->inc_cntr = llc->dec_cntr = 2;
				if (llc->npta > 0)
					--llc->npta;
			} else
				--llc->dec_cntr;
		}
	return 0;
}

/**
 *	llc_conn_ac_dec_tx_win_size - decreases tx window size
 *	@sk: current connection structure.
 *	@ev: current event.
 *
 *	After receiving of a REJ command or response, transmit window size is
 *	decreased by number of PDUs which are outstanding yet. Returns 0 for
 *	success, 1 otherwise.
 */
int llc_conn_ac_dec_tx_win_size(struct sock *sk, struct llc_conn_state_ev *ev)
{
	struct llc_opt *llc = llc_sk(sk);
	u8 unacked_pdu = skb_queue_len(&llc->pdu_unack_q);

	llc->k -= unacked_pdu;
	if (llc->k < 2)
		llc->k = 2;
	return 0;
}

/**
 *	llc_conn_ac_inc_tx_win_size - tx window size is inc by 1
 *	@sk: current connection structure.
 *	@ev: current event.
 *
 *	After receiving an RR response with f-bit set to one, transmit window
 *	size is increased by one. Returns 0 for success, 1 otherwise.
 */
int llc_conn_ac_inc_tx_win_size(struct sock *sk, struct llc_conn_state_ev *ev)
{
	struct llc_opt *llc = llc_sk(sk);

	llc->k += 1;
	if (llc->k > 128)
		llc->k = 128 ;
	return 0;
}

int llc_conn_ac_stop_all_timers(struct sock *sk, struct llc_conn_state_ev *ev)
{
	struct llc_opt *llc = llc_sk(sk);

	del_timer(&llc->pf_cycle_timer.timer);
	llc->pf_cycle_timer.running = 0;
	del_timer(&llc->ack_timer.timer);
	llc->ack_timer.running = 0;
	del_timer(&llc->rej_sent_timer.timer);
	llc->rej_sent_timer.running = 0;
	del_timer(&llc->busy_state_timer.timer);
	llc->busy_state_timer.running = 0;
	llc->ack_must_be_send = 0;
	llc->ack_pf = 0;
	return 0;
}

int llc_conn_ac_stop_other_timers(struct sock *sk, struct llc_conn_state_ev *ev)
{
	struct llc_opt *llc = llc_sk(sk);

	del_timer(&llc->rej_sent_timer.timer);
	llc->rej_sent_timer.running = 0;
	del_timer(&llc->pf_cycle_timer.timer);
	llc->pf_cycle_timer.running = 0;
	del_timer(&llc->busy_state_timer.timer);
	llc->busy_state_timer.running = 0;
	llc->ack_must_be_send = 0;
	llc->ack_pf = 0;
	return 0;
}

int llc_conn_ac_start_ack_timer(struct sock *sk, struct llc_conn_state_ev *ev)
{
	struct llc_opt *llc = llc_sk(sk);

	del_timer(&llc->ack_timer.timer);
	llc->ack_timer.timer.expires  = jiffies + llc->ack_timer.expire * HZ;
	llc->ack_timer.timer.data     = (unsigned long)sk;
	llc->ack_timer.timer.function = llc_conn_ack_tmr_cb;
	add_timer(&llc->ack_timer.timer);
	llc->ack_timer.running = 1;
	return 0;
}

int llc_conn_ac_start_rej_timer(struct sock *sk, struct llc_conn_state_ev *ev)
{
	struct llc_opt *llc = llc_sk(sk);

	del_timer(&llc->rej_sent_timer.timer);
	llc->rej_sent_timer.timer.expires = jiffies +
					    llc->rej_sent_timer.expire * HZ;
	llc->rej_sent_timer.timer.data     = (unsigned long)sk;
	llc->rej_sent_timer.timer.function = llc_conn_rej_tmr_cb;
	add_timer(&llc->rej_sent_timer.timer);
	llc->rej_sent_timer.running = 1;
	return 0;
}

int llc_conn_ac_start_ack_tmr_if_not_running(struct sock *sk,
					     struct llc_conn_state_ev *ev)
{
	struct llc_opt *llc = llc_sk(sk);

	if (!llc->ack_timer.running) {
		llc->ack_timer.timer.expires  = jiffies +
						llc->ack_timer.expire * HZ;
		llc->ack_timer.timer.data     = (unsigned long)sk;
		llc->ack_timer.timer.function = llc_conn_ack_tmr_cb;
		add_timer(&llc->ack_timer.timer);
		llc->ack_timer.running = 1;
	}
	return 0;
}

int llc_conn_ac_stop_ack_timer(struct sock *sk, struct llc_conn_state_ev *ev)
{
	del_timer(&llc_sk(sk)->ack_timer.timer);
	llc_sk(sk)->ack_timer.running = 0;
	return 0;
}

int llc_conn_ac_stop_p_timer(struct sock *sk, struct llc_conn_state_ev *ev)
{
	struct llc_opt *llc = llc_sk(sk);

	del_timer(&llc->pf_cycle_timer.timer);
	llc->pf_cycle_timer.running = 0;
	llc->p_flag = 0;
	return 0;
}

int llc_conn_ac_stop_rej_timer(struct sock *sk, struct llc_conn_state_ev *ev)
{
	del_timer(&llc_sk(sk)->rej_sent_timer.timer);
	llc_sk(sk)->rej_sent_timer.running = 0;
	return 0;
}

int llc_conn_ac_upd_nr_received(struct sock *sk, struct llc_conn_state_ev *ev)
{
	int acked;
	u16 unacked = 0;
	u8 fbit;
	struct sk_buff *skb = ev->data.pdu.skb;
	llc_pdu_sn_t *rx_pdu = (llc_pdu_sn_t *)skb->nh.raw;
	struct llc_opt *llc = llc_sk(sk);

	llc->last_nr = PDU_SUPV_GET_Nr(rx_pdu);
	acked = llc_conn_remove_acked_pdus(sk, llc->last_nr, &unacked);
	/* On loopback we don't queue I frames in unack_pdu_q queue. */
	if (acked > 0 || (llc->dev->flags & IFF_LOOPBACK)) {
		llc->retry_count = 0;
		del_timer(&llc->ack_timer.timer);
		llc->ack_timer.running = 0;
		if (llc->failed_data_req) {
			/* already, we did not accept data from upper layer
			 * (tx_window full or unacceptable state). Now, we
			 * can send data and must inform to upper layer.
			 */
			llc->failed_data_req = 0;
			llc_conn_ac_data_confirm(sk, ev);
		}
		if (unacked) {
			llc->ack_timer.timer.expires  = jiffies +
						   llc->ack_timer.expire * HZ;
			llc->ack_timer.timer.data     = (unsigned long)sk;
			llc->ack_timer.timer.function = llc_conn_ack_tmr_cb;
			add_timer(&llc->ack_timer.timer);
			llc->ack_timer.running = 1;
	       }
	} else if (llc->failed_data_req) {
		llc_pdu_decode_pf_bit(skb, &fbit);
		if (fbit == 1) {
			llc->failed_data_req = 0;
			llc_conn_ac_data_confirm(sk, ev);
		}
	}
	return 0;
}

int llc_conn_ac_upd_p_flag(struct sock *sk, struct llc_conn_state_ev *ev)
{
	struct sk_buff *skb = ev->data.pdu.skb;
	llc_pdu_sn_t *rx_pdu = (llc_pdu_sn_t *)skb->nh.raw;
	u8 f_bit;

	if (!LLC_PDU_IS_RSP(rx_pdu) &&
	    !llc_pdu_decode_pf_bit(skb, &f_bit) && f_bit) {
		llc_sk(sk)->p_flag = 0;
		llc_conn_ac_stop_p_timer(sk, ev);
	}
	return 0;
}

int llc_conn_ac_set_data_flag_2(struct sock *sk, struct llc_conn_state_ev *ev)
{
	llc_sk(sk)->data_flag = 2;
	return 0;
}

int llc_conn_ac_set_data_flag_0(struct sock *sk, struct llc_conn_state_ev *ev)
{
	llc_sk(sk)->data_flag = 0;
	return 0;
}

int llc_conn_ac_set_data_flag_1(struct sock *sk, struct llc_conn_state_ev *ev)
{
	llc_sk(sk)->data_flag = 1;
	return 0;
}

int llc_conn_ac_set_data_flag_1_if_data_flag_eq_0(struct sock *sk,
						  struct llc_conn_state_ev *ev)
{
	if (!llc_sk(sk)->data_flag)
		llc_sk(sk)->data_flag = 1;
	return 0;
}

int llc_conn_ac_set_p_flag_0(struct sock *sk, struct llc_conn_state_ev *ev)
{
	llc_sk(sk)->p_flag = 0;
	return 0;
}

int llc_conn_ac_set_p_flag_1(struct sock *sk, struct llc_conn_state_ev *ev)
{
	llc_sk(sk)->p_flag = 1;
	return 0;
}

int llc_conn_ac_set_remote_busy_0(struct sock *sk, struct llc_conn_state_ev *ev)
{
	llc_sk(sk)->remote_busy_flag = 0;
	return 0;
}

int llc_conn_ac_set_cause_flag_0(struct sock *sk, struct llc_conn_state_ev *ev)
{
	llc_sk(sk)->cause_flag = 0;
	return 0;
}

int llc_conn_ac_set_cause_flag_1(struct sock *sk, struct llc_conn_state_ev *ev)
{
	llc_sk(sk)->cause_flag = 1;
	return 0;
}

int llc_conn_ac_set_retry_cnt_0(struct sock *sk, struct llc_conn_state_ev *ev)
{
	llc_sk(sk)->retry_count = 0;
	return 0;
}

int llc_conn_ac_inc_retry_cnt_by_1(struct sock *sk,
				   struct llc_conn_state_ev *ev)
{
	llc_sk(sk)->retry_count++;
	return 0;
}

int llc_conn_ac_set_vr_0(struct sock *sk, struct llc_conn_state_ev *ev)
{
	llc_sk(sk)->vR = 0;
	return 0;
}

int llc_conn_ac_inc_vr_by_1(struct sock *sk, struct llc_conn_state_ev *ev)
{
	llc_sk(sk)->vR = PDU_GET_NEXT_Vr(llc_sk(sk)->vR);
	return 0;
}

int llc_conn_ac_set_vs_0(struct sock *sk, struct llc_conn_state_ev *ev)
{
	llc_sk(sk)->vS = 0;
	return 0;
}

int llc_conn_ac_set_vs_nr(struct sock *sk, struct llc_conn_state_ev *ev)
{
	llc_sk(sk)->vS = llc_sk(sk)->last_nr;
	return 0;
}

int llc_conn_ac_inc_vs_by_1(struct sock *sk, struct llc_conn_state_ev *ev)
{
	llc_sk(sk)->vS = (llc_sk(sk)->vS + 1) % 128;
	return 0;
}

int llc_conn_ac_set_f_flag_p(struct sock *sk, struct llc_conn_state_ev *ev)
{
	llc_pdu_decode_pf_bit(ev->data.pdu.skb, &llc_sk(sk)->f_flag);
	return 0;
}

void llc_conn_pf_cycle_tmr_cb(unsigned long timeout_data)
{
	struct sock *sk = (struct sock *)timeout_data;
	struct llc_conn_state_ev *ev;

	llc_sk(sk)->pf_cycle_timer.running = 0;
	ev = llc_conn_alloc_ev(sk);
	if (ev) {
		ev->type = LLC_CONN_EV_TYPE_P_TMR;
		ev->data.tmr.timer_specific = NULL;
		llc_process_tmr_ev(sk, ev);
	}
}

static void llc_conn_busy_tmr_cb(unsigned long timeout_data)
{
	struct sock *sk = (struct sock *)timeout_data;
	struct llc_conn_state_ev *ev;

	llc_sk(sk)->busy_state_timer.running = 0;
	ev = llc_conn_alloc_ev(sk);
	if (ev) {
		ev->type = LLC_CONN_EV_TYPE_BUSY_TMR;
		ev->data.tmr.timer_specific = NULL;
		llc_process_tmr_ev(sk, ev);
	}
}

void llc_conn_ack_tmr_cb(unsigned long timeout_data)
{
	struct sock* sk = (struct sock *)timeout_data;
	struct llc_conn_state_ev *ev;

	llc_sk(sk)->ack_timer.running = 0;
	ev = llc_conn_alloc_ev(sk);
	if (ev) {
		ev->type = LLC_CONN_EV_TYPE_ACK_TMR;
		ev->data.tmr.timer_specific = NULL;
		llc_process_tmr_ev(sk, ev);
	}
}

static void llc_conn_rej_tmr_cb(unsigned long timeout_data)
{
	struct sock *sk = (struct sock *)timeout_data;
	struct llc_conn_state_ev *ev;

	llc_sk(sk)->rej_sent_timer.running = 0;
	ev = llc_conn_alloc_ev(sk);
	if (ev) {
		ev->type = LLC_CONN_EV_TYPE_REJ_TMR;
		ev->data.tmr.timer_specific = NULL;
		llc_process_tmr_ev(sk, ev);
	}
}

int llc_conn_ac_rst_vs(struct sock *sk, struct llc_conn_state_ev *ev)
{
	llc_sk(sk)->X = llc_sk(sk)->vS;
	llc_conn_ac_set_vs_nr(sk, ev);
	return 0;
}

int llc_conn_ac_upd_vs(struct sock *sk, struct llc_conn_state_ev *ev)
{
	llc_pdu_sn_t *rx_pdu = (llc_pdu_sn_t *)ev->data.pdu.skb->nh.raw;
	u8 nr = PDU_SUPV_GET_Nr(rx_pdu);

	if (llc_circular_between(llc_sk(sk)->vS, nr, llc_sk(sk)->X))
		llc_conn_ac_set_vs_nr(sk, ev);
	return 0;
}

/*
 * Non-standard actions; these not contained in IEEE specification; for
 * our own usage
 */
/**
 *	llc_conn_disc - removes connection from SAP list and frees it
 *	@sk: closed connection
 *	@ev: occurred event
 *
 *	Returns 2, to indicate the state machine that the connection was freed.
 */
int llc_conn_disc(struct sock *sk, struct llc_conn_state_ev *ev)
{
	llc_sap_unassign_sock(llc_sk(sk)->sap, sk);
	llc_sock_free(sk);
	return 2;
}

/**
 *	llc_conn_reset - resets connection
 *	@sk : reseting connection.
 *	@ev: occurred event.
 *
 *	Stop all timers, empty all queues and reset all flags.
 */
int llc_conn_reset(struct sock *sk, struct llc_conn_state_ev *ev)
{
	llc_sock_reset(sk);
	return 0;
}

/**
 *	llc_circular_between - designates that b is between a and c or not
 *	@a: lower bound
 *	@b: element to see if is between a and b
 *	@c: upper bound
 *
 *	This function designates that b is between a and c or not (for example,
 *	0 is between 127 and 1). Returns 1 if b is between a and c, 0
 *	otherwise.
 */
u8 llc_circular_between(u8 a, u8 b, u8 c)
{
	b = b - a;
	c = c - a;
	return b <= c;
}

/**
 *	llc_process_tmr_ev - timer backend
 *	@sk: active connection
 *	@ev: occurred event
 *
 *	This function is called from timer callback functions. When connection
 *	is busy (during sending a data frame) timer expiration event must be
 *	queued. Otherwise this event can be sent to connection state machine.
 *	Queued events will process by process_rxframes_events function after
 *	sending data frame. Returns 0 for success, 1 otherwise.
 */
static void llc_process_tmr_ev(struct sock *sk, struct llc_conn_state_ev *ev)
{
	bh_lock_sock(sk);
	if (llc_sk(sk)->state == LLC_CONN_OUT_OF_SVC) {
		printk(KERN_WARNING "timer called on closed connection\n");
		llc_conn_free_ev(ev);
		goto out;
	}
	if (!sk->lock.users)
		llc_conn_send_ev(sk, ev);
	else {
		struct sk_buff *skb = alloc_skb(1, GFP_ATOMIC);

		if (skb) {
			skb->cb[0] = LLC_EVENT;
			skb->data = (void *)ev;
			sk_add_backlog(sk, skb);
		} else
			llc_conn_free_ev(ev);
	}
out:
	bh_unlock_sock(sk);
}
