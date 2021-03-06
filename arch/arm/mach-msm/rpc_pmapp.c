/* Copyright (c) 2009, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include <linux/err.h>
#include <asm/mach-types.h>
#include <mach/board.h>
#include <mach/rpc_pmapp.h>
#include <mach/msm_rpcrouter.h>
#include <mach/vreg.h>

#define PM_APP_USB_PROG				0x30000060
#define PM_APP_USB_VERS_1_1			0x00010001
#define PM_APP_USB_VERS_1_2			0x00010002
#define PM_APP_USB_VERS_2_1			0x00020001

#define VBUS_SESS_VALID_CB_PROC			1
#define PM_VOTE_USB_PWR_SEL_SWITCH_APP__HSUSB 	(1 << 2)
#define PM_USB_PWR_SEL_SWITCH_ID 		0

struct rpc_pmapp_ids {
	unsigned long	reg_for_vbus_valid;
	unsigned long	vote_for_vbus_valid_switch;
};

static struct rpc_pmapp_ids rpc_ids;
static struct vreg *boost_vreg, *usb_vreg;
static struct msm_rpc_client *client;
static int ldo_on;

static void rpc_pmapp_init_rpc_ids(unsigned long vers)
{
	if (vers == PM_APP_USB_VERS_1_1) {
		rpc_ids.reg_for_vbus_valid		= 5;
		rpc_ids.vote_for_vbus_valid_switch	= 6;
	} else if (vers == PM_APP_USB_VERS_1_2) {
		rpc_ids.reg_for_vbus_valid		= 16;
		rpc_ids.vote_for_vbus_valid_switch	= 17;
	} else if (vers == PM_APP_USB_VERS_2_1) {
		rpc_ids.reg_for_vbus_valid		= 0; /* NA */
		rpc_ids.vote_for_vbus_valid_switch	= 0; /* NA */
	}
}

struct usb_pwr_sel_switch_args {
	uint32_t cmd;
	uint32_t switch_id;
	uint32_t app_mask;
};

static int usb_pwr_sel_switch_arg_cb(struct msm_rpc_client *client,
					void *buf, void *data)
{
	struct usb_pwr_sel_switch_args *args = buf;

	args->cmd = cpu_to_be32(*(uint32_t *)data);
	args->switch_id = cpu_to_be32(PM_USB_PWR_SEL_SWITCH_ID);
	args->app_mask = cpu_to_be32(PM_VOTE_USB_PWR_SEL_SWITCH_APP__HSUSB);
	return sizeof(struct usb_pwr_sel_switch_args);
}

static int msm_pm_app_vote_usb_pwr_sel_switch(uint32_t cmd)
{
	return msm_rpc_client_req(client,
			rpc_ids.vote_for_vbus_valid_switch,
			usb_pwr_sel_switch_arg_cb,
			&cmd, NULL, NULL, -1);
}

struct vbus_sess_valid_args {
	uint32_t cb_id;
};

static int vbus_sess_valid_arg_cb(struct msm_rpc_client *client,
					void *buf, void *data)
{
	struct vbus_sess_valid_args *args = buf;

	args->cb_id = cpu_to_be32(*(uint32_t *)data);
	return sizeof(struct vbus_sess_valid_args);
}

int msm_pm_app_register_vbus_sn(void (*callback)(int online))
{
	uint32_t cb_id = msm_rpc_add_cb_func(client, (void *)callback);

	/* In case of NULL callback funtion, cb_id would be -1 */
	if ((int) cb_id < -1)
		return cb_id;

	return msm_rpc_client_req(client,
			rpc_ids.reg_for_vbus_valid,
			vbus_sess_valid_arg_cb,
			&cb_id, NULL, NULL, -1);

}
EXPORT_SYMBOL(msm_pm_app_register_vbus_sn);

void msm_pm_app_unregister_vbus_sn(void (*callback)(int online))
{
	msm_rpc_remove_cb_func(client, (void *)callback);
}
EXPORT_SYMBOL(msm_pm_app_unregister_vbus_sn);

int msm_pm_app_enable_usb_ldo(int enable)
{
	int ret;

	if (ldo_on == enable)
		return 0;
	ldo_on = enable;

	if (enable) {
		/* vote to turn ON Boost Vreg_5V */
		ret = vreg_enable(boost_vreg);
		if (ret < 0)
			return ret;
		/* vote to switch it to VREG_5V source */
		ret = msm_pm_app_vote_usb_pwr_sel_switch(1);
		if (ret < 0) {
			vreg_disable(boost_vreg);
			return ret;
		}
		ret = vreg_enable(usb_vreg);
		if (ret < 0) {
			msm_pm_app_vote_usb_pwr_sel_switch(0);
			vreg_disable(boost_vreg);
			return ret;
		}

	} else {
		ret = vreg_disable(usb_vreg);
		if (ret < 0)
			return ret;
		ret = vreg_disable(boost_vreg);
		if (ret < 0)
			return ret;
		/* vote to switch it to VBUS source */
		ret = msm_pm_app_vote_usb_pwr_sel_switch(0);
		if (ret < 0)
			return ret;
	}

	return 0;
}
EXPORT_SYMBOL(msm_pm_app_enable_usb_ldo);

struct vbus_sn_notification_args {
	uint32_t cb_id;
	uint32_t vbus; /* vbus = 0 if VBUS is present */
};

static int vbus_notification_cb(struct msm_rpc_client *client,
				void *buffer, int in_size)
{
	struct vbus_sn_notification_args *args;
	struct rpc_request_hdr *req = buffer;
	int rc;
	uint32_t accept_status;
	void (*cb_func)(int);
	uint32_t cb_id;
	int vbus;

	args = (struct vbus_sn_notification_args *) (req + 1);
	cb_id = be32_to_cpu(args->cb_id);
	vbus = be32_to_cpu(args->vbus);

	cb_func = msm_rpc_get_cb_func(client, cb_id);
	if (cb_func) {
		cb_func(!vbus);
		accept_status = RPC_ACCEPTSTAT_SUCCESS;
	} else
		accept_status = RPC_ACCEPTSTAT_SYSTEM_ERR;

	msm_rpc_start_accepted_reply(client, be32_to_cpu(req->xid),
				     accept_status);
	rc = msm_rpc_send_accepted_reply(client, 0);
	if (rc)
		pr_err("%s: send accepted reply failed: %d\n", __func__, rc);

	return rc;
}

static int pm_app_usb_cb_func(struct msm_rpc_client *client,
				void *buffer, int in_size)
{
	int rc;
	struct rpc_request_hdr *req = buffer;

	switch (be32_to_cpu(req->procedure)) {
	case VBUS_SESS_VALID_CB_PROC:
		rc = vbus_notification_cb(client, buffer, in_size);
		break;
	default:
		pr_err("%s: procedure not supported %d\n", __func__,
		       be32_to_cpu(req->procedure));
		msm_rpc_start_accepted_reply(client, be32_to_cpu(req->xid),
					     RPC_ACCEPTSTAT_PROC_UNAVAIL);
		rc = msm_rpc_send_accepted_reply(client, 0);
		if (rc)
			pr_err("%s: sending reply failed: %d\n", __func__, rc);
		break;
	}
	return rc;
}

int msm_pm_app_rpc_init(void)
{

	if (!machine_is_qsd8x50_ffa() && !machine_is_qsd8x50a_ffa()
			&& !machine_is_msm7x27_ffa())
		return -ENOTSUPP;

	boost_vreg = vreg_get(NULL, "boost");
	if (IS_ERR(boost_vreg)) {
		pr_err("%s: boost vreg get failed\n", __func__);
		return PTR_ERR(boost_vreg);
	}

	usb_vreg = vreg_get(NULL, "usb");
	if (IS_ERR(usb_vreg)) {
		pr_err("%s: usb vreg get failed\n", __func__);
		vreg_put(usb_vreg);
		return PTR_ERR(usb_vreg);
	}

	client = msm_rpc_register_client("pmapp_usb",
			PM_APP_USB_PROG,
			PM_APP_USB_VERS_2_1, 1,
			pm_app_usb_cb_func);
	if (!IS_ERR(client)) {
		rpc_pmapp_init_rpc_ids(PM_APP_USB_VERS_2_1);
		goto done;
	}

	client = msm_rpc_register_client("pmapp_usb",
			PM_APP_USB_PROG,
			PM_APP_USB_VERS_1_2, 1,
			pm_app_usb_cb_func);
	if (!IS_ERR(client)) {
		rpc_pmapp_init_rpc_ids(PM_APP_USB_VERS_1_2);
		goto done;
	}

	client = msm_rpc_register_client("pmapp_usb",
			PM_APP_USB_PROG,
			PM_APP_USB_VERS_1_1, 1,
			pm_app_usb_cb_func);
	if (!IS_ERR(client))
		rpc_pmapp_init_rpc_ids(PM_APP_USB_VERS_1_1);
	else
		return PTR_ERR(client);

done:
	return 0;
}
EXPORT_SYMBOL(msm_pm_app_rpc_init);

void msm_pm_app_rpc_deinit(void)
{
	if (client) {
		msm_rpc_unregister_client(client);
		msm_pm_app_enable_usb_ldo(0);
		vreg_put(boost_vreg);
		vreg_put(usb_vreg);
	}
}
EXPORT_SYMBOL(msm_pm_app_rpc_deinit);

#define PMAPP_RPC_TIMEOUT (5*HZ)

#define PMAPP_RPC_PROG		0x30000060
#define PMAPP_RPC_VER_2_1	0x00020001
#define PMAPP_RPC_VER_3_1	0x00030001

#define PMAPP_DISPLAY_CLOCK_CONFIG_PROC		21
#define PMAPP_CLOCK_VOTE_PROC			27

/* Clock voter name max length */
#define PMAPP_CLOCK_VOTER_ID_LEN		4

/* error bit flags defined by modem side */
#define PM_ERR_FLAG__PAR1_OUT_OF_RANGE		(0x0001)
#define PM_ERR_FLAG__PAR2_OUT_OF_RANGE		(0x0002)
#define PM_ERR_FLAG__PAR3_OUT_OF_RANGE		(0x0004)
#define PM_ERR_FLAG__PAR4_OUT_OF_RANGE		(0x0008)
#define PM_ERR_FLAG__PAR5_OUT_OF_RANGE		(0x0010)

#define PM_ERR_FLAG__ALL_PARMS_OUT_OF_RANGE   	(0x001F) /* all 5 previous */

#define PM_ERR_FLAG__SBI_OPT_ERR		(0x0080)
#define PM_ERR_FLAG__FEATURE_NOT_SUPPORTED	(0x0100)

#define	PMAPP_BUFF_SIZE		256

struct pmapp_buf {
	char *start;		/* buffer start addr */
	char *end;		/* buffer end addr */
	int size;		/* buffer size */
	char *data;		/* payload begin addr */
	int len;		/* payload len */
};

static DEFINE_MUTEX(pmapp_mtx);

struct pmapp_ctrl {
	int inited;
	struct pmapp_buf tbuf;
	struct pmapp_buf rbuf;
	struct msm_rpc_endpoint *endpoint;
};

static struct pmapp_ctrl pmapp_ctrl = {
	.inited = -1,
};


static int pmapp_rpc_set_only(uint data0, uint data1, uint data2,
				uint data3, int num, int proc);

static int pmapp_buf_init(void)
{
	struct pmapp_ctrl *pm = &pmapp_ctrl;

	memset(&pmapp_ctrl, 0, sizeof(pmapp_ctrl));

	pm->tbuf.start = kmalloc(PMAPP_BUFF_SIZE, GFP_KERNEL);
	if (pm->tbuf.start == NULL) {
		printk(KERN_ERR "%s:%u\n", __func__, __LINE__);
		return -ENOMEM;
	}

	pm->tbuf.data = pm->tbuf.start;
	pm->tbuf.size = PMAPP_BUFF_SIZE;
	pm->tbuf.end = pm->tbuf.start + PMAPP_BUFF_SIZE;
	pm->tbuf.len = 0;

	pm->rbuf.start = kmalloc(PMAPP_BUFF_SIZE, GFP_KERNEL);
	if (pm->rbuf.start == NULL) {
		kfree(pm->tbuf.start);
		printk(KERN_ERR "%s:%u\n", __func__, __LINE__);
		return -ENOMEM;
	}
	pm->rbuf.data = pm->rbuf.start;
	pm->rbuf.size = PMAPP_BUFF_SIZE;
	pm->rbuf.end = pm->rbuf.start + PMAPP_BUFF_SIZE;
	pm->rbuf.len = 0;

	pm->inited = 1;

	return 0;
}

static inline void pmapp_buf_reserve(struct pmapp_buf *bp, int len)
{
	bp->data += len;
}

static inline void pmapp_buf_reset(struct pmapp_buf *bp)
{
	bp->data = bp->start;
	bp->len = 0;
}

static int modem_to_linux_err(uint err)
{
	if (err == 0)
		return 0;

	if (err & PM_ERR_FLAG__ALL_PARMS_OUT_OF_RANGE)
		return -EINVAL;	/* PM_ERR_FLAG__PAR[1..5]_OUT_OF_RANGE */

	if (err & PM_ERR_FLAG__SBI_OPT_ERR)
		return -EIO;

	if (err & PM_ERR_FLAG__FEATURE_NOT_SUPPORTED)
		return -ENOSYS;

	return -EPERM;
}

static int pmapp_put_tx_data(struct pmapp_buf *tp, uint datav)
{
	uint *lp;

	if ((tp->size - tp->len) < sizeof(datav)) {
		printk(KERN_ERR "%s: OVERFLOW size=%d len=%d\n",
					__func__, tp->size, tp->len);
		return -1;
	}

	lp = (uint *)tp->data;
	*lp = cpu_to_be32(datav);
	tp->data += sizeof(datav);
	tp->len += sizeof(datav);

	return sizeof(datav);
}

static int pmapp_pull_rx_data(struct pmapp_buf *rp, uint *datap)
{
	uint *lp;

	if (rp->len < sizeof(*datap)) {
		printk(KERN_ERR "%s: UNDERRUN len=%d\n", __func__, rp->len);
		return -1;
	}
	lp = (uint *)rp->data;
	*datap = be32_to_cpu(*lp);
	rp->data += sizeof(*datap);
	rp->len -= sizeof(*datap);

	return sizeof(*datap);
}


static int pmapp_rpc_req_reply(struct pmapp_buf *tbuf, struct pmapp_buf *rbuf,
	int	proc)
{
	struct pmapp_ctrl *pm = &pmapp_ctrl;
	int	ans, len;


	if ((pm->endpoint == NULL) || IS_ERR(pm->endpoint)) {
		pm->endpoint = msm_rpc_connect_compatible(PMAPP_RPC_PROG,
					PMAPP_RPC_VER_3_1, 0);
		if (IS_ERR(pm->endpoint)) {
			pm->endpoint = msm_rpc_connect_compatible(
				PMAPP_RPC_PROG, PMAPP_RPC_VER_2_1, 0);
		}
		if (IS_ERR(pm->endpoint)) {
			ans  = PTR_ERR(pm->endpoint);
			printk(KERN_ERR "%s: init rpc failed! ans = %d\n",
						__func__, ans);
			return ans;
		}
	}

	/*
	* data is point to next available space at this moment,
	* move it back to beginning of request header and increase
	* the length
	*/
	tbuf->data = tbuf->start;
	tbuf->len += sizeof(struct rpc_request_hdr);

	len = msm_rpc_call_reply(pm->endpoint, proc,
				tbuf->data, tbuf->len,
				rbuf->data, rbuf->size,
				PMAPP_RPC_TIMEOUT);

	if (len <= 0) {
		printk(KERN_ERR "%s: rpc failed! len = %d\n", __func__, len);
		pm->endpoint = NULL;	/* re-connect later ? */
		return len;
	}

	rbuf->len = len;
	/* strip off rpc_reply_hdr */
	rbuf->data += sizeof(struct rpc_reply_hdr);
	rbuf->len -= sizeof(struct rpc_reply_hdr);

	return rbuf->len;
}

static int pmapp_rpc_set_only(uint data0, uint data1, uint data2, uint data3,
		int num, int proc)
{
	struct pmapp_ctrl *pm = &pmapp_ctrl;
	struct pmapp_buf	*tp;
	struct pmapp_buf	*rp;
	int	stat;


	if (mutex_lock_interruptible(&pmapp_mtx))
		return -ERESTARTSYS;

	if (pm->inited <= 0) {
		stat = pmapp_buf_init();
		if (stat < 0) {
			mutex_unlock(&pmapp_mtx);
			return stat;
		}
	}

	tp = &pm->tbuf;
	rp = &pm->rbuf;

	pmapp_buf_reset(tp);
	pmapp_buf_reserve(tp, sizeof(struct rpc_request_hdr));
	pmapp_buf_reset(rp);

	if (num > 0)
		pmapp_put_tx_data(tp, data0);

	if (num > 1)
		pmapp_put_tx_data(tp, data1);

	if (num > 2)
		pmapp_put_tx_data(tp, data2);

	if (num > 3)
		pmapp_put_tx_data(tp, data3);

	stat = pmapp_rpc_req_reply(tp, rp, proc);
	if (stat < 0) {
		mutex_unlock(&pmapp_mtx);
		return stat;
	}

	pmapp_pull_rx_data(rp, &stat);	/* result from server */

	mutex_unlock(&pmapp_mtx);

	return modem_to_linux_err(stat);
}

int pmapp_display_clock_config(uint enable)
{
	return pmapp_rpc_set_only(enable, 0, 0, 0, 1,
			PMAPP_DISPLAY_CLOCK_CONFIG_PROC);
}
EXPORT_SYMBOL(pmapp_display_clock_config);

int pmapp_clock_vote(const char *voter_id, uint clock_id, uint vote)
{
	if (strlen(voter_id) != PMAPP_CLOCK_VOTER_ID_LEN)
		return -EINVAL;

	return pmapp_rpc_set_only(*((uint *) voter_id), clock_id, vote, 0, 3,
			PMAPP_CLOCK_VOTE_PROC);
}
EXPORT_SYMBOL(pmapp_clock_vote);
