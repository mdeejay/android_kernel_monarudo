/*
 * Common function shared by Linux WEXT, cfg80211 and p2p drivers
 *
 * Copyright (C) 1999-2012, Broadcom Corporation
 * 
 *      Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2 (the "GPL"),
 * available at http://www.broadcom.com/licenses/GPLv2.php, with the
 * following added to such license:
 * 
 *      As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy and
 * distribute the resulting executable under terms of your choice, provided that
 * you also meet, for each linked independent module, the terms and conditions of
 * the license of that module.  An independent module is a module which is not
 * derived from this software.  The special exception does not apply to any
 * modifications of the software.
 * 
 *      Notwithstanding the above, under no circumstances may you combine this
 * software in any way with any other Broadcom software provided under a license
 * other than the GPL, without Broadcom's express prior written consent.
 *
 * $Id: wldev_common.c,v 1.1.4.1.2.14 2011-02-09 01:40:07 $
 */

#include <osl.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/netdevice.h>

#include <wldev_common.h>
#include <bcmutils.h>

#define htod32(i) i
#define htod16(i) i
#define dtoh32(i) i
#define dtoh16(i) i
#define htodchanspec(i) i
#define dtohchanspec(i) i

#define	WLDEV_ERROR(args)						\
	do {										\
		printk(KERN_ERR "WLDEV-ERROR) %s : ", __func__);	\
		printk args;							\
	} while (0)

extern int dhd_ioctl_entry_local(struct net_device *net, wl_ioctl_t *ioc, int cmd);

s32 wldev_ioctl(
	struct net_device *dev, u32 cmd, void *arg, u32 len, u32 set)
{
	s32 ret = 0;
	struct wl_ioctl ioc;


	memset(&ioc, 0, sizeof(ioc));
	ioc.cmd = cmd;
	ioc.buf = arg;
	ioc.len = len;
	ioc.set = set;
	ret = dhd_ioctl_entry_local(dev, &ioc, cmd);


	return ret;
}

/*HTC_CSP_START*/
s32 wldev_ioctl_no_memset(
	struct net_device *dev, u32 cmd, void *arg, u32 len, u32 set)
{
	s32 ret = 0;
	struct wl_ioctl ioc;

	ioc.cmd = cmd;
	ioc.buf = arg;
	ioc.len = len;
	ioc.set = set;
	ret = dhd_ioctl_entry_local(dev, &ioc, cmd);

	return ret;
}
/*HTC_CSP_END*/

/* Format a iovar buffer, not bsscfg indexed. The bsscfg index will be
 * taken care of in dhd_ioctl_entry. Internal use only, not exposed to
 * wl_iw, wl_cfg80211 and wl_cfgp2p
 */
static s32 wldev_mkiovar(
	s8 *iovar_name, s8 *param, s32 paramlen,
	s8 *iovar_buf, u32 buflen)
{
	s32 iolen = 0;

	iolen = bcm_mkiovar(iovar_name, param, paramlen, iovar_buf, buflen);
	return iolen;
}

s32 wldev_iovar_getbuf(
	struct net_device *dev, s8 *iovar_name,
	void *param, s32 paramlen, void *buf, s32 buflen, struct mutex* buf_sync)
{
	s32 ret = 0;
	if (buf_sync) {
		mutex_lock(buf_sync);
	}
	wldev_mkiovar(iovar_name, param, paramlen, buf, buflen);
	ret = wldev_ioctl(dev, WLC_GET_VAR, buf, buflen, FALSE);
	if (buf_sync)
		mutex_unlock(buf_sync);
	return ret;
}


s32 wldev_iovar_setbuf(
	struct net_device *dev, s8 *iovar_name,
	void *param, s32 paramlen, void *buf, s32 buflen, struct mutex* buf_sync)
{
	s32 ret = 0;
	s32 iovar_len;
	if (buf_sync) {
		mutex_lock(buf_sync);
	}
	iovar_len = wldev_mkiovar(iovar_name, param, paramlen, buf, buflen);
	if (iovar_len > 0)
		ret = wldev_ioctl(dev, WLC_SET_VAR, buf, iovar_len, TRUE);
	else {
		if (buf_sync) {
			mutex_unlock(buf_sync);
		}
		return BCME_BUFTOOSHORT;
	}
	if (buf_sync)
		mutex_unlock(buf_sync);
	return ret;
}

s32 wldev_iovar_setint(
	struct net_device *dev, s8 *iovar, s32 val)
{
	s8 iovar_buf[WLC_IOCTL_SMLEN];

	val = htod32(val);
	memset(iovar_buf, 0, sizeof(iovar_buf));
	return wldev_iovar_setbuf(dev, iovar, &val, sizeof(val), iovar_buf,
		sizeof(iovar_buf), NULL);
}


s32 wldev_iovar_getint(
	struct net_device *dev, s8 *iovar, s32 *pval)
{
	s8 iovar_buf[WLC_IOCTL_SMLEN];
	s32 err;

	memset(iovar_buf, 0, sizeof(iovar_buf));
	err = wldev_iovar_getbuf(dev, iovar, pval, sizeof(*pval), iovar_buf,
		sizeof(iovar_buf), NULL);
	if (err == 0)
	{
		memcpy(pval, iovar_buf, sizeof(*pval));
		*pval = dtoh32(*pval);
	}
	return err;
}

/** Format a bsscfg indexed iovar buffer. The bsscfg index will be
 *  taken care of in dhd_ioctl_entry. Internal use only, not exposed to
 *  wl_iw, wl_cfg80211 and wl_cfgp2p
 */
s32 wldev_mkiovar_bsscfg(
	const s8 *iovar_name, s8 *param, s32 paramlen,
	s8 *iovar_buf, s32 buflen, s32 bssidx)
{
	const s8 *prefix = "bsscfg:";
	s8 *p;
	u32 prefixlen;
	u32 namelen;
	u32 iolen;

	if (bssidx == 0) {
		return wldev_mkiovar((s8*)iovar_name, (s8 *)param, paramlen,
			(s8 *) iovar_buf, buflen);
	}

	prefixlen = (u32) strlen(prefix); /* lengh of bsscfg prefix */
	namelen = (u32) strlen(iovar_name) + 1; /* lengh of iovar  name + null */
	iolen = prefixlen + namelen + sizeof(u32) + paramlen;

	if (buflen < 0 || iolen > (u32)buflen)
	{
		WLDEV_ERROR(("%s: buffer is too short\n", __FUNCTION__));
		return BCME_BUFTOOSHORT;
	}

	p = (s8 *)iovar_buf;

	/* copy prefix, no null */
	memcpy(p, prefix, prefixlen);
	p += prefixlen;

	/* copy iovar name including null */
	memcpy(p, iovar_name, namelen);
	p += namelen;

	/* bss config index as first param */
	bssidx = htod32(bssidx);
	memcpy(p, &bssidx, sizeof(u32));
	p += sizeof(u32);

	/* parameter buffer follows */
	if (paramlen)
		memcpy(p, param, paramlen);

	return iolen;

}

s32 wldev_iovar_getbuf_bsscfg(
	struct net_device *dev, s8 *iovar_name,
	void *param, s32 paramlen, void *buf, s32 buflen, s32 bsscfg_idx, struct mutex* buf_sync)
{
	s32 ret = 0;
	if (buf_sync) {
		mutex_lock(buf_sync);
	}

	wldev_mkiovar_bsscfg(iovar_name, param, paramlen, buf, buflen, bsscfg_idx);
	ret = wldev_ioctl(dev, WLC_GET_VAR, buf, buflen, FALSE);
	if (buf_sync) {
		mutex_unlock(buf_sync);
	}
	return ret;

}

s32 wldev_iovar_setbuf_bsscfg(
	struct net_device *dev, s8 *iovar_name,
	void *param, s32 paramlen, void *buf, s32 buflen, s32 bsscfg_idx, struct mutex* buf_sync)
{
	s32 ret = 0;
	s32 iovar_len;
	if (buf_sync) {
		mutex_lock(buf_sync);
	}
	iovar_len = wldev_mkiovar_bsscfg(iovar_name, param, paramlen, buf, buflen, bsscfg_idx);
	if (iovar_len > 0)
		ret = wldev_ioctl(dev, WLC_SET_VAR, buf, iovar_len, TRUE);
	else {
		ret = BCME_BUFTOOSHORT;
	}
	if (buf_sync) {
		mutex_unlock(buf_sync);
	}
	return ret;
}

s32 wldev_iovar_setint_bsscfg(
	struct net_device *dev, s8 *iovar, s32 val, s32 bssidx)
{
	s8 iovar_buf[WLC_IOCTL_SMLEN];

	val = htod32(val);
	memset(iovar_buf, 0, sizeof(iovar_buf));
	return wldev_iovar_setbuf_bsscfg(dev, iovar, &val, sizeof(val), iovar_buf,
		sizeof(iovar_buf), bssidx, NULL);
}


s32 wldev_iovar_getint_bsscfg(
	struct net_device *dev, s8 *iovar, s32 *pval, s32 bssidx)
{
	s8 iovar_buf[WLC_IOCTL_SMLEN];
	s32 err;

	memset(iovar_buf, 0, sizeof(iovar_buf));
	err = wldev_iovar_getbuf_bsscfg(dev, iovar, pval, sizeof(*pval), iovar_buf,
		sizeof(iovar_buf), bssidx, NULL);
	if (err == 0)
	{
		memcpy(pval, iovar_buf, sizeof(*pval));
		*pval = dtoh32(*pval);
	}
	return err;
}

int wldev_get_link_speed(
	struct net_device *dev, int *plink_speed)
{
	int error;

	if (!plink_speed)
		return -ENOMEM;
	error = wldev_ioctl(dev, WLC_GET_RATE, plink_speed, sizeof(int), 0);
	if (unlikely(error))
		return error;

	/* Convert internal 500Kbps to Kbps */
	*plink_speed *= 500;
	return error;
}

int wldev_get_rssi(
	struct net_device *dev, int *prssi)
{
	scb_val_t scb_val;
	int error;

	if (!prssi)
		return -ENOMEM;
	bzero(&scb_val, sizeof(scb_val_t));

	error = wldev_ioctl(dev, WLC_GET_RSSI, &scb_val, sizeof(scb_val_t), 0);
	if (unlikely(error))
		return error;

	*prssi = dtoh32(scb_val.val);
	return error;
}

int wldev_get_ssid(
	struct net_device *dev, wlc_ssid_t *pssid)
{
	int error;

	if (!pssid)
		return -ENOMEM;
	error = wldev_ioctl(dev, WLC_GET_SSID, pssid, sizeof(wlc_ssid_t), 0);
	if (unlikely(error))
		return error;
	pssid->SSID_len = dtoh32(pssid->SSID_len);
	return error;
}

int wldev_get_band(
	struct net_device *dev, uint *pband)
{
	int error;

	error = wldev_ioctl(dev, WLC_GET_BAND, pband, sizeof(uint), 0);
	return error;
}

int wldev_set_band(
	struct net_device *dev, uint band)
{
	int error = -1;

	if ((band == WLC_BAND_AUTO) || (band == WLC_BAND_5G) || (band == WLC_BAND_2G)) {
		error = wldev_ioctl(dev, WLC_SET_BAND, &band, sizeof(band), 1);
	}
	return error;
}

int wldev_set_country(
	struct net_device *dev, char *country_code)
{
	int error = -1;
	wl_country_t cspec = {{0}, 0, {0}};
	scb_val_t scbval;
	char smbuf[WLC_IOCTL_SMLEN];
	/*HTC_CSP_START*/
	uint32 chan_buf[WL_NUMCHANNELS];
	wl_uint32_list_t *list;
	channel_info_t ci;
	int retry = 0;
	int chan = 1;
	/*HTC_CSP_END*/

	if (!country_code)
		return error;

	error = wldev_iovar_getbuf(dev, "country", &cspec, sizeof(cspec),
		smbuf, sizeof(smbuf), NULL);
	if (error < 0)
		WLDEV_ERROR(("%s: get country failed = %d\n", __FUNCTION__, error));

	if ((error < 0) ||
	    (strncmp(country_code, smbuf, WLC_CNTRY_BUF_SZ) != 0)) {
		bzero(&scbval, sizeof(scb_val_t));
		error = wldev_ioctl(dev, WLC_DISASSOC, &scbval, sizeof(scb_val_t), 1);
		if (error < 0) {
			WLDEV_ERROR(("%s: set country failed due to Disassoc error %d\n",
				__FUNCTION__, error));
			return error;
		}
/*HTC_CSP_START*/
		error = wldev_ioctl(dev, WLC_SET_CHANNEL, &chan, sizeof(chan), 1);
		if (error < 0) {
			WLDEV_ERROR(("%s: set country failed due to channel 1 error %d\n",
				__FUNCTION__, error));
			return error;
		}
	}

get_channel_retry:
	if ((error = wldev_ioctl(dev, WLC_GET_CHANNEL, &ci, sizeof(ci), 0))) {
		WLDEV_ERROR(("%s: get channel fail!\n", __FUNCTION__));
		return error;
	}
	ci.scan_channel = dtoh32(ci.scan_channel);
	if (ci.scan_channel) {
		retry++;
		printf("%s: scan in progress, retry %d!\n", __FUNCTION__, retry);
		if (retry > 3)
			return -EBUSY;
		bcm_mdelay(1000);
		goto get_channel_retry;
	}
/*HTC_CSP_END*/

	cspec.rev = -1;
	memcpy(cspec.country_abbrev, country_code, WLC_CNTRY_BUF_SZ);
	memcpy(cspec.ccode, country_code, WLC_CNTRY_BUF_SZ);
	get_customized_country_code((char *)&cspec.country_abbrev, &cspec);
	error = wldev_iovar_setbuf(dev, "country", &cspec, sizeof(cspec),
		smbuf, sizeof(smbuf), NULL);
	if (error < 0) {
	/*HTC_CSP_START*/
		if (strcmp(cspec.country_abbrev, DEF_COUNTRY_CODE) != 0) {
			strcpy(country_code, DEF_COUNTRY_CODE);
			retry = 0;
			goto get_channel_retry;
		}
		else {
			WLDEV_ERROR(("%s: set country for %s as %s rev %d failed\n",
				__FUNCTION__, country_code, cspec.ccode, cspec.rev));
			return error;
		}
	}
	/* check if there are available channels */
	else {
		if (strcmp(country_code, DEF_COUNTRY_CODE) != 0) {
			list = (wl_uint32_list_t *)(void *)chan_buf;
			list->count = htod32(WL_NUMCHANNELS);
			if ((error = wldev_ioctl_no_memset(dev, WLC_GET_VALID_CHANNELS, &chan_buf, sizeof(chan_buf), 0))) {
				WLDEV_ERROR(("%s: get channel list fail! %d\n", __FUNCTION__, error));
				return error;
			}
			/* if NULL, set default country code instead and set country code again */
			printf("%s: channel_count = %d\n", __FUNCTION__, list->count);
			if (list->count == 0) {
				strcpy(country_code, DEF_COUNTRY_CODE);
				retry = 0;
				goto get_channel_retry;
			}
		}	
	}
	/*HTC_CSP_END*/
	dhd_bus_country_set(dev, &cspec);
	printk(KERN_INFO "[WLAN] %s: set country for %s as %s rev %d\n",
		__func__, country_code, cspec.ccode, cspec.rev);
	return 0;
}

//BRCM APSTA START
extern int is_screen_off;
#ifdef APSTA_CONCURRENT
static int wldev_set_pktfilter_enable_by_id(struct net_device *dev, int pkt_id, int enable)
{
	wl_pkt_filter_enable_t	enable_parm;
	char smbuf[WLC_IOCTL_SMLEN];
	int res;

	/* enable or disable pkt filter, enable:1, disable:0 */
	enable_parm.id = htod32(pkt_id);
	enable_parm.enable = htod32(enable);
	res = wldev_iovar_setbuf(dev, "pkt_filter_enable", &enable_parm, sizeof(enable_parm),
		smbuf, sizeof(smbuf), NULL);
	if (res < 0) {
		WLDEV_ERROR(("%s: set pkt_filter_enable failed, error:%d\n", __FUNCTION__, res));
		return res;
	}
	
	return 0;
}

int wldev_set_pktfilter_enable(struct net_device *dev, int enable)
{
	wldev_set_pktfilter_enable_by_id(dev, 100, enable);
        printf("%s: pkt_filter id:100 %s\n", __FUNCTION__, (enable)?"enable":"disable");
	wldev_set_pktfilter_enable_by_id(dev, 101, enable);
        printf("%s: pkt_filter id:101 %s\n", __FUNCTION__, (enable)?"enable":"disable");
	wldev_set_pktfilter_enable_by_id(dev, 102, enable);
        printf("%s: pkt_filter id:102 %s\n", __FUNCTION__, (enable)?"enable":"disable");
	wldev_set_pktfilter_enable_by_id(dev, 104, enable);
        printf("%s: pkt_filter id:104 %s\n", __FUNCTION__, (enable)?"enable":"disable");
//HTC SSDP packet filter ++
    wldev_set_pktfilter_enable_by_id(dev, 105, enable);
        printf("%s: pkt_filter id:105 %s\n", __FUNCTION__, (enable)?"enable":"disable");
//HTC SSDP packet filter --
//HTC WIVU packet filter ++

	if(!is_screen_off){
    	wldev_set_pktfilter_enable_by_id(dev, 106, enable);
        printf("%s: pkt_filter id:106 %s\n", __FUNCTION__, (enable)?"enable":"disable");
	}
//HTC WIVU packet filter --
        return 0;
}
#ifdef SOFTAP

#define WL_SOFTAP(x) printf x

static struct ap_profile ap_cfg;

extern struct net_device *ap_net_dev;
extern int init_ap_profile_from_string(char *param_str, struct ap_profile *ap_cfg);
extern int set_apsta_cfg(struct net_device *dev, struct ap_profile *ap);
//BRCM_APSTA_START
extern int set_ap_channel(struct net_device *dev, struct ap_profile *ap);
//BRCM_APSTA_END
extern int wait_for_ap_ready(int sec);
extern void wl_iw_restart_apsta(struct ap_profile *ap);
extern int wl_iw_set_ap_security(struct net_device *dev, struct ap_profile *ap);
int
wldev_set_apsta_cfg(struct net_device *dev, char *param_str)
{
	int res = 0;

	printf("%s: enter\n", __FUNCTION__);

        if (!dev) {
                 WLDEV_ERROR(("%s: dev is null\n", __FUNCTION__));
                 return -1;
  	}
	
	init_ap_profile_from_string(param_str, &ap_cfg);

        if ((res = set_apsta_cfg(dev, &ap_cfg)) < 0)
                 WLDEV_ERROR(("%s failed to set_apsta_cfg %d\n", __FUNCTION__, res));
	
	return res;
}
void
wldev_restart_ap(struct net_device *dev)
{
	wl_iw_restart_apsta(&ap_cfg);
}

#endif  /* SOFTAP */

int
wldev_get_ap_status(struct net_device *dev)
{
	int res = 0;
	int ap = 0;
	int apsta = 0;
       	char smbuf[WLC_IOCTL_SMLEN];

	printf("%s: enter\n", __FUNCTION__);

        if (!dev) {
                WLDEV_ERROR(("%s: dev is null\n", __FUNCTION__));
                return -1;
        }

        if ((res = wldev_ioctl(dev, WLC_GET_AP, &ap, sizeof(ap), 0))) {
                WLDEV_ERROR(("%s fail to get ap\n", __FUNCTION__));
		ap = 0;
	}

        if ((res = wldev_iovar_getbuf(dev, "apsta", &apsta, sizeof(apsta), smbuf, sizeof(smbuf), NULL)) < 0 ){
                WLDEV_ERROR(("%s fail to get apsta \n", __FUNCTION__));
        } else {
		memcpy(&apsta, smbuf, sizeof(apsta));
		apsta = dtoh32(apsta);
	}

	return (ap||apsta);
}
//Hugh 2012-04-05 ++++

//HTC_WIFI_START [Hugh 2012-08-21 scan_suppress change ++++]
int scan_suppress_flag = 0;
//HTC_WIFI_END [Hugh 2012-08-21 scan_suppress change ----]

//HTC_WIFI_START [Hugh 2012-08-31 scan_suppress change ++++]
int wldev_start_stop_scansuppress(struct net_device *dev)
{
	int res = 0;
	int enable = 1;
	if (!dev) {
		WLDEV_ERROR(("%s: dev is null\n", __FUNCTION__));
		return (res = -1);;
	}
	
	printf("%s enable[%d]\n",__FUNCTION__ ,enable);


	 if(scan_suppress_flag){
		if ((res = wldev_ioctl(dev, WLC_SET_SCANSUPPRESS, &enable, sizeof(enable), 1))) {
			WLDEV_ERROR(("%s fail to SET_SCANSUPPRESS enable\n", __FUNCTION__));
			return (res = -1);
		}
		printf("Successful Enable scan suppress!!\n");
		return 0;
	}else
		printf("scan_suppress_flag not set do nothing!!\n");
		
		return 0;

}
//HTC_WIFI_START [Hugh 2012-08-21 scan_suppress change -----]



int
wldev_set_scansuppress(struct net_device *dev,int enable)
{
	int res = 0;
//HTC_WIFI_START [Hugh 2012-08-21 scan_suppress change ++++]
//	int roam_off;
//HTC_WIFI_START [Hugh 2012-08-21 scan_suppress change ----]
	int scan_unassoc_time ;

	printf("%s: enter\n", __FUNCTION__);

	if (!dev) {
		WLDEV_ERROR(("%s: dev is null\n", __FUNCTION__));
		return -1;
	}

	printf("wldev_set_scansuppress enable[%d]\n", enable);

	if(enable){
//HTC_WIFI_START [Hugh 2012-08-21 scan_suppress change ++++]
#if 1
//HTC_WIFI_START [Hugh 2012-08-21 scan_suppress change ----]
		scan_unassoc_time = 20;
		
		if((res =  wldev_ioctl(dev, WLC_SET_SCAN_UNASSOC_TIME, (char *)&scan_unassoc_time,sizeof(scan_unassoc_time), 1))) {
					WLDEV_ERROR(("%s fail to set  WLC_SET_SCAN_UNASSOC_TIME\n", __FUNCTION__));
		}

		if ((res = wldev_ioctl(dev, WLC_SET_SCANSUPPRESS, &enable, sizeof(enable), 1))) {
			WLDEV_ERROR(("%s fail to get ap\n", __FUNCTION__));
		}
		scan_suppress_flag = 1;
//HTC_WIFI_START [Hugh 2012-08-21 scan_suppress change ++++]
#else
	
	dhd_wl_ioctl_cmd(dhd, WLC_SET_SCAN_UNASSOC_TIME, (char *)&scan_unassoc_time,
		sizeof(scan_unassoc_time), TRUE, 0);

	roam_off = 1;
	if((res = wldev_iovar_setint(dev, "roam_off", roam_off))){
		printf("%s fail to set roam_off[%d]\n", __FUNCTION__,roam_off);
		return -1;
	}else{
		printf("Set roam_off 1 and enable scansuppress_flag!!\n");
		scan_suppress_flag = 1;
	}
#endif
//HTC_WIFI_START [Hugh 2012-08-21 scan_suppress change ----]
		printf("Successful enable scan suppress!!\n");
	}else{
//HTC_WIFI_START [Hugh 2012-08-21 scan_suppress change ++++]
#if 1
//HTC_WIFI_START [Hugh 2012-08-21 scan_suppress change ----]

		scan_unassoc_time = 80;
		
		if ((res =  wldev_ioctl(dev, WLC_SET_SCAN_UNASSOC_TIME, (char *)&scan_unassoc_time,sizeof(scan_unassoc_time), 1))) {
					WLDEV_ERROR(("%s fail to set  WLC_SET_SCAN_UNASSOC_TIME\n", __FUNCTION__));
		}

		if ((res = wldev_ioctl(dev, WLC_SET_SCANSUPPRESS, &enable, sizeof(enable), 1))) {
			WLDEV_ERROR(("%s fail to get ap\n", __FUNCTION__));		
		}
		scan_suppress_flag = 0;
		
//HTC_WIFI_START [Hugh 2012-08-21 scan_suppress change ++++]
#else
	roam_off = 0;
	if((res = wldev_iovar_setint(dev, "roam_off", roam_off))){
		printf("%s fail to set roam_off[%d]\n", __FUNCTION__,roam_off);
		return -1;
	}else{
		printf("set roam_off to zero and disable scansuppress_flag!!\n");
		scan_suppress_flag = 0;
	}
#endif
//HTC_WIFI_START [Hugh 2012-08-21 scan_suppress change ----]
		printf("Successful disable scan suppress!!\n");
	}
	
	return 0;
}

//Hugh 2012-04-05 ++++
extern int wl_softap_stop(struct net_device *dev);

//Hugh 2012-08-30 CT2 modification ++++
#ifdef APSTA_CONCURRENT
s32 wldev_set_ssid(struct net_device *dev,int *channel)
{
	wlc_ssid_t ap_ssid;
	bss_setbuf_t bss_setbuf;
	char smbuf[WLC_IOCTL_SMLEN];

	int res = 0;
	printf("  %s  ap_cfg.ssid[%s]\n",__FUNCTION__, ap_cfg.ssid);
	ap_ssid.SSID_len = strlen(ap_cfg.ssid);
	strncpy(ap_ssid.SSID, ap_cfg.ssid, ap_ssid.SSID_len);

	bss_setbuf.cfg = 1;
	bss_setbuf.val = 0;  /* down the interface */
	
	if ((res = wldev_iovar_setbuf_bsscfg(dev, "bss", &bss_setbuf, sizeof(bss_setbuf), 
			smbuf, sizeof(smbuf), 1, NULL)) < 0){
			printf("%s: ERROR:%d, set bss down failed\n", __FUNCTION__, res);
	}
	
	bcm_mdelay(50);

	res = wldev_ioctl(ap_net_dev, WLC_SET_CHANNEL, channel, sizeof(*channel), 1);
	if (res < 0) {
		printf("%s set channel fial res[%d]",__FUNCTION__,res);
	}


	bss_setbuf.cfg = 1;
	bss_setbuf.val = 1;  /* up the interface */
	bcm_mdelay(50);

	if ((res = wldev_iovar_setbuf_bsscfg(dev, "bss", &bss_setbuf, sizeof(bss_setbuf), 
			smbuf, sizeof(smbuf), 1, NULL)) < 0){
			printf("%s: ERROR:%d, set bss up failed\n", __FUNCTION__, res);
	}
			printf("%s: up Conap part of apsta concurrent\n", __FUNCTION__);
		
	return res;

}
#endif
//Hugh 2012-08-30 CT2 modification ----

int
wldev_set_apsta(struct net_device *dev, bool enable)
{
   	int res = 0;
   	int mpc = 0;
   	int concr_mode = 0;
	int roam_off;
   	char smbuf[WLC_IOCTL_SMLEN];
	bss_setbuf_t bss_setbuf;
	int frameburst;

        memset(smbuf, 0, sizeof(smbuf));

	printf("%s: enter\n", __FUNCTION__);

   	if (!dev) {
                  printf("%s: dev is null\n", __FUNCTION__);
                  return -1;
   	}

	if (enable){
		/* wait for interface ready */
		wait_for_ap_ready(1); //broacom 0405

		if ( ap_net_dev == NULL ) {
                        printf("%s ap_net_dev == NULL\n", __FUNCTION__);
                        goto fail;
		}

		//Hugh 2012-09-10 CT2 set Concr_mode ++++
				concr_mode = 1;
				if ((res = wldev_iovar_setint(dev, "concr_mode_set", concr_mode))) {
						printf("%s fail to set concr_mode res 1[%d]\n", __FUNCTION__,res);
				}
		//Hugh 2012-09-10 CT2 set Concr_mode ++++

		/* 2012-09-21 Stop roam when start Concurrent ++++ */
		roam_off = 1;
		if((res = wldev_iovar_setint(dev, "roam_off", roam_off)))
				printf("%s fail to set roam_off res[%d]\n", __FUNCTION__,res);
		/* 2012-09-21 Stop roam when start Concurrent ---- */
		
   		mpc = 0;
      	        if ((res = wldev_iovar_setint(dev, "mpc", mpc))) {
           	        printf("%s fail to set mpc\n", __FUNCTION__);
           	        goto fail;
      	        }	

         	if ((res = wl_iw_set_ap_security(ap_net_dev, &ap_cfg)) != 0) {
           	        printf(" %s ERROR setting SOFTAP security in :%d\n", __FUNCTION__, res);
         	        goto fail;
                } 
                
                bss_setbuf.cfg = 1;
                bss_setbuf.val = 1;  /* up the interface */

                if ((res = wldev_iovar_setbuf_bsscfg(dev, "bss", &bss_setbuf, sizeof(bss_setbuf), smbuf, sizeof(smbuf), 1, NULL)) < 0){
           	         printf("%s: ERROR:%d, set bss up failed\n", __FUNCTION__, res);
           	         goto fail;
        	}

	        bcm_mdelay(500);

//HTC_WIFI_START [Hugh 2012-08-23 APSTA_CT3 tout ++++]
            printf("prepare set frameburst \n");
            frameburst = 1;
            if ((res = wldev_ioctl(dev, WLC_SET_FAKEFRAG, &frameburst, sizeof(frameburst), 0))) {
                printf("%s fail to set frameburst !!\n", __FUNCTION__);
            }
//HTC_WIFI_END [Hugh 2012-08-23 APSTA_CT3 tout ----]

		if ((res = wldev_iovar_setint(dev, "allmulti", 1))) {
            		printf("%s: ERROR:%d, set allmulti failed\n", __FUNCTION__, res);
            		goto fail;
		}
//BRCM_APSTA_START
		set_ap_channel(dev,&ap_cfg);
		ap_net_dev->operstate = IF_OPER_UP;
//BRCM_APSTA_END
	} else {
		if ((res = wl_softap_stop(ap_net_dev))){
           		printf("%s: ERROR:%d, call wl_softap_stop failed\n", __FUNCTION__, res);
           		goto fail;
		}

	//Hugh 2012-09-10 CT2 set Concr_mode ++++
		concr_mode = 0;
		if ((res = wldev_iovar_setint(dev, "concr_mode_set", concr_mode))) {
				printf("%s fail to set concr_mode res 2[%d]\n", __FUNCTION__,res);
			}
	//Hugh 2012-09-10 CT2 set Concr_mode ++++

	/* 2012-09-21 Stop roam when start Concurrent ++++ */
	roam_off = 0;
	if((res = wldev_iovar_setint(dev, "roam_off", roam_off)))
			printf("%s fail to set roam_off res[%d]\n", __FUNCTION__,res);
	/* 2012-09-21 Stop roam when start Concurrent ---- */

	mpc = 1;
	     	if ((res = wldev_iovar_setint(dev, "mpc", mpc))) {
        	   	printf("%s fail to set mpc\n", __FUNCTION__);
           		goto fail;
	      	}

//HTC_WIFI_START [Hugh 2012-08-23 APSTA_CT3 tout ++++]
        printf("prepare set frameburst \n");
        frameburst = 0;
        if ((res = wldev_ioctl(dev, WLC_SET_FAKEFRAG, &frameburst, sizeof(frameburst), 0))) {
            printf("%s fail to set frameburst !!\n", __FUNCTION__);
        }
//HTC_WIFI_END [Hugh 2012-08-23 APSTA_CT3 tout ----]
	}

fail:
    return res;
}
#ifdef BRCM_WPSAP
int wldev_set_ap_sta_registra_wsec(struct net_device *dev, char *command, int total_len)
{
	int bytes_written = 0;
	int wsec = 0;

        if ( !ap_net_dev ) return 0;

	wldev_iovar_getint(ap_net_dev, "wsec", &wsec);
	WLDEV_ERROR(("### %s devname[%s],got wsec(bset)=0x%x\n", __FUNCTION__, ap_net_dev->name, wsec));

	wsec |= SES_OW_ENABLED;
	WLDEV_ERROR(("### %s devname[%s],wsec=0x%x\n", __FUNCTION__, ap_net_dev->name, wsec));

	wldev_iovar_setint(ap_net_dev, "wsec", wsec);
	WLDEV_ERROR(("### %s devname[%s] seting\n", __FUNCTION__, ap_net_dev->name));

	wldev_iovar_getint(ap_net_dev, "wsec", &wsec);
	WLDEV_ERROR(("### %s devname[%s],got(aset) wsec=0x%x\n", __FUNCTION__, ap_net_dev->name, wsec));

	return bytes_written;
}
#endif /* BRCM_WPSAP */


//Hugh 2012-08-30 CT2 modification ++++
void wldev_san_check_channel(struct net_device *ndev,int *errcode)
{   
    int error = 0;
    channel_info_t ci;
    int consta_channel = 0;
    int conap_channel = 0;

    printf("Enter %s\n",__FUNCTION__);
        
    if (!ndev ){
        printf("%s NULL ConSTA netdev[%p]",__FUNCTION__,ndev);
        *errcode = 1;
            
    }   
            
    if (!ap_net_dev ){
        printf("%s NULL ConAP netdev[%p]",__FUNCTION__,ap_net_dev);
        *errcode = 2;
    }           

    if ((error = wldev_ioctl(ndev, WLC_GET_CHANNEL, &ci, sizeof(ci), 0))) {
        WLDEV_ERROR(("%s: get ConSTA channel fail!\n", __FUNCTION__));
        *errcode = 3;
    }
    else{
        printf("%s Got ConSTA TargetChannel[%d]\n",__FUNCTION__,ci.target_channel);
        consta_channel = ci.target_channel;
    }
    if ((error = wldev_ioctl(ap_net_dev, WLC_GET_CHANNEL, &ci, sizeof(ci), 0))) {
        WLDEV_ERROR(("%s: get ConAP channel fail!\n", __FUNCTION__));
        *errcode = 4;
    }
    else{
        printf("%s Got ConAP TargetChannel[%d]\n",__FUNCTION__,ci.target_channel);
        conap_channel = ci.target_channel;

    }

    if(consta_channel == conap_channel){
        printf("ConSTA,ConAP Channel same \n");
        *errcode = 0;
    }
    else{
        printf("ConSTA,ConAP Channel NOT same!!! \n");
        *errcode = consta_channel;
    }

    printf("Leave %s errcode[%d]",__FUNCTION__,*errcode);
}
//Hugh 2012-08-30 CT2 modification ---


#endif /* APSTA_CONCURRENT */
//BRCM APSTA END
