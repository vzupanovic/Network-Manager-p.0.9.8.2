/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* NetworkManager -- Network link manager
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
 * Copyright (C) 2005 - 2011 Red Hat, Inc.
 * Copyright (C) 2006 - 2008 Novell, Inc.
 * Copyright (C) 2011 Intel Corporation. All rights reserved.
 */

#include <config.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <net/ethernet.h>
#include <unistd.h>
#include <math.h>

#include <glib.h>

#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <netlink/route/link.h>

#include <linux/nl80211.h>

#include "wifi-utils-private.h"
#include "wifi-utils-nl80211.h"
#include "nm-logging.h"
#include "nm-utils.h"

typedef struct {
	WifiData parent;
	struct nl_sock *nl_sock;
	int id;
	struct nl_cb *nl_cb;
	guint32 *freqs;
	int num_freqs;
} WifiDataNl80211;

static int ack_handler (struct nl_msg *msg, void *arg)
{
	int *done = arg;
	*done = 1;
	return NL_STOP;
}

static int finish_handler (struct nl_msg *msg, void *arg)
{
	int *done = arg;
	*done = 1;
	return NL_SKIP;
}

static int error_handler (struct sockaddr_nl *nla, struct nlmsgerr *err,
			  void *arg)
{
	int *done = arg;
	*done = err->error;
	return NL_SKIP;
}

static struct nl_msg *
_nl80211_alloc_msg (int id, int ifindex, guint32 cmd, guint32 flags)
{
	struct nl_msg *msg;

	msg = nlmsg_alloc ();
	if (msg) {
		genlmsg_put (msg, 0, 0, id, 0, flags, cmd, 0);
		NLA_PUT_U32 (msg, NL80211_ATTR_IFINDEX, ifindex);
	}
	return msg;

 nla_put_failure:
	nlmsg_free (msg);
	return NULL;
}

static struct nl_msg *
nl80211_alloc_msg (WifiDataNl80211 *nl80211, guint32 cmd, guint32 flags)
{
	return _nl80211_alloc_msg (nl80211->id, nl80211->parent.ifindex, cmd, flags);
}

/* NOTE: this function consumes 'msg' */
static int
_nl80211_send_and_recv (struct nl_sock *nl_sock, 
                        struct nl_cb *nl_cb,
                        struct nl_msg *msg,
                        int (*valid_handler)(struct nl_msg *, void *),
                        void *valid_data)
{
	struct nl_cb *cb;
	int err, done;

	g_return_val_if_fail (msg != NULL, -ENOMEM);

	cb = nl_cb_clone (nl_cb);
	if (!cb) {
		err = -ENOMEM;
		goto out;
	}

	err = nl_send_auto_complete (nl_sock, msg);
	if (err < 0)
		goto out;

	done = 0;
	nl_cb_err (cb, NL_CB_CUSTOM, error_handler, &done);
	nl_cb_set (cb, NL_CB_FINISH, NL_CB_CUSTOM, finish_handler, &done);
	nl_cb_set (cb, NL_CB_ACK, NL_CB_CUSTOM, ack_handler, &done);
	if (valid_handler)
		nl_cb_set (cb, NL_CB_VALID, NL_CB_CUSTOM, valid_handler, valid_data);

	/* Loop until one of our NL callbacks says we're done; on success
	 * done will be 1, on error it will be < 0.
	 */
	while (!done) {
		err = nl_recvmsgs (nl_sock, cb);
		if (err && err != -NLE_AGAIN) {
			nm_log_warn (LOGD_WIFI, "nl_recvmsgs() error: (%d) %s",
			             err, nl_geterror (err));
			break;
		}
	}
	if (err == 0 && done < 0)
		err = done;

 out:
	nl_cb_put (cb);
	nlmsg_free (msg);
	return err;
}

static int
nl80211_send_and_recv (WifiDataNl80211 *nl80211,
                       struct nl_msg *msg,
                       int (*valid_handler)(struct nl_msg *, void *),
                       void *valid_data)
{
	return _nl80211_send_and_recv (nl80211->nl_sock, nl80211->nl_cb, msg, valid_handler, valid_data);
}

static void
wifi_nl80211_deinit (WifiData *parent)
{
	WifiDataNl80211 *nl80211 = (WifiDataNl80211 *) parent;

	if (nl80211->nl_sock)
		nl_socket_free (nl80211->nl_sock);
	if (nl80211->nl_cb)
		nl_cb_put (nl80211->nl_cb);
	g_free (nl80211->freqs);
}

struct nl80211_iface_info {
	NM80211Mode mode;
};

static int nl80211_iface_info_handler (struct nl_msg *msg, void *arg)
{
	struct nl80211_iface_info *info = arg;
	struct genlmsghdr *gnlh = nlmsg_data (nlmsg_hdr (msg));
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
                
	if (nla_parse (tb, NL80211_ATTR_MAX, genlmsg_attrdata (gnlh, 0),
		       genlmsg_attrlen (gnlh, 0), NULL) < 0)
		return NL_SKIP;

	if (!tb[NL80211_ATTR_IFTYPE])
		return NL_SKIP;

	switch (nla_get_u32 (tb[NL80211_ATTR_IFTYPE])) {
	case NL80211_IFTYPE_ADHOC:
		info->mode = NM_802_11_MODE_ADHOC;
		break;
	case NL80211_IFTYPE_AP:
		info->mode = NM_802_11_MODE_AP;
		break;
	case NL80211_IFTYPE_STATION:
		info->mode = NM_802_11_MODE_INFRA;
		break;
	}
                          
	return NL_SKIP;
}

static NM80211Mode
wifi_nl80211_get_mode (WifiData *data)
{
	WifiDataNl80211 *nl80211 = (WifiDataNl80211 *) data;
	struct nl80211_iface_info iface_info = {
		.mode = NM_802_11_MODE_UNKNOWN,
	};
	struct nl_msg *msg;

	msg = nl80211_alloc_msg (nl80211, NL80211_CMD_GET_INTERFACE, 0);

	if (nl80211_send_and_recv (nl80211, msg, nl80211_iface_info_handler,
				   &iface_info) < 0)
		return NM_802_11_MODE_UNKNOWN;

	return iface_info.mode;
}

static gboolean
wifi_nl80211_set_mode (WifiData *data, const NM80211Mode mode)
{
	WifiDataNl80211 *nl80211 = (WifiDataNl80211 *) data;
	struct nl_msg *msg;
	int err;

	msg = nl80211_alloc_msg (nl80211, NL80211_CMD_SET_INTERFACE, 0);

	switch (mode) {
	case NM_802_11_MODE_INFRA:
		NLA_PUT_U32 (msg, NL80211_ATTR_IFTYPE, NL80211_IFTYPE_STATION);
		break;
	case NM_802_11_MODE_ADHOC:
		NLA_PUT_U32 (msg, NL80211_ATTR_IFTYPE, NL80211_IFTYPE_ADHOC);
		break;
	case NM_802_11_MODE_AP:
		NLA_PUT_U32 (msg, NL80211_ATTR_IFTYPE, NL80211_IFTYPE_AP);
		break;
	default:
		g_assert_not_reached ();
	}

	err = nl80211_send_and_recv (nl80211, msg, NULL, NULL);
	return err ? FALSE : TRUE;

 nla_put_failure:
	nlmsg_free (msg);
	return FALSE;
}

/* @divisor: pass what value @xbm should be divided by to get dBm */
static guint32 nl80211_xbm_to_percent (gint32 xbm, guint32 divisor)
{
#define NOISE_FLOOR_DBM  -90
#define SIGNAL_MAX_DBM   -20

	xbm /= divisor;
	xbm = CLAMP(xbm, NOISE_FLOOR_DBM, SIGNAL_MAX_DBM);

	return 100 - 70 * (((float) SIGNAL_MAX_DBM - (float) xbm) /
			   ((float) SIGNAL_MAX_DBM - (float) NOISE_FLOOR_DBM));
}

struct nl80211_bss_info {
	guint32 freq;
	guint8 bssid[ETH_ALEN];
	guint8 ssid[32];
	guint32 ssid_len;
	guint32 beacon_signal;
	gboolean valid;
};

#define WLAN_EID_SSID	0

static void find_ssid (guint8 *ies, guint32 ies_len,
		       guint8 **ssid, guint32 *ssid_len)
{
	*ssid = NULL;
	*ssid_len = 0;

	while (ies_len > 2 && ies[0] != WLAN_EID_SSID) {
		ies_len -= ies[1] + 2;
		ies += ies[1] + 2;
	}
	if (ies_len < 2)
		return;
	if (ies_len < 2 + ies[1])
		return;

	*ssid_len = ies[1];
	*ssid = ies + 2;
}

static int nl80211_bss_dump_handler (struct nl_msg *msg, void *arg)
{
	struct nl80211_bss_info *info = arg;
	struct genlmsghdr *gnlh = nlmsg_data (nlmsg_hdr (msg));
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct nlattr *bss[NL80211_BSS_MAX + 1];
	static struct nla_policy bss_policy[NL80211_BSS_MAX + 1] = {
		[NL80211_BSS_TSF] = { .type = NLA_U64 },
		[NL80211_BSS_FREQUENCY] = { .type = NLA_U32 },
		[NL80211_BSS_BSSID] = { },
		[NL80211_BSS_BEACON_INTERVAL] = { .type = NLA_U16 },
		[NL80211_BSS_CAPABILITY] = { .type = NLA_U16 },
		[NL80211_BSS_INFORMATION_ELEMENTS] = { },
		[NL80211_BSS_SIGNAL_MBM] = { .type = NLA_U32 },
		[NL80211_BSS_SIGNAL_UNSPEC] = { .type = NLA_U8 },
		[NL80211_BSS_STATUS] = { .type = NLA_U32 },
	};
	guint32 status;
                
	if (nla_parse (tb, NL80211_ATTR_MAX, genlmsg_attrdata (gnlh, 0),
		       genlmsg_attrlen (gnlh, 0), NULL) < 0)
		return NL_SKIP;

	if (tb[NL80211_ATTR_BSS] == NULL)
		return NL_SKIP;

	if (nla_parse_nested (bss, NL80211_BSS_MAX,
			      tb[NL80211_ATTR_BSS],
			      bss_policy))
		return NL_SKIP;

	if (bss[NL80211_BSS_STATUS] == NULL)
		return NL_SKIP;

	status = nla_get_u32 (bss[NL80211_BSS_STATUS]);

	if (status != NL80211_BSS_STATUS_ASSOCIATED &&
	    status != NL80211_BSS_STATUS_IBSS_JOINED)
		return NL_SKIP;

	if (bss[NL80211_BSS_BSSID] == NULL)
		return NL_SKIP;
	memcpy(info->bssid, nla_data (bss[NL80211_BSS_BSSID]), ETH_ALEN);

	if (bss[NL80211_BSS_FREQUENCY])
		info->freq = nla_get_u32 (bss[NL80211_BSS_FREQUENCY]);

	if (bss[NL80211_BSS_SIGNAL_UNSPEC])
		info->beacon_signal =
			nla_get_u8 (bss[NL80211_BSS_SIGNAL_UNSPEC]);

	if (bss[NL80211_BSS_SIGNAL_MBM])
		info->beacon_signal =
			nl80211_xbm_to_percent (nla_get_u32 (bss[NL80211_BSS_SIGNAL_MBM]), 100);

	if (bss[NL80211_BSS_INFORMATION_ELEMENTS]) {
		guint8 *ssid;
		guint32 ssid_len;

		find_ssid(nla_data (bss[NL80211_BSS_INFORMATION_ELEMENTS]),
			  nla_len (bss[NL80211_BSS_INFORMATION_ELEMENTS]),
			  &ssid, &ssid_len);
		if (ssid && ssid_len && ssid_len <= sizeof(info->ssid)) {
			memcpy (info->ssid, ssid, ssid_len);
			info->ssid_len = ssid_len;
		}
	}

	info->valid = TRUE;

	return NL_SKIP;
}

static void nl80211_get_bss_info (WifiDataNl80211 *nl80211,
				  struct nl80211_bss_info *bss_info)
{
	struct nl_msg *msg;

	memset(bss_info, 0, sizeof(*bss_info));

	msg = nl80211_alloc_msg (nl80211, NL80211_CMD_GET_SCAN, NLM_F_DUMP);

	nl80211_send_and_recv (nl80211, msg, nl80211_bss_dump_handler, bss_info);
}

static guint32
wifi_nl80211_get_freq (WifiData *data)
{
	WifiDataNl80211 *nl80211 = (WifiDataNl80211 *) data;
	struct nl80211_bss_info bss_info;

	nl80211_get_bss_info (nl80211, &bss_info);

	return bss_info.freq;
}

static guint32
wifi_nl80211_find_freq (WifiData *data, const guint32 *freqs)
{
	WifiDataNl80211 *nl80211 = (WifiDataNl80211 *) data;
	int i;

	for (i = 0; i < nl80211->num_freqs; i++) {
		while (*freqs) {
			if (nl80211->freqs[i] == *freqs)
				return *freqs;
			freqs++;
		}
	}
	return 0;
}

static GByteArray *
wifi_nl80211_get_ssid (WifiData *data)
{
	WifiDataNl80211 *nl80211 = (WifiDataNl80211 *) data;
	GByteArray *array = NULL;
	struct nl80211_bss_info bss_info;

	nl80211_get_bss_info (nl80211, &bss_info);

	if (bss_info.valid) {
		array = g_byte_array_sized_new (bss_info.ssid_len);
		g_byte_array_append (array, (const guint8 *) bss_info.ssid,
				     bss_info.ssid_len);
	}

	return array;
}

static gboolean
wifi_nl80211_get_bssid (WifiData *data, struct ether_addr *out_bssid)
{
	WifiDataNl80211 *nl80211 = (WifiDataNl80211 *) data;
	struct nl80211_bss_info bss_info;

	nl80211_get_bss_info (nl80211, &bss_info);

	if (bss_info.valid)
		memcpy(out_bssid, bss_info.bssid, ETH_ALEN);

	return bss_info.valid;
}

struct nl80211_station_info {
	guint32 txrate;
	gboolean txrate_valid;
	guint8 signal;
	gboolean signal_valid;
};

static int nl80211_station_handler (struct nl_msg *msg, void *arg)
{
	struct nl80211_station_info *info = arg;
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data (nlmsg_hdr (msg));
	struct nlattr *sinfo[NL80211_STA_INFO_MAX + 1];
	struct nlattr *rinfo[NL80211_RATE_INFO_MAX + 1];
	static struct nla_policy stats_policy[NL80211_STA_INFO_MAX + 1] = {
		[NL80211_STA_INFO_INACTIVE_TIME] = { .type = NLA_U32 },
		[NL80211_STA_INFO_RX_BYTES] = { .type = NLA_U32 },
		[NL80211_STA_INFO_TX_BYTES] = { .type = NLA_U32 },
		[NL80211_STA_INFO_RX_PACKETS] = { .type = NLA_U32 },
		[NL80211_STA_INFO_TX_PACKETS] = { .type = NLA_U32 },
		[NL80211_STA_INFO_SIGNAL] = { .type = NLA_U8 },
		[NL80211_STA_INFO_TX_BITRATE] = { .type = NLA_NESTED },
		[NL80211_STA_INFO_LLID] = { .type = NLA_U16 },
		[NL80211_STA_INFO_PLID] = { .type = NLA_U16 },
		[NL80211_STA_INFO_PLINK_STATE] = { .type = NLA_U8 },
	};

	static struct nla_policy rate_policy[NL80211_RATE_INFO_MAX + 1] = {
		[NL80211_RATE_INFO_BITRATE] = { .type = NLA_U16 },
		[NL80211_RATE_INFO_MCS] = { .type = NLA_U8 },
		[NL80211_RATE_INFO_40_MHZ_WIDTH] = { .type = NLA_FLAG },
		[NL80211_RATE_INFO_SHORT_GI] = { .type = NLA_FLAG },
	};

	if (nla_parse (tb, NL80211_ATTR_MAX, genlmsg_attrdata (gnlh, 0),
		       genlmsg_attrlen (gnlh, 0), NULL) < 0)
		return NL_SKIP;

	if (tb[NL80211_ATTR_STA_INFO] == NULL)
		return NL_SKIP;

	if (nla_parse_nested (sinfo, NL80211_STA_INFO_MAX,
			      tb[NL80211_ATTR_STA_INFO],
			      stats_policy))
		return NL_SKIP;

	if (sinfo[NL80211_STA_INFO_TX_BITRATE] == NULL)
		return NL_SKIP;

	if (nla_parse_nested (rinfo, NL80211_RATE_INFO_MAX,
			      sinfo[NL80211_STA_INFO_TX_BITRATE],
			      rate_policy))
		return NL_SKIP;

	if (rinfo[NL80211_RATE_INFO_BITRATE] == NULL)
		return NL_SKIP;

	/* convert from nl80211's units of 100kbps to NM's kbps */
	info->txrate = nla_get_u16 (rinfo[NL80211_RATE_INFO_BITRATE]) * 100;
	info->txrate_valid = TRUE;

	if (sinfo[NL80211_STA_INFO_SIGNAL] != NULL) {
		info->signal = nl80211_xbm_to_percent ((gint8) nla_get_u8 (sinfo[NL80211_STA_INFO_SIGNAL]), 1);
		info->signal_valid = TRUE;
	}

	return NL_SKIP;
}

static void nl80211_get_ap_info (WifiDataNl80211 *nl80211,
				 struct nl80211_station_info *sta_info)
{
	struct nl_msg *msg;
	struct nl80211_bss_info bss_info;

	memset(sta_info, 0, sizeof(*sta_info));

	nl80211_get_bss_info (nl80211, &bss_info);
	if (!bss_info.valid)
		return;

	msg = nl80211_alloc_msg (nl80211, NL80211_CMD_GET_STATION, 0);
	if (msg) {
		NLA_PUT (msg, NL80211_ATTR_MAC, ETH_ALEN, bss_info.bssid);

		nl80211_send_and_recv (nl80211, msg, nl80211_station_handler, sta_info);
		if (!sta_info->signal_valid) {
			/* Fall back to bss_info signal quality (both are in percent) */
			sta_info->signal = bss_info.beacon_signal;
		}
	}

	return;

 nla_put_failure:
	nlmsg_free (msg);
	return;
}

static guint32
wifi_nl80211_get_rate (WifiData *data)
{
	WifiDataNl80211 *nl80211 = (WifiDataNl80211 *) data;
	struct nl80211_station_info sta_info;

	nl80211_get_ap_info (nl80211, &sta_info);

	return sta_info.txrate;
}

static int
wifi_nl80211_get_qual (WifiData *data)
{
	WifiDataNl80211 *nl80211 = (WifiDataNl80211 *) data;
	struct nl80211_station_info sta_info;

	nl80211_get_ap_info (nl80211, &sta_info);
	return sta_info.signal;
}

struct nl80211_device_info {
	guint32 *freqs;
	int num_freqs;
	guint32 caps;
	gboolean can_scan;
	gboolean can_scan_ssid;
	gboolean supported;
	gboolean success;
};

#define WLAN_CIPHER_SUITE_USE_GROUP 0x000FAC00
#define WLAN_CIPHER_SUITE_WEP40     0x000FAC01
#define WLAN_CIPHER_SUITE_TKIP      0x000FAC02
#define WLAN_CIPHER_SUITE_CCMP      0x000FAC04
#define WLAN_CIPHER_SUITE_WEP104    0x000FAC05
#define WLAN_CIPHER_SUITE_AES_CMAC  0x000FAC06
#define WLAN_CIPHER_SUITE_GCMP      0x000FAC08
#define WLAN_CIPHER_SUITE_SMS4      0x00147201

static int nl80211_wiphy_info_handler (struct nl_msg *msg, void *arg)
{
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data (nlmsg_hdr (msg));
	struct nl80211_device_info *info = arg;
	struct nlattr *tb_band[NL80211_BAND_ATTR_MAX + 1];
	struct nlattr *tb_freq[NL80211_FREQUENCY_ATTR_MAX + 1];
	struct nlattr *nl_band;
	struct nlattr *nl_freq;
	int rem_freq;
	int rem_band;
	int freq_idx;
	static struct nla_policy freq_policy[NL80211_FREQUENCY_ATTR_MAX + 1] = {
		[NL80211_FREQUENCY_ATTR_FREQ] = { .type = NLA_U32 },
		[NL80211_FREQUENCY_ATTR_DISABLED] = { .type = NLA_FLAG },
		[NL80211_FREQUENCY_ATTR_PASSIVE_SCAN] = { .type = NLA_FLAG },
		[NL80211_FREQUENCY_ATTR_NO_IBSS] = { .type = NLA_FLAG },
		[NL80211_FREQUENCY_ATTR_RADAR] = { .type = NLA_FLAG },
		[NL80211_FREQUENCY_ATTR_MAX_TX_POWER] = { .type = NLA_U32 },
	};

	if (nla_parse (tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		       genlmsg_attrlen (gnlh, 0), NULL) < 0)
		return NL_SKIP;

	if (tb[NL80211_ATTR_WIPHY_BANDS] == NULL)
		return NL_SKIP;

	if (tb[NL80211_ATTR_MAX_NUM_SCAN_SSIDS]) {
		info->can_scan_ssid =
			nla_get_u8 (tb[NL80211_ATTR_MAX_NUM_SCAN_SSIDS]) > 0;
	} else {
		/* old kernel that only had mac80211, so assume it can */
		info->can_scan_ssid = TRUE;
	}

	if (tb[NL80211_ATTR_SUPPORTED_COMMANDS]) {
		struct nlattr *nl_cmd;
		int i;

		nla_for_each_nested (nl_cmd, tb[NL80211_ATTR_SUPPORTED_COMMANDS], i) {
			switch (nla_get_u32 (nl_cmd)) {
			case NL80211_CMD_TRIGGER_SCAN:
				info->can_scan = TRUE;
				break;
			case NL80211_CMD_CONNECT:
			case NL80211_CMD_AUTHENTICATE:
				/* Only devices that support CONNECT or AUTH actually support
				 * 802.11, unlike say ipw2x00 (up to at least kernel 3.4) which
				 * has minimal info support, but no actual command support.
				 * This check mirrors what wpa_supplicant does to determine
				 * whether or not to use the nl80211 driver.
				 */
				info->supported = TRUE;
				break;
			default:
				break;
			}
		}
	}

	info->num_freqs = 0;

	nla_for_each_nested (nl_band, tb[NL80211_ATTR_WIPHY_BANDS], rem_band) {
		if (nla_parse_nested (tb_band, NL80211_BAND_ATTR_MAX, nl_band,
				      NULL) < 0)
			return NL_SKIP;

		nla_for_each_nested(nl_freq, tb_band[NL80211_BAND_ATTR_FREQS],
				    rem_freq) {
			nla_parse_nested (tb_freq, NL80211_FREQUENCY_ATTR_MAX,
					  nl_freq, freq_policy);

			if (!tb_freq[NL80211_FREQUENCY_ATTR_FREQ])
				continue;

			info->num_freqs++;
		}
	}

	info->freqs = g_malloc0 (sizeof(guint32) * info->num_freqs);

	freq_idx = 0;
	nla_for_each_nested (nl_band, tb[NL80211_ATTR_WIPHY_BANDS], rem_band) {
		if (nla_parse_nested (tb_band, NL80211_BAND_ATTR_MAX, nl_band,
				      NULL) < 0)
			return NL_SKIP;

		nla_for_each_nested(nl_freq, tb_band[NL80211_BAND_ATTR_FREQS],
				    rem_freq) {
			nla_parse_nested (tb_freq, NL80211_FREQUENCY_ATTR_MAX,
					  nl_freq, freq_policy);

			if (!tb_freq[NL80211_FREQUENCY_ATTR_FREQ])
				continue;

			info->freqs[freq_idx] =
				nla_get_u32 (tb_freq[NL80211_FREQUENCY_ATTR_FREQ]);
			freq_idx++;
		}
	}

	if (tb[NL80211_ATTR_CIPHER_SUITES]) {
		int num;
		int i;
		__u32 *ciphers = nla_data (tb[NL80211_ATTR_CIPHER_SUITES]);

		num = nla_len (tb[NL80211_ATTR_CIPHER_SUITES]) / sizeof(__u32);
		for (i = 0; i < num; i++) {
			switch (ciphers[i]) {
			case WLAN_CIPHER_SUITE_WEP40:
				info->caps |= NM_WIFI_DEVICE_CAP_CIPHER_WEP40;
				break;
			case WLAN_CIPHER_SUITE_WEP104:
				info->caps |= NM_WIFI_DEVICE_CAP_CIPHER_WEP104;
				break;
			case WLAN_CIPHER_SUITE_TKIP:
				info->caps |= NM_WIFI_DEVICE_CAP_CIPHER_TKIP |
					      NM_WIFI_DEVICE_CAP_WPA;
				break;
			case WLAN_CIPHER_SUITE_CCMP:
				info->caps |= NM_WIFI_DEVICE_CAP_CIPHER_CCMP |
					      NM_WIFI_DEVICE_CAP_RSN;
				break;
			case WLAN_CIPHER_SUITE_AES_CMAC:
			case WLAN_CIPHER_SUITE_GCMP:
			case WLAN_CIPHER_SUITE_SMS4:
				break;
			default:
				nm_log_dbg (LOGD_HW | LOGD_WIFI, "Don't know the meaning of NL80211_ATTR_CIPHER_SUITE %#8.8x.", ciphers[i]);
				break;
			}
		}
	}

	if (tb[NL80211_ATTR_SUPPORTED_IFTYPES]) {
		struct nlattr *nl_mode;
		int i;

		nla_for_each_nested (nl_mode, tb[NL80211_ATTR_SUPPORTED_IFTYPES], i) {
			if (nla_type (nl_mode) == NL80211_IFTYPE_AP)
				info->caps |= NM_WIFI_DEVICE_CAP_AP;
			else if (nla_type (nl_mode) == NL80211_IFTYPE_ADHOC)
				info->caps |= NM_WIFI_DEVICE_CAP_ADHOC;
		}
	}

	info->success = TRUE;

	return NL_SKIP;
}

WifiData *
wifi_nl80211_init (const char *iface, int ifindex)
{
	WifiDataNl80211 *nl80211;
	struct nl_msg *msg;
	struct nl80211_device_info device_info = {};

	nl80211 = wifi_data_new (iface, ifindex, sizeof (*nl80211));
	nl80211->parent.get_mode = wifi_nl80211_get_mode;
	nl80211->parent.set_mode = wifi_nl80211_set_mode;
	nl80211->parent.get_freq = wifi_nl80211_get_freq;
	nl80211->parent.find_freq = wifi_nl80211_find_freq;
	nl80211->parent.get_ssid = wifi_nl80211_get_ssid;
	nl80211->parent.get_bssid = wifi_nl80211_get_bssid;
	nl80211->parent.get_rate = wifi_nl80211_get_rate;
	nl80211->parent.get_qual = wifi_nl80211_get_qual;
	nl80211->parent.deinit = wifi_nl80211_deinit;

	nl80211->nl_sock = nl_socket_alloc ();
	if (nl80211->nl_sock == NULL)
		goto error;

	if (genl_connect (nl80211->nl_sock))
		goto error;

	nl80211->id = genl_ctrl_resolve (nl80211->nl_sock, "nl80211");
	if (nl80211->id < 0)
		goto error;

	nl80211->nl_cb = nl_cb_alloc (NL_CB_DEFAULT);
	if (nl80211->nl_cb == NULL)
		goto error;

	msg = nl80211_alloc_msg (nl80211, NL80211_CMD_GET_WIPHY, 0);

	if (nl80211_send_and_recv (nl80211, msg, nl80211_wiphy_info_handler,
				   &device_info) < 0) {
		nm_log_dbg (LOGD_HW | LOGD_WIFI,
				    "(%s): NL80211_CMD_GET_WIPHY request failed",
				    nl80211->parent.iface);
		goto error;
	}

	if (!device_info.success) {
		nm_log_dbg (LOGD_HW | LOGD_WIFI,
				    "(%s): NL80211_CMD_GET_WIPHY request indicated failure",
				    nl80211->parent.iface);
		goto error;
	}

	if (!device_info.supported) {
		nm_log_dbg (LOGD_HW | LOGD_WIFI,
				    "(%s): driver does not fully support nl80211, falling back to WEXT",
				    nl80211->parent.iface);
		goto error;
	}

	if (!device_info.can_scan_ssid) {
		nm_log_err (LOGD_HW | LOGD_WIFI,
		            "(%s): driver does not support SSID scans",
		            nl80211->parent.iface);
		goto error;
	}

	if (device_info.num_freqs == 0 || device_info.freqs == NULL) {
		nm_log_err (LOGD_HW | LOGD_WIFI,
				    "(%s): driver reports no supported frequencies",
				    nl80211->parent.iface);
		goto error;
	}

	if (device_info.caps == 0) {
		nm_log_err (LOGD_HW | LOGD_WIFI,
		            "(%s): driver doesn't report support of any encryption",
		            nl80211->parent.iface);
		goto error;
	}

	nl80211->freqs = device_info.freqs;
	nl80211->num_freqs = device_info.num_freqs;
	nl80211->parent.can_scan_ssid = device_info.can_scan_ssid;
	nl80211->parent.caps = device_info.caps;

	nm_log_info (LOGD_HW | LOGD_WIFI,
	             "(%s): using nl80211 for WiFi device control",
	             nl80211->parent.iface);

	return (WifiData *) nl80211;

error:
	wifi_utils_deinit ((WifiData *) nl80211);
	return NULL;
}

static int
iface_to_index (struct nl_sock *nl_sock, const char *iface)
{
	struct nl_cache *link_cache = NULL;
	int err, ifindex;

	/* name to index */
	err = rtnl_link_alloc_cache (nl_sock, AF_UNSPEC, &link_cache);
	if (err < 0) {
		nm_log_warn (LOGD_HW, "failed to allocate link cache: (%d) %s",
		             err, nl_geterror (err));
		return -1;
	}
	nl_cache_mngt_provide (link_cache);
	nl_cache_refill (nl_sock, link_cache);
	ifindex = rtnl_link_name2i (link_cache, iface);
	nl_cache_free (link_cache);

	return ifindex;
}

gboolean
wifi_nl80211_is_wifi (const char *iface)
{
	struct nl_sock *nl_sock;
	struct nl_cb *nl_cb = NULL;
	struct nl_msg *msg = NULL;
	int id, ifindex;
	struct nl80211_iface_info iface_info = {
		.mode = NM_802_11_MODE_UNKNOWN,
	};
	gboolean is_wifi = FALSE;

	nl_sock = nl_socket_alloc ();
	if (nl_sock == NULL)
		return FALSE;

	if (genl_connect (nl_sock))
		goto error;

	ifindex = iface_to_index (nl_sock, iface);
	if (ifindex < 0)
		goto error;

	id = genl_ctrl_resolve (nl_sock, "nl80211");
	if (id < 0)
		goto error;

	nl_cb = nl_cb_alloc (NL_CB_DEFAULT);
	if (nl_cb) {
		msg = _nl80211_alloc_msg (id, ifindex, NL80211_CMD_GET_INTERFACE, 0);
		if (_nl80211_send_and_recv (nl_sock,
			                        nl_cb,
			                        msg,
			                        nl80211_iface_info_handler,
			                        &iface_info) >= 0)
			is_wifi = (iface_info.mode != NM_802_11_MODE_UNKNOWN);
	}

 error:
	if (nl_cb)
		nl_cb_put (nl_cb);
	nl_socket_free (nl_sock);
	return is_wifi;
}

