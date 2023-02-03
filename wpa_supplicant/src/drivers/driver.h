/*
 * Driver interface definition
 * Copyright (c) 2003-2017, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 *
 * This file defines a driver interface used by both %wpa_supplicant and
 * hostapd. The first part of the file defines data structures used in various
 * driver operations. This is followed by the struct wpa_driver_ops that each
 * driver wrapper will beed to define with callback functions for requesting
 * driver operations. After this, there are definitions for driver event
 * reporting with wpa_supplicant_event() and some convenience helper functions
 * that can be used to report events.
 */

#ifndef DRIVER_H
#define DRIVER_H

#define WPA_SUPPLICANT_DRIVER_VERSION 4

#include "common/defs.h"
#include "common/ieee802_11_defs.h"
#include "common/wpa_common.h"
#include "utils/list.h"

/**
 * enum reg_change_initiator - Regulatory change initiator
 */
enum reg_change_initiator {
	REGDOM_SET_BY_CORE,
	REGDOM_SET_BY_USER,
	REGDOM_SET_BY_DRIVER,
	REGDOM_SET_BY_COUNTRY_IE,
	REGDOM_BEACON_HINT,
};

/**
 * enum reg_type - Regulatory change types
 */
enum reg_type {
	REGDOM_TYPE_UNKNOWN,
	REGDOM_TYPE_COUNTRY,
	REGDOM_TYPE_WORLD,
	REGDOM_TYPE_CUSTOM_WORLD,
	REGDOM_TYPE_INTERSECTION,
};

struct wpa_bss;
struct wpa_supplicant;
/**
 * struct wpa_scan_res - Scan result for an BSS/IBSS
 * @flags: information flags about the BSS/IBSS (WPA_SCAN_*)
 * @bssid: BSSID
 * @freq: frequency of the channel in MHz (e.g., 2412 = channel 1)
 * @beacon_int: beacon interval in TUs (host byte order)
 * @caps: capability information field in host byte order
 * @qual: signal quality
 * @noise: noise level
 * @level: signal level
 * @tsf: Timestamp
 * @age: Age of the information in milliseconds (i.e., how many milliseconds
 * ago the last Beacon or Probe Response frame was received)
 * @snr: Signal-to-noise ratio in dB (calculated during scan result processing)
 * @parent_tsf: Time when the Beacon/Probe Response frame was received in terms
 * of TSF of the BSS specified by %tsf_bssid.
 * @tsf_bssid: The BSS that %parent_tsf TSF time refers to.
 * @ie_len: length of the following IE field in octets
 * @beacon_ie_len: length of the following Beacon IE field in octets
 *
 * This structure is used as a generic format for scan results from the
 * driver. Each driver interface implementation is responsible for converting
 * the driver or OS specific scan results into this format.
 *
 * If the driver does not support reporting all IEs, the IE data structure is
 * constructed of the IEs that are available. This field will also need to
 * include SSID in IE format. All drivers are encouraged to be extended to
 * report all IEs to make it easier to support future additions.
 *
 * This structure data is followed by ie_len octets of IEs from Probe Response
 * frame (or if the driver does not indicate source of IEs, these may also be
 * from Beacon frame). After the first set of IEs, another set of IEs may follow
 * (with beacon_ie_len octets of data) if the driver provides both IE sets.
 */
struct wpa_scan_res {
	unsigned int flags;
	u8 bssid[ETH_ALEN];
	int chan;
	u16 beacon_int;
	u16 caps;
	int noise;
	int level;
	u64 tsf;
	unsigned int age;
	u64 parent_tsf;
	u8 tsf_bssid[ETH_ALEN];
	size_t ie_len;
	size_t beacon_ie_len;
	/* Followed by ie_len + beacon_ie_len octets of IE data */
};

/**
 * struct wpa_scan_results - Scan results
 * @res: Array of pointers to allocated variable length scan result entries
 * @num: Number of entries in the scan result array
 * @fetch_time: Time when the results were fetched from the driver
 */
struct wpa_scan_results {
	struct wpa_scan_res **res;
	size_t num;
	struct os_reltime fetch_time;
};

#define WPAS_MAX_SCAN_SSIDS 1

/**
 * struct wpa_driver_scan_ssid - SSIDs to scan for
 * @ssid - specific SSID to scan for (ProbeReq)
 *	%NULL or zero-length SSID is used to indicate active scan
 *	with wildcard SSID.
 * @ssid_len - Length of the SSID in octets
 */
struct wpa_driver_scan_ssid {
	const u8 *ssid;
	size_t ssid_len;
};

/**
 * struct wpa_driver_scan_params - Scan parameters
 * Data for struct wpa_driver_ops::scan2().
 */
struct wpa_driver_scan_params {
	/**
	 * ssids - SSIDs to scan for
	 */
	struct wpa_driver_scan_ssid ssids[WPAS_MAX_SCAN_SSIDS];

	/**
	 * num_ssids - Number of entries in ssids array
	 * Zero indicates a request for a passive scan.
	 */
	size_t num_ssids;

	/**
	 * freqs - Array of frequencies to scan or %NULL for all frequencies
	 *
	 * The frequency is set in MHz. The array is zero-terminated.
	 */
	int channel;

	/**
	 * bssid - Specific BSSID to scan for
	 *
	 * This optional parameter can be used to replace the default wildcard
	 * BSSID with a specific BSSID to scan for if results are needed from
	 * only a single BSS.
	 */
	const u8 *bssid;

	 /**
	  * duration - Dwell time on each channel
	  *
	  * This optional parameter can be used to set the dwell time on each
	  * channel. In TUs.
	  */
	 u16 duration;

	 unsigned int duration_mandatory;
	 u8 mode;
};

struct wpa_bss_trans_info {
	u8 mbo_transition_reason;
	u8 n_candidates;
	u8 *bssid;
};

struct wpa_bss_candidate_info {
	u8 num;
	struct candidate_list {
		u8 bssid[ETH_ALEN];
		u8 is_accept;
		u32 reject_reason;
	} *candidates;
};

/**
 * struct wpa_driver_ops - Driver interface API definition
 *
 * This structure defines the API that each driver interface needs to implement
 * for core wpa_supplicant code. All driver specific functionality is captured
 * in this wrapper.
 */

/**
 * struct external_auth - External authentication trigger parameters
 *
 * These are used across the external authentication request and event
 * interfaces.
 * @action: Action type / trigger for external authentication. Only significant
 *	for the event interface.
 * @bssid: BSSID of the peer with which the authentication has to happen. Used
 *	by both the request and event interface.
 * @ssid: SSID of the AP. Used by both the request and event interface.
 * @ssid_len: SSID length in octets.
 * @key_mgmt_suite: AKM suite of the respective authentication. Optional for
 *	the request interface.
 * @status: Status code, %WLAN_STATUS_SUCCESS for successful authentication,
 *	use %WLAN_STATUS_UNSPECIFIED_FAILURE if wpa_supplicant cannot give
 *	the real status code for failures. Used only for the request interface
 *	from user space to the driver.
 * @pmkid: Generated PMKID as part of external auth exchange (e.g., SAE).
 */
struct external_auth {
	enum {
		EXT_AUTH_START,
		EXT_AUTH_ABORT,
	} action;
	const u8 *bssid;
	const u8 *ssid;
	size_t ssid_len;
	unsigned int key_mgmt_suite;
	u16 status;
	const u8 *pmkid;
};

struct wpa_driver_ops {
	/** Name of the driver interface */
	const char *name;
	/** One line description of the driver interface */
	const char *desc;

	/**
	 * get_bssid - Get the current BSSID
	 * @priv: private driver interface data
	 * @bssid: buffer for BSSID (ETH_ALEN = 6 bytes)
	 *
	 * Returns: 0 on success, -1 on failure
	 *
	 * Query kernel driver for the current BSSID and copy it to bssid.
	 * Setting bssid to 00:00:00:00:00:00 is recommended if the STA is not
	 * associated.
	 */
	int (*get_bssid)(void *priv, u8 *bssid);

	/**
	 * get_ssid - Get the current SSID
	 * @priv: private driver interface data
	 * @ssid: buffer for SSID (at least 32 bytes)
	 *
	 * Returns: Length of the SSID on success, -1 on failure
	 *
	 * Query kernel driver for the current SSID and copy it to ssid.
	 * Returning zero is recommended if the STA is not associated.
	 *
	 * Note: SSID is an array of octets, i.e., it is not nul terminated and
	 * can, at least in theory, contain control characters (including nul)
	 * and as such, should be processed as binary data, not a printable
	 * string.
	 */
	int (*get_ssid)(void *priv, u8 *ssid);

	/**
	 * set_key - Configure encryption key
	 * @ifname: Interface name (for multi-SSID/VLAN support)
	 * @priv: private driver interface data
	 * @alg: encryption algorithm (%WPA_ALG_NONE, %WPA_ALG_WEP,
	 *	%WPA_ALG_TKIP, %WPA_ALG_CCMP, %WPA_ALG_IGTK, %WPA_ALG_PMK,
	 *	%WPA_ALG_GCMP, %WPA_ALG_GCMP_256, %WPA_ALG_CCMP_256,
	 *	%WPA_ALG_BIP_GMAC_128, %WPA_ALG_BIP_GMAC_256,
	 *	%WPA_ALG_BIP_CMAC_256);
	 *	%WPA_ALG_NONE clears the key.
	 * @addr: Address of the peer STA (BSSID of the current AP when setting
	 *	pairwise key in station mode), ff:ff:ff:ff:ff:ff for
	 *	broadcast keys, %NULL for default keys that are used both for
	 *	broadcast and unicast; when clearing keys, %NULL is used to
	 *	indicate that both the broadcast-only and default key of the
	 *	specified key index is to be cleared
	 * @key_idx: key index (0..3), usually 0 for unicast keys; 0..4095 for
	 *	IGTK
	 * @set_tx: configure this key as the default Tx key (only used when
	 *	driver does not support separate unicast/individual key
	 * @seq: sequence number/packet number, seq_len octets, the next
	 *	packet number to be used for in replay protection; configured
	 *	for Rx keys (in most cases, this is only used with broadcast
	 *	keys and set to zero for unicast keys); %NULL if not set
	 * @seq_len: length of the seq, depends on the algorithm:
	 *	TKIP: 6 octets, CCMP/GCMP: 6 octets, IGTK: 6 octets
	 * @key: key buffer; TKIP: 16-byte temporal key, 8-byte Tx Mic key,
	 *	8-byte Rx Mic Key
	 * @key_len: length of the key buffer in octets (WEP: 5 or 13,
	 *	TKIP: 32, CCMP/GCMP: 16, IGTK: 16)
	 *
	 * Returns: 0 on success, -1 on failure
	 *
	 * Configure the given key for the kernel driver. If the driver
	 * supports separate individual keys (4 default keys + 1 individual),
	 * addr can be used to determine whether the key is default or
	 * individual. If only 4 keys are supported, the default key with key
	 * index 0 is used as the individual key. STA must be configured to use
	 * it as the default Tx key (set_tx is set) and accept Rx for all the
	 * key indexes. In most cases, WPA uses only key indexes 1 and 2 for
	 * broadcast keys, so key index 0 is available for this kind of
	 * configuration.
	 *
	 * Please note that TKIP keys include separate TX and RX MIC keys and
	 * some drivers may expect them in different order than wpa_supplicant
	 * is using. If the TX/RX keys are swapped, all TKIP encrypted packets
	 * will trigger Michael MIC errors. This can be fixed by changing the
	 * order of MIC keys by swapping te bytes 16..23 and 24..31 of the key
	 * in driver_*.c set_key() implementation, see driver_ndis.c for an
	 * example on how this can be done.
	 */
	int (*set_key)(const char *ifname, void *priv, enum wpa_alg alg,
		       const u8 *addr, int key_idx, int set_tx,
		       const u8 *seq, size_t seq_len,
		       const u8 *key, size_t key_len);

	/**
	 * init - Initialize driver interface
	 * @ctx: context to be used when calling wpa_supplicant functions,
	 * e.g., wpa_supplicant_event()
	 * @ifname: interface name, e.g., wlan0
	 *
	 * Returns: Pointer to private data, %NULL on failure
	 *
	 * Initialize driver interface, including event processing for kernel
	 * driver events (e.g., associated, scan results, Michael MIC failure).
	 * This function can allocate a private configuration data area for
	 * @ctx, file descriptor, interface name, etc. information that may be
	 * needed in future driver operations. If this is not used, non-NULL
	 * value will need to be returned because %NULL is used to indicate
	 * failure. The returned value will be used as 'void *priv' data for
	 * all other driver_ops functions.
	 *
	 * The main event loop (eloop.c) of wpa_supplicant can be used to
	 * register callback for read sockets (eloop_register_read_sock()).
	 *
	 * See below for more information about events and
	 * wpa_supplicant_event() function.
	 */
	void * (*init)(void *ctx, const char *ifname);

	/**
	 * deinit - Deinitialize driver interface
	 * @priv: private driver interface data from init()
	 *
	 * Shut down driver interface and processing of driver events. Free
	 * private data buffer if one was allocated in init() handler.
	 */
	void (*deinit)(void *priv);

	/**
	 * set_param - Set driver configuration parameters
	 * @priv: private driver interface data from init()
	 * @param: driver specific configuration parameters
	 *
	 * Returns: 0 on success, -1 on failure
	 *
	 * Optional handler for notifying driver interface about configuration
	 * parameters (driver_param).
	 */
	int (*set_param)(void *priv, const char *param);

	/**
	 * set_countermeasures - Enable/disable TKIP countermeasures
	 * @priv: private driver interface data
	 * @enabled: 1 = countermeasures enabled, 0 = disabled
	 *
	 * Returns: 0 on success, -1 on failure
	 *
	 * Configure TKIP countermeasures. When these are enabled, the driver
	 * should drop all received and queued frames that are using TKIP.
	 */
	int (*set_countermeasures)(void *priv, int enabled);

	/**
	 * deauthenticate - Request driver to deauthenticate
	 * @priv: private driver interface data
	 * @addr: peer address (BSSID of the AP)
	 * @reason_code: 16-bit reason code to be sent in the deauthentication
	 *	frame
	 *
	 * Returns: 0 on success, -1 on failure
	 */
	int (*deauthenticate)(void *priv, const u8 *addr, int reason_code);

	/**
	 * flush_pmkid - Flush PMKSA cache
	 * @priv: private driver interface data
	 *
	 * Returns: 0 on success, -1 on failure
	 *
	 * This function is called when the supplicant drops all PMKSA cache
	 * entries for any reason.
	 *
	 * If the driver generates RSN IE, i.e., it does not use wpa_ie in
	 * associate(), remove_pmkid() can be used to synchronize PMKSA caches
	 * between the driver and wpa_supplicant. If the driver uses wpa_ie
	 * from wpa_supplicant, this driver_ops function does not need to be
	 * implemented. Likewise, if the driver does not support WPA, this
	 * function is not needed.
	 */
	int (*flush_pmkid)(void *priv);

	/**
	 * poll - Poll driver for association information
	 * @priv: private driver interface data
	 *
	 * This is an option callback that can be used when the driver does not
	 * provide event mechanism for association events. This is called when
	 * receiving WPA EAPOL-Key messages that require association
	 * information. The driver interface is supposed to generate associnfo
	 * event before returning from this callback function. In addition, the
	 * driver interface should generate an association event after having
	 * sent out associnfo.
	 */
	void (*poll)(void *priv);

	/**
	 * get_ifindex - Get interface index
	 * @priv: private driver interface data
	 *
	 * Returns: Interface index
	 */
	unsigned int (*get_ifindex)(void *priv);

	/**
	 * get_ifname - Get interface name
	 * @priv: private driver interface data
	 *
	 * Returns: Pointer to the interface name. This can differ from the
	 * interface name used in init() call. Init() is called first.
	 *
	 * This optional function can be used to allow the driver interface to
	 * replace the interface name with something else, e.g., based on an
	 * interface mapping from a more descriptive name.
	 */
	const char * (*get_ifname)(void *priv);

	/**
	 * get_mac_addr - Get own MAC address
	 * @priv: private driver interface data
	 *
	 * Returns: Pointer to own MAC address or %NULL on failure
	 *
	 * This optional function can be used to get the own MAC address of the
	 * device from the driver interface code. This is only needed if the
	 * l2_packet implementation for the OS does not provide easy access to
	 * a MAC address. */
	const u8 * (*get_mac_addr)(void *priv);

	/**
	 * set_operstate - Sets device operating state to DORMANT or UP
	 * @priv: private driver interface data
	 * @state: 0 = dormant, 1 = up
	 * Returns: 0 on success, -1 on failure
	 *
	 * This is an optional function that can be used on operating systems
	 * that support a concept of controlling network device state from user
	 * space applications. This function, if set, gets called with
	 * state = 1 when authentication has been completed and with state = 0
	 * when connection is lost.
	 */
	int (*set_operstate)(void *priv, int state);

	/**
	 * mlme_setprotection - MLME-SETPROTECTION.request primitive
	 * @priv: Private driver interface data
	 * @addr: Address of the station for which to set protection (may be
	 * %NULL for group keys)
	 * @protect_type: MLME_SETPROTECTION_PROTECT_TYPE_*
	 * @key_type: MLME_SETPROTECTION_KEY_TYPE_*
	 * Returns: 0 on success, -1 on failure
	 *
	 * This is an optional function that can be used to set the driver to
	 * require protection for Tx and/or Rx frames. This uses the layer
	 * interface defined in IEEE 802.11i-2004 clause 10.3.22.1
	 * (MLME-SETPROTECTION.request). Many drivers do not use explicit
	 * set protection operation; instead, they set protection implicitly
	 * based on configured keys.
	 */
	int (*mlme_setprotection)(void *priv, const u8 *addr, int protect_type,
				  int key_type);

	/**
	 * get_hw_feature_data - Get hardware support data (channels and rates)
	 * @priv: Private driver interface data
	 * @num_modes: Variable for returning the number of returned modes
	 * flags: Variable for returning hardware feature flags
	 * @dfs: Variable for returning DFS region (HOSTAPD_DFS_REGION_*)
	 * Returns: Pointer to allocated hardware data on success or %NULL on
	 * failure. Caller is responsible for freeing this.
	 */
	struct hostapd_hw_modes * (*get_hw_feature_data)(void *priv,
							 u16 *num_modes,
							 u16 *flags, u8 *dfs);

	/**
	 * send_mlme - Send management frame from MLME
	 * @priv: Private driver interface data
	 * @data: IEEE 802.11 management frame with IEEE 802.11 header
	 * @data_len: Size of the management frame
	 * @noack: Do not wait for this frame to be acked (disable retries)
	 * @freq: Frequency (in MHz) to send the frame on, or 0 to let the
	 * driver decide
	 * @csa_offs: Array of CSA offsets or %NULL
	 * @csa_offs_len: Number of elements in csa_offs
	 * Returns: 0 on success, -1 on failure
	 */
	int (*send_mlme)(void *priv, const u8 *data, size_t data_len,
			 int noack, unsigned int freq, const u16 *csa_offs,
			 size_t csa_offs_len);

	/**
	 * update_ft_ies - Update FT (IEEE 802.11r) IEs
	 * @priv: Private driver interface data
	 * @md: Mobility domain (2 octets) (also included inside ies)
	 * @ies: FT IEs (MDIE, FTIE, ...) or %NULL to remove IEs
	 * @ies_len: Length of FT IEs in bytes
	 * Returns: 0 on success, -1 on failure
	 *
	 * The supplicant uses this callback to let the driver know that keying
	 * material for FT is available and that the driver can use the
	 * provided IEs in the next message in FT authentication sequence.
	 *
	 * This function is only needed for driver that support IEEE 802.11r
	 * (Fast BSS Transition).
	 */
	int (*update_ft_ies)(void *priv, const u8 *md, const u8 *ies,
			     size_t ies_len);

	/**
	 * get_scan_results2 - Fetch the latest scan results
	 * @priv: private driver interface data
	 *
	 * Returns: Allocated buffer of scan results (caller is responsible for
	 * freeing the data structure) on success, NULL on failure
	 */
	 struct wpa_scan_results * (*get_scan_results2)(void *priv);

	/**
	 * set_country - Set country
	 * @priv: Private driver interface data
	 * @alpha2: country to which to switch to
	 * Returns: 0 on success, -1 on failure
	 *
	 * This function is for drivers which support some form
	 * of setting a regulatory domain.
	 */
	int (*set_country)(void *priv, const char *alpha2);

	/**
	 * get_country - Get country
	 * @priv: Private driver interface data
	 * @alpha2: Buffer for returning country code (at least 3 octets)
	 * Returns: 0 on success, -1 on failure
	 */
	int (*get_country)(void *priv, char *alpha2);

	/**
	 * global_init - Global driver initialization
	 * @ctx: wpa_global pointer
	 * Returns: Pointer to private data (global), %NULL on failure
	 *
	 * This optional function is called to initialize the driver wrapper
	 * for global data, i.e., data that applies to all interfaces. If this
	 * function is implemented, global_deinit() will also need to be
	 * implemented to free the private data. The driver will also likely
	 * use init2() function instead of init() to get the pointer to global
	 * data available to per-interface initializer.
	 */
	void * (*global_init)(void *ctx);

	/**
	 * global_deinit - Global driver deinitialization
	 * @priv: private driver global data from global_init()
	 *
	 * Terminate any global driver related functionality and free the
	 * global data structure.
	 */
	void (*global_deinit)(void *priv);

	/**
	 * init2 - Initialize driver interface (with global data)
	 * @ctx: context to be used when calling wpa_supplicant functions,
	 * e.g., wpa_supplicant_event()
	 * @ifname: interface name, e.g., wlan0
	 * @global_priv: private driver global data from global_init()
	 * Returns: Pointer to private data, %NULL on failure
	 *
	 * This function can be used instead of init() if the driver wrapper
	 * uses global data.
	 */
	void * (*init2)(void *ctx, const char *ifname, void *global_priv);

	/**
	 * get_interfaces - Get information about available interfaces
	 * @global_priv: private driver global data from global_init()
	 * Returns: Allocated buffer of interface information (caller is
	 * responsible for freeing the data structure) on success, NULL on
	 * failure
	 */
	struct wpa_interface_info * (*get_interfaces)(void *global_priv);

	/**
	 * scan2 - Request the driver to initiate scan
	 * @priv: private driver interface data
	 * @params: Scan parameters
	 *
	 * Returns: 0 on success, -1 on failure
	 *
	 * Once the scan results are ready, the driver should report scan
	 * results event for wpa_supplicant which will eventually request the
	 * results with wpa_driver_get_scan_results2().
	 */
	int (*scan2)(void *priv, struct wpa_driver_scan_params *params);

	/**
	 * set_privacy - Enable/disable privacy (AP only)
	 * @priv: Private driver interface data
	 * @enabled: 1 = privacy enabled, 0 = disabled
	 * Returns: 0 on success, -1 on failure
	 *
	 * This is an optional function to configure privacy field in the
	 * kernel driver for Beacon frames. This can be left undefined (set to
	 * %NULL) if the driver uses the Beacon template from set_ap().
	 *
	 * DEPRECATED - use set_ap() instead
	 */
	int (*set_privacy)(void *priv, int enabled);

	/**
	 * get_seqnum - Fetch the current TSC/packet number (AP only)
	 * @ifname: The interface name (main or virtual)
	 * @priv: Private driver interface data
	 * @addr: MAC address of the station or %NULL for group keys
	 * @idx: Key index
	 * @seq: Buffer for returning the latest used TSC/packet number
	 * Returns: 0 on success, -1 on failure
	 *
	 * This function is used to fetch the last used TSC/packet number for
	 * a TKIP, CCMP, GCMP, or BIP/IGTK key. It is mainly used with group
	 * keys, so there is no strict requirement on implementing support for
	 * unicast keys (i.e., addr != %NULL).
	 */
	int (*get_seqnum)(const char *ifname, void *priv, const u8 *addr,
			  int idx, u8 *seq);

	/**
	 * flush - Flush all association stations (AP only)
	 * @priv: Private driver interface data
	 * Returns: 0 on success, -1 on failure
	 *
	 * This function requests the driver to disassociate all associated
	 * stations. This function does not need to be implemented if the
	 * driver does not process association frames internally.
	 */
	int (*flush)(void *priv);

	/**
	 * set_generic_elem - Add IEs into Beacon/Probe Response frames (AP)
	 * @priv: Private driver interface data
	 * @elem: Information elements
	 * @elem_len: Length of the elem buffer in octets
	 * Returns: 0 on success, -1 on failure
	 *
	 * This is an optional function to add information elements in the
	 * kernel driver for Beacon and Probe Response frames. This can be left
	 * undefined (set to %NULL) if the driver uses the Beacon template from
	 * set_ap().
	 *
	 * DEPRECATED - use set_ap() instead
	 */
	int (*set_generic_elem)(void *priv, const u8 *elem, size_t elem_len);

	/**
	 * hapd_send_eapol - Send an EAPOL packet (AP only)
	 * @priv: private driver interface data
	 * @addr: Destination MAC address
	 * @data: EAPOL packet starting with IEEE 802.1X header
	 * @data_len: Length of the EAPOL packet in octets
	 * @encrypt: Whether the frame should be encrypted
	 * @own_addr: Source MAC address
	 * @flags: WPA_STA_* flags for the destination station
	 *
	 * Returns: 0 on success, -1 on failure
	 */
	int (*hapd_send_eapol)(void *priv, const u8 *addr, const u8 *data,
			       size_t data_len, int encrypt,
			       const u8 *own_addr, u32 flags);

	/**
	 * sta_deauth - Deauthenticate a station (AP only)
	 * @priv: Private driver interface data
	 * @own_addr: Source address and BSSID for the Deauthentication frame
	 * @addr: MAC address of the station to deauthenticate
	 * @reason: Reason code for the Deauthentiation frame
	 * Returns: 0 on success, -1 on failure
	 *
	 * This function requests a specific station to be deauthenticated and
	 * a Deauthentication frame to be sent to it.
	 */
	int (*sta_deauth)(void *priv, const u8 *own_addr, const u8 *addr,
			  int reason);

	/**
	 * sta_disassoc - Disassociate a station (AP only)
	 * @priv: Private driver interface data
	 * @own_addr: Source address and BSSID for the Disassociation frame
	 * @addr: MAC address of the station to disassociate
	 * @reason: Reason code for the Disassociation frame
	 * Returns: 0 on success, -1 on failure
	 *
	 * This function requests a specific station to be disassociated and
	 * a Disassociation frame to be sent to it.
	 */
	int (*sta_disassoc)(void *priv, const u8 *own_addr, const u8 *addr,
			    int reason);

	/**
	 * sta_remove - Remove a station entry (AP only)
	 * @priv: Private driver interface data
	 * @addr: MAC address of the station to be removed
	 * Returns: 0 on success, -1 on failure
	 */
	int (*sta_remove)(void *priv, const u8 *addr);

	/**
	 * hapd_get_ssid - Get the current SSID (AP only)
	 * @priv: Private driver interface data
	 * @buf: Buffer for returning the SSID
	 * @len: Maximum length of the buffer
	 * Returns: Length of the SSID on success, -1 on failure
	 *
	 * This function need not be implemented if the driver uses Beacon
	 * template from set_ap() and does not reply to Probe Request frames.
	 */
	int (*hapd_get_ssid)(void *priv, u8 *buf, int len);

	/**
	 * hapd_set_ssid - Set SSID (AP only)
	 * @priv: Private driver interface data
	 * @buf: SSID
	 * @len: Length of the SSID in octets
	 * Returns: 0 on success, -1 on failure
	 *
	 * DEPRECATED - use set_ap() instead
	 */
	int (*hapd_set_ssid)(void *priv, const u8 *buf, int len);

	/**
	 * hapd_set_countermeasures - Enable/disable TKIP countermeasures (AP)
	 * @priv: Private driver interface data
	 * @enabled: 1 = countermeasures enabled, 0 = disabled
	 * Returns: 0 on success, -1 on failure
	 *
	 * This need not be implemented if the driver does not take care of
	 * association processing.
	 */
	int (*hapd_set_countermeasures)(void *priv, int enabled);
	/**
	 * get_inact_sec - Get station inactivity duration (AP only)
	 * @priv: Private driver interface data
	 * @addr: Station address
	 * Returns: Number of seconds station has been inactive, -1 on failure
	 */
	int (*get_inact_sec)(void *priv, const u8 *addr);

	/**
	 * sta_clear_stats - Clear station statistics (AP only)
	 * @priv: Private driver interface data
	 * @addr: Station address
	 * Returns: 0 on success, -1 on failure
	 */
	int (*sta_clear_stats)(void *priv, const u8 *addr);

	/**
	 * set_rts - Set RTS threshold
	 * @priv: Private driver interface data
	 * @rts: RTS threshold in octets
	 * Returns: 0 on success, -1 on failure
	 */
	int (*set_rts)(void *priv, int rts);

	/**
	 * set_frag - Set fragmentation threshold
	 * @priv: Private driver interface data
	 * @frag: Fragmentation threshold in octets
	 * Returns: 0 on success, -1 on failure
	 */
	int (*set_frag)(void *priv, int frag);

	/**
	 * sta_set_flags - Set station flags (AP only)
	 * @priv: Private driver interface data
	 * @addr: Station address
	 * @total_flags: Bitmap of all WPA_STA_* flags currently set
	 * @flags_or: Bitmap of WPA_STA_* flags to add
	 * @flags_and: Bitmap of WPA_STA_* flags to us as a mask
	 * Returns: 0 on success, -1 on failure
	 */
	int (*sta_set_flags)(void *priv, const u8 *addr,
			     unsigned int total_flags, unsigned int flags_or,
			     unsigned int flags_and);

	/**
	 * set_tx_queue_params - Set TX queue parameters
	 * @priv: Private driver interface data
	 * @queue: Queue number (0 = VO, 1 = VI, 2 = BE, 3 = BK)
	 * @aifs: AIFS
	 * @cw_min: cwMin
	 * @cw_max: cwMax
	 * @burst_time: Maximum length for bursting in 0.1 msec units
	 */
	int (*set_tx_queue_params)(void *priv, int queue, int aifs, int cw_min,
				   int cw_max, int burst_time);

	/**
	 * set_sta_vlan - Bind a station into a specific interface (AP only)
	 * @priv: Private driver interface data
	 * @ifname: Interface (main or virtual BSS or VLAN)
	 * @addr: MAC address of the associated station
	 * @vlan_id: VLAN ID
	 * Returns: 0 on success, -1 on failure
	 *
	 * This function is used to bind a station to a specific virtual
	 * interface. It is only used if when virtual interfaces are supported,
	 * e.g., to assign stations to different VLAN interfaces based on
	 * information from a RADIUS server. This allows separate broadcast
	 * domains to be used with a single BSS.
	 */
	int (*set_sta_vlan)(void *priv, const u8 *addr, const char *ifname,
			    int vlan_id);

	/**
	 * commit - Optional commit changes handler (AP only)
	 * @priv: driver private data
	 * Returns: 0 on success, -1 on failure
	 *
	 * This optional handler function can be registered if the driver
	 * interface implementation needs to commit changes (e.g., by setting
	 * network interface up) at the end of initial configuration. If set,
	 * this handler will be called after initial setup has been completed.
	 */
	int (*commit)(void *priv);

	/**
	 * send_ether - Send an ethernet packet (AP only)
	 * @priv: private driver interface data
	 * @dst: Destination MAC address
	 * @src: Source MAC address
	 * @proto: Ethertype
	 * @data: EAPOL packet starting with IEEE 802.1X header
	 * @data_len: Length of the EAPOL packet in octets
	 * Returns: 0 on success, -1 on failure
	 */
	int (*send_ether)(void *priv, const u8 *dst, const u8 *src, u16 proto,
			  const u8 *data, size_t data_len);

	/**
	 * set_radius_acl_auth - Notification of RADIUS ACL change
	 * @priv: Private driver interface data
	 * @mac: MAC address of the station
	 * @accepted: Whether the station was accepted
	 * @session_timeout: Session timeout for the station
	 * Returns: 0 on success, -1 on failure
	 */
	int (*set_radius_acl_auth)(void *priv, const u8 *mac, int accepted,
				   u32 session_timeout);

	/**
	 * set_radius_acl_expire - Notification of RADIUS ACL expiration
	 * @priv: Private driver interface data
	 * @mac: MAC address of the station
	 * Returns: 0 on success, -1 on failure
	 */
	int (*set_radius_acl_expire)(void *priv, const u8 *mac);

	/**
	 * set_ap_wps_ie - Add WPS IE(s) into Beacon/Probe Response frames (AP)
	 * @priv: Private driver interface data
	 * @beacon: WPS IE(s) for Beacon frames or %NULL to remove extra IE(s)
	 * @proberesp: WPS IE(s) for Probe Response frames or %NULL to remove
	 *	extra IE(s)
	 * @assocresp: WPS IE(s) for (Re)Association Response frames or %NULL
	 *	to remove extra IE(s)
	 * Returns: 0 on success, -1 on failure
	 *
	 * This is an optional function to add WPS IE in the kernel driver for
	 * Beacon and Probe Response frames. This can be left undefined (set
	 * to %NULL) if the driver uses the Beacon template from set_ap()
	 * and does not process Probe Request frames. If the driver takes care
	 * of (Re)Association frame processing, the assocresp buffer includes
	 * WPS IE(s) that need to be added to (Re)Association Response frames
	 * whenever a (Re)Association Request frame indicated use of WPS.
	 *
	 * This will also be used to add P2P IE(s) into Beacon/Probe Response
	 * frames when operating as a GO. The driver is responsible for adding
	 * timing related attributes (e.g., NoA) in addition to the IEs
	 * included here by appending them after these buffers. This call is
	 * also used to provide Probe Response IEs for P2P Listen state
	 * operations for drivers that generate the Probe Response frames
	 * internally.
	 *
	 * DEPRECATED - use set_ap() instead
	 */
	int (*set_ap_wps_ie)(void *priv, const struct wpabuf *beacon,
			     const struct wpabuf *proberesp,
			     const struct wpabuf *assocresp);

	/**
	 * set_supp_port - Set IEEE 802.1X Supplicant Port status
	 * @priv: Private driver interface data
	 * @authorized: Whether the port is authorized
	 * Returns: 0 on success, -1 on failure
	 */
	int (*set_supp_port)(void *priv, int authorized);

	/**
	 * set_wds_sta - Bind a station into a 4-address WDS (AP only)
	 * @priv: Private driver interface data
	 * @addr: MAC address of the associated station
	 * @aid: Association ID
	 * @val: 1 = bind to 4-address WDS; 0 = unbind
	 * @bridge_ifname: Bridge interface to use for the WDS station or %NULL
	 *	to indicate that bridge is not to be used
	 * @ifname_wds: Buffer to return the interface name for the new WDS
	 *	station or %NULL to indicate name is not returned.
	 * Returns: 0 on success, -1 on failure
	 */
	int (*set_wds_sta)(void *priv, const u8 *addr, int aid, int val,
			   const char *bridge_ifname, char *ifname_wds);

	/**
	 * send_action - Transmit an Action frame
	 * @priv: Private driver interface data
	 * @freq: Frequency (in MHz) of the channel
	 * @wait: Time to wait off-channel for a response (in ms), or zero
	 * @dst: Destination MAC address (Address 1)
	 * @src: Source MAC address (Address 2)
	 * @bssid: BSSID (Address 3)
	 * @data: Frame body
	 * @data_len: data length in octets
	 @ @no_cck: Whether CCK rates must not be used to transmit this frame
	 * Returns: 0 on success, -1 on failure
	 *
	 * This command can be used to request the driver to transmit an action
	 * frame to the specified destination.
	 *
	 * If the %WPA_DRIVER_FLAGS_OFFCHANNEL_TX flag is set, the frame will
	 * be transmitted on the given channel and the device will wait for a
	 * response on that channel for the given wait time.
	 *
	 * If the flag is not set, the wait time will be ignored. In this case,
	 * if a remain-on-channel duration is in progress, the frame must be
	 * transmitted on that channel; alternatively the frame may be sent on
	 * the current operational channel (if in associated state in station
	 * mode or while operating as an AP.)
	 *
	 * If @src differs from the device MAC address, use of a random
	 * transmitter address is requested for this message exchange.
	 */
	int (*send_action)(void *priv, unsigned int freq, unsigned int wait,
			   const u8 *dst, const u8 *src, const u8 *bssid,
			   const u8 *data, size_t data_len, int no_cck);

	/**
	 * send_action_cancel_wait - Cancel action frame TX wait
	 * @priv: Private driver interface data
	 *
	 * This command cancels the wait time associated with sending an action
	 * frame. It is only available when %WPA_DRIVER_FLAGS_OFFCHANNEL_TX is
	 * set in the driver flags.
	 */
	void (*send_action_cancel_wait)(void *priv);

	/**
	 * remain_on_channel - Remain awake on a channel
	 * @priv: Private driver interface data
	 * @freq: Frequency (in MHz) of the channel
	 * @duration: Duration in milliseconds
	 * Returns: 0 on success, -1 on failure
	 *
	 * This command is used to request the driver to remain awake on the
	 * specified channel for the specified duration and report received
	 * Action frames with EVENT_RX_MGMT events. Optionally, received
	 * Probe Request frames may also be requested to be reported by calling
	 * probe_req_report(). These will be reported with EVENT_RX_PROBE_REQ.
	 *
	 * The driver may not be at the requested channel when this function
	 * returns, i.e., the return code is only indicating whether the
	 * request was accepted. The caller will need to wait until the
	 * EVENT_REMAIN_ON_CHANNEL event indicates that the driver has
	 * completed the channel change. This may take some time due to other
	 * need for the radio and the caller should be prepared to timing out
	 * its wait since there are no guarantees on when this request can be
	 * executed.
	 */
	int (*remain_on_channel)(void *priv, unsigned int freq,
				 unsigned int duration);

	/**
	 * cancel_remain_on_channel - Cancel remain-on-channel operation
	 * @priv: Private driver interface data
	 *
	 * This command can be used to cancel a remain-on-channel operation
	 * before its originally requested duration has passed. This could be
	 * used, e.g., when remain_on_channel() is used to request extra time
	 * to receive a response to an Action frame and the response is
	 * received when there is still unneeded time remaining on the
	 * remain-on-channel operation.
	 */
	int (*cancel_remain_on_channel)(void *priv);

	/**
	 * probe_req_report - Request Probe Request frames to be indicated
	 * @priv: Private driver interface data
	 * @report: Whether to report received Probe Request frames
	 * Returns: 0 on success, -1 on failure (or if not supported)
	 *
	 * This command can be used to request the driver to indicate when
	 * Probe Request frames are received with EVENT_RX_PROBE_REQ events.
	 * Since this operation may require extra resources, e.g., due to less
	 * optimal hardware/firmware RX filtering, many drivers may disable
	 * Probe Request reporting at least in station mode. This command is
	 * used to notify the driver when the Probe Request frames need to be
	 * reported, e.g., during remain-on-channel operations.
	 */
	int (*probe_req_report)(void *priv, int report);

	/**
	 * deinit_ap - Deinitialize AP mode
	 * @priv: Private driver interface data
	 * Returns: 0 on success, -1 on failure (or if not supported)
	 *
	 * This optional function can be used to disable AP mode related
	 * configuration. If the interface was not dynamically added,
	 * change the driver mode to station mode to allow normal station
	 * operations like scanning to be completed.
	 */
	int (*deinit_ap)(void *priv);

	/**
	 * deinit_p2p_cli - Deinitialize P2P client mode
	 * @priv: Private driver interface data
	 * Returns: 0 on success, -1 on failure (or if not supported)
	 *
	 * This optional function can be used to disable P2P client mode. If the
	 * interface was not dynamically added, change the interface type back
	 * to station mode.
	 */
	int (*deinit_p2p_cli)(void *priv);

	/**
	 * suspend - Notification on system suspend/hibernate event
	 * @priv: Private driver interface data
	 */
	void (*suspend)(void *priv);

	/**
	 * resume - Notification on system resume/thaw event
	 * @priv: Private driver interface data
	 */
	void (*resume)(void *priv);

	/**
	 * signal_monitor - Set signal monitoring parameters
	 * @priv: Private driver interface data
	 * @threshold: Threshold value for signal change events; 0 = disabled
	 * @hysteresis: Minimum change in signal strength before indicating a
	 *	new event
	 * Returns: 0 on success, -1 on failure (or if not supported)
	 *
	 * This function can be used to configure monitoring of signal strength
	 * with the current AP. Whenever signal strength drops below the
	 * %threshold value or increases above it, EVENT_SIGNAL_CHANGE event
	 * should be generated assuming the signal strength has changed at
	 * least %hysteresis from the previously indicated signal change event.
	 */
	int (*signal_monitor)(void *priv, int threshold, int hysteresis);

	/**
	 * send_frame - Send IEEE 802.11 frame (testing use only)
	 * @priv: Private driver interface data
	 * @data: IEEE 802.11 frame with IEEE 802.11 header
	 * @data_len: Size of the frame
	 * @encrypt: Whether to encrypt the frame (if keys are set)
	 * Returns: 0 on success, -1 on failure
	 *
	 * This function is only used for debugging purposes and is not
	 * required to be implemented for normal operations.
	 */
	int (*send_frame)(void *priv, const u8 *data, size_t data_len,
			  int encrypt);

	/**
	 * get_noa - Get current Notice of Absence attribute payload
	 * @priv: Private driver interface data
	 * @buf: Buffer for returning NoA
	 * @buf_len: Buffer length in octets
	 * Returns: Number of octets used in buf, 0 to indicate no NoA is being
	 * advertized, or -1 on failure
	 *
	 * This function is used to fetch the current Notice of Absence
	 * attribute value from GO.
	 */
	int (*get_noa)(void *priv, u8 *buf, size_t buf_len);

	/**
	 * set_noa - Set Notice of Absence parameters for GO (testing)
	 * @priv: Private driver interface data
	 * @count: Count
	 * @start: Start time in ms from next TBTT
	 * @duration: Duration in ms
	 * Returns: 0 on success or -1 on failure
	 *
	 * This function is used to set Notice of Absence parameters for GO. It
	 * is used only for testing. To disable NoA, all parameters are set to
	 * 0.
	 */
	int (*set_noa)(void *priv, u8 count, int start, int duration);

	/**
	 * set_p2p_powersave - Set P2P power save options
	 * @priv: Private driver interface data
	 * @legacy_ps: 0 = disable, 1 = enable, 2 = maximum PS, -1 = no change
	 * @opp_ps: 0 = disable, 1 = enable, -1 = no change
	 * @ctwindow: 0.. = change (msec), -1 = no change
	 * Returns: 0 on success or -1 on failure
	 */
	int (*set_p2p_powersave)(void *priv, int legacy_ps, int opp_ps,
				 int ctwindow);

	/**
	 * ampdu - Enable/disable aggregation
	 * @priv: Private driver interface data
	 * @ampdu: 1/0 = enable/disable A-MPDU aggregation
	 * Returns: 0 on success or -1 on failure
	 */
	int (*ampdu)(void *priv, int ampdu);

	/**
	 * get_radio_name - Get physical radio name for the device
	 * @priv: Private driver interface data
	 * Returns: Radio name or %NULL if not known
	 *
	 * The returned data must not be modified by the caller. It is assumed
	 * that any interface that has the same radio name as another is
	 * sharing the same physical radio. This information can be used to
	 * share scan results etc. information between the virtual interfaces
	 * to speed up various operations.
	 */
	const char * (*get_radio_name)(void *priv);

	/**
	 * send_tdls_mgmt - for sending TDLS management packets
	 * @priv: private driver interface data
	 * @dst: Destination (peer) MAC address
	 * @action_code: TDLS action code for the mssage
	 * @dialog_token: Dialog Token to use in the message (if needed)
	 * @status_code: Status Code or Reason Code to use (if needed)
	 * @peer_capab: TDLS peer capability (TDLS_PEER_* bitfield)
	 * @initiator: Is the current end the TDLS link initiator
	 * @buf: TDLS IEs to add to the message
	 * @len: Length of buf in octets
	 * Returns: 0 on success, negative (<0) on failure
	 *
	 * This optional function can be used to send packet to driver which is
	 * responsible for receiving and sending all TDLS packets.
	 */
	int (*send_tdls_mgmt)(void *priv, const u8 *dst, u8 action_code,
			      u8 dialog_token, u16 status_code, u32 peer_capab,
			      int initiator, const u8 *buf, size_t len);

	/**
	 * set_qos_map - Set QoS Map
	 * @priv: Private driver interface data
	 * @qos_map_set: QoS Map
	 * @qos_map_set_len: Length of QoS Map
	 */
	int (*set_qos_map)(void *priv, const u8 *qos_map_set,
			   u8 qos_map_set_len);

	/**
	 * br_add_ip_neigh - Add a neigh to the bridge ip neigh table
	 * @priv: Private driver interface data
	 * @version: IP version of the IP address, 4 or 6
	 * @ipaddr: IP address for the neigh entry
	 * @prefixlen: IP address prefix length
	 * @addr: Corresponding MAC address
	 * Returns: 0 on success, negative (<0) on failure
	 */
	int (*br_add_ip_neigh)(void *priv, u8 version, const u8 *ipaddr,
			       int prefixlen, const u8 *addr);

	/**
	 * br_delete_ip_neigh - Remove a neigh from the bridge ip neigh table
	 * @priv: Private driver interface data
	 * @version: IP version of the IP address, 4 or 6
	 * @ipaddr: IP address for the neigh entry
	 * Returns: 0 on success, negative (<0) on failure
	 */
	int (*br_delete_ip_neigh)(void *priv, u8 version, const u8 *ipaddr);

	/**
	 * set_authmode - Set authentication algorithm(s) for static WEP
	 * @priv: Private driver interface data
	 * @authmode: 1=Open System, 2=Shared Key, 3=both
	 * Returns: 0 on success, -1 on failure
	 *
	 * This function can be used to set authentication algorithms for AP
	 * mode when static WEP is used. If the driver uses user space MLME/SME
	 * implementation, there is no need to implement this function.
	 *
	 * DEPRECATED - use set_ap() instead
	 */
	int (*set_authmode)(void *priv, int authmode);

#ifdef ANDROID
	/**
	 * driver_cmd - Execute driver-specific command
	 * @priv: Private driver interface data
	 * @cmd: Command to execute
	 * @buf: Return buffer
	 * @buf_len: Buffer length
	 * Returns: 0 on success, -1 on failure
	 */
	int (*driver_cmd)(void *priv, char *cmd, char *buf, size_t buf_len);
#endif /* ANDROID */

	/**
	 * vendor_cmd - Execute vendor specific command
	 * @priv: Private driver interface data
	 * @vendor_id: Vendor id
	 * @subcmd: Vendor command id
	 * @data: Vendor command parameters (%NULL if no parameters)
	 * @data_len: Data length
	 * @buf: Return buffer (%NULL to ignore reply)
	 * Returns: 0 on success, negative (<0) on failure
	 *
	 * This function handles vendor specific commands that are passed to
	 * the driver/device. The command is identified by vendor id and
	 * command id. Parameters can be passed as argument to the command
	 * in the data buffer. Reply (if any) will be filled in the supplied
	 * return buffer.
	 *
	 * The exact driver behavior is driver interface and vendor specific. As
	 * an example, this will be converted to a vendor specific cfg80211
	 * command in case of the nl80211 driver interface.
	 */
	int (*vendor_cmd)(void *priv, unsigned int vendor_id,
			  unsigned int subcmd, const u8 *data, size_t data_len,
			  struct wpabuf *buf);

	/**
	 * set_rekey_info - Set rekey information
	 * @priv: Private driver interface data
	 * @kek: Current KEK
	 * @kek_len: KEK length in octets
	 * @kck: Current KCK
	 * @kck_len: KCK length in octets
	 * @replay_ctr: Current EAPOL-Key Replay Counter
	 *
	 * This optional function can be used to provide information for the
	 * driver/firmware to process EAPOL-Key frames in Group Key Handshake
	 * while the host (including wpa_supplicant) is sleeping.
	 */
	void (*set_rekey_info)(void *priv, const u8 *kek, size_t kek_len,
			       const u8 *kck, size_t kck_len,
			       const u8 *replay_ctr);

	/**
	 * sta_assoc - Station association indication
	 * @priv: Private driver interface data
	 * @own_addr: Source address and BSSID for association frame
	 * @addr: MAC address of the station to associate
	 * @reassoc: flag to indicate re-association
	 * @status: association response status code
	 * @ie: assoc response ie buffer
	 * @len: ie buffer length
	 * Returns: 0 on success, -1 on failure
	 *
	 * This function indicates the driver to send (Re)Association
	 * Response frame to the station.
	 */
	 int (*sta_assoc)(void *priv, const u8 *own_addr, const u8 *addr,
			  int reassoc, u16 status, const u8 *ie, size_t len);

	/**
	 * add_tspec - Add traffic stream
	 * @priv: Private driver interface data
	 * @addr: MAC address of the station to associate
	 * @tspec_ie: tspec ie buffer
	 * @tspec_ielen: tspec ie length
	 * Returns: 0 on success, -1 on failure
	 *
	 * This function adds the traffic steam for the station
	 * and fills the medium_time in tspec_ie.
	 */
	 int (*add_tspec)(void *priv, const u8 *addr, u8 *tspec_ie,
			  size_t tspec_ielen);

	/**
	 * add_sta_node - Add a station node in the driver
	 * @priv: Private driver interface data
	 * @addr: MAC address of the station to add
	 * @auth_alg: authentication algorithm used by the station
	 * Returns: 0 on success, -1 on failure
	 *
	 * This function adds the station node in the driver, when
	 * the station gets added by FT-over-DS.
	 */
	int (*add_sta_node)(void *priv, const u8 *addr, u16 auth_alg);

	/**
	 * sched_scan - Request the driver to initiate scheduled scan
	 * @priv: Private driver interface data
	 * @params: Scan parameters
	 * Returns: 0 on success, -1 on failure
	 *
	 * This operation should be used for scheduled scan offload to
	 * the hardware. Every time scan results are available, the
	 * driver should report scan results event for wpa_supplicant
	 * which will eventually request the results with
	 * wpa_driver_get_scan_results2(). This operation is optional
	 * and if not provided or if it returns -1, we fall back to
	 * normal host-scheduled scans.
	 */
	int (*sched_scan)(void *priv, struct wpa_driver_scan_params *params);

	/**
	 * stop_sched_scan - Request the driver to stop a scheduled scan
	 * @priv: Private driver interface data
	 * Returns: 0 on success, -1 on failure
	 *
	 * This should cause the scheduled scan to be stopped and
	 * results should stop being sent. Must be supported if
	 * sched_scan is supported.
	 */
	int (*stop_sched_scan)(void *priv);

	/**
	 * poll_client - Probe (null data or such) the given station
	 * @priv: Private driver interface data
	 * @own_addr: MAC address of sending interface
	 * @addr: MAC address of the station to probe
	 * @qos: Indicates whether station is QoS station
	 *
	 * This function is used to verify whether an associated station is
	 * still present. This function does not need to be implemented if the
	 * driver provides such inactivity polling mechanism.
	 */
	void (*poll_client)(void *priv, const u8 *own_addr,
			    const u8 *addr, int qos);

	/**
	 * radio_disable - Disable/enable radio
	 * @priv: Private driver interface data
	 * @disabled: 1=disable 0=enable radio
	 * Returns: 0 on success, -1 on failure
	 *
	 * This optional command is for testing purposes. It can be used to
	 * disable the radio on a testbed device to simulate out-of-radio-range
	 * conditions.
	 */
	int (*radio_disable)(void *priv, int disabled);

	/**
	 * add_tx_ts - Add traffic stream
	 * @priv: Private driver interface data
	 * @tsid: Traffic stream ID
	 * @addr: Receiver address
	 * @user_prio: User priority of the traffic stream
	 * @admitted_time: Admitted time for this TS in units of
	 *	32 microsecond periods (per second).
	 * Returns: 0 on success, -1 on failure
	 */
	int (*add_tx_ts)(void *priv, u8 tsid, const u8 *addr, u8 user_prio,
			 u16 admitted_time);

	/**
	 * del_tx_ts - Delete traffic stream
	 * @priv: Private driver interface data
	 * @tsid: Traffic stream ID
	 * @addr: Receiver address
	 * Returns: 0 on success, -1 on failure
	 */
	int (*del_tx_ts)(void *priv, u8 tsid, const u8 *addr);

	/**
	 * Disable channel switching with TDLS peer
	 * @priv: Private driver interface data
	 * @addr: MAC address of the TDLS peer
	 * Returns: 0 on success, -1 on failure
	 *
	 * This function indicates to the driver that it should stop switching
	 * with a given TDLS peer.
	 */
	int (*tdls_disable_channel_switch)(void *priv, const u8 *addr);

	/**
	 * stop_ap - Removes beacon from AP
	 * @priv: Private driver interface data
	 * Returns: 0 on success, -1 on failure (or if not supported)
	 *
	 * This optional function can be used to disable AP mode related
	 * configuration. Unlike deinit_ap, it does not change to station
	 * mode.
	 */
	int (*stop_ap)(void *priv);

	/**
	 * get_survey - Retrieve survey data
	 * @priv: Private driver interface data
	 * @freq: If set, survey data for the specified frequency is only
	 *	being requested. If not set, all survey data is requested.
	 * Returns: 0 on success, -1 on failure
	 *
	 * Use this to retrieve:
	 *
	 * - the observed channel noise floor
	 * - the amount of time we have spent on the channel
	 * - the amount of time during which we have spent on the channel that
	 *   the radio has determined the medium is busy and we cannot
	 *   transmit
	 * - the amount of time we have spent receiving data
	 * - the amount of time we have spent transmitting data
	 *
	 * This data can be used for spectrum heuristics. One example is
	 * Automatic Channel Selection (ACS). The channel survey data is
	 * kept on a linked list on the channel data, one entry is added
	 * for each survey. The min_nf of the channel is updated for each
	 * survey.
	 */
	int (*get_survey)(void *priv, unsigned int freq);

	/**
	 * status - Get driver interface status information
	 * @priv: Private driver interface data
	 * @buf: Buffer for printing the status information
	 * @buflen: Maximum length of the buffer
	 * Returns: Length of written status information or -1 on failure
	 */
	int (*status)(void *priv, char *buf, size_t buflen);

	/**
	 * roaming - Set roaming policy for driver-based BSS selection
	 * @priv: Private driver interface data
	 * @allowed: Whether roaming within ESS is allowed
	 * @bssid: Forced BSSID if roaming is disabled or %NULL if not set
	 * Returns: Length of written status information or -1 on failure
	 *
	 * This optional callback can be used to update roaming policy from the
	 * associate() command (bssid being set there indicates that the driver
	 * should not roam before getting this roaming() call to allow roaming.
	 * If the driver does not indicate WPA_DRIVER_FLAGS_BSS_SELECTION
	 * capability, roaming policy is handled within wpa_supplicant and there
	 * is no need to implement or react to this callback.
	 */
	int (*roaming)(void *priv, int allowed, const u8 *bssid);

	/**
	 * disable_fils - Enable/disable FILS feature
	 * @priv: Private driver interface data
	 * @disable: 0-enable and 1-disable FILS feature
	 * Returns: 0 on success, -1 on failure
	 *
	 * This callback can be used to configure driver and below layers to
	 * enable/disable all FILS features.
	 */
	int (*disable_fils)(void *priv, int disable);

	/**
	 * set_mac_addr - Set MAC address
	 * @priv: Private driver interface data
	 * @addr: MAC address to use or %NULL for setting back to permanent
	 * Returns: 0 on success, -1 on failure
	 */
	int (*set_mac_addr)(void *priv, const u8 *addr);

#ifdef CONFIG_MACSEC
	int (*macsec_init)(void *priv, struct macsec_init_params *params);

	int (*macsec_deinit)(void *priv);

	/**
	 * macsec_get_capability - Inform MKA of this driver's capability
	 * @priv: Private driver interface data
	 * @cap: Driver's capability
	 * Returns: 0 on success, -1 on failure
	 */
	int (*macsec_get_capability)(void *priv, enum macsec_cap *cap);

	/**
	 * enable_protect_frames - Set protect frames status
	 * @priv: Private driver interface data
	 * @enabled: TRUE = protect frames enabled
	 *           FALSE = protect frames disabled
	 * Returns: 0 on success, -1 on failure (or if not supported)
	 */
	int (*enable_protect_frames)(void *priv, Boolean enabled);

	/**
	 * enable_encrypt - Set encryption status
	 * @priv: Private driver interface data
	 * @enabled: TRUE = encrypt outgoing traffic
	 *           FALSE = integrity-only protection on outgoing traffic
	 * Returns: 0 on success, -1 on failure (or if not supported)
	 */
	int (*enable_encrypt)(void *priv, Boolean enabled);

	/**
	 * set_replay_protect - Set replay protect status and window size
	 * @priv: Private driver interface data
	 * @enabled: TRUE = replay protect enabled
	 *           FALSE = replay protect disabled
	 * @window: replay window size, valid only when replay protect enabled
	 * Returns: 0 on success, -1 on failure (or if not supported)
	 */
	int (*set_replay_protect)(void *priv, Boolean enabled, u32 window);

	/**
	 * set_current_cipher_suite - Set current cipher suite
	 * @priv: Private driver interface data
	 * @cs: EUI64 identifier
	 * Returns: 0 on success, -1 on failure (or if not supported)
	 */
	int (*set_current_cipher_suite)(void *priv, u64 cs);

	/**
	 * enable_controlled_port - Set controlled port status
	 * @priv: Private driver interface data
	 * @enabled: TRUE = controlled port enabled
	 *           FALSE = controlled port disabled
	 * Returns: 0 on success, -1 on failure (or if not supported)
	 */
	int (*enable_controlled_port)(void *priv, Boolean enabled);

	/**
	 * get_receive_lowest_pn - Get receive lowest pn
	 * @priv: Private driver interface data
	 * @sa: secure association
	 * Returns: 0 on success, -1 on failure (or if not supported)
	 */
	int (*get_receive_lowest_pn)(void *priv, struct receive_sa *sa);

	/**
	 * get_transmit_next_pn - Get transmit next pn
	 * @priv: Private driver interface data
	 * @sa: secure association
	 * Returns: 0 on success, -1 on failure (or if not supported)
	 */
	int (*get_transmit_next_pn)(void *priv, struct transmit_sa *sa);

	/**
	 * set_transmit_next_pn - Set transmit next pn
	 * @priv: Private driver interface data
	 * @sa: secure association
	 * Returns: 0 on success, -1 on failure (or if not supported)
	 */
	int (*set_transmit_next_pn)(void *priv, struct transmit_sa *sa);

	/**
	 * set_receive_lowest_pn - Set receive lowest PN
	 * @priv: Private driver interface data
	 * @sa: secure association
	 * Returns: 0 on success, -1 on failure (or if not supported)
	 */
	int (*set_receive_lowest_pn)(void *priv, struct receive_sa *sa);

	/**
	 * create_receive_sc - create secure channel for receiving
	 * @priv: Private driver interface data
	 * @sc: secure channel
	 * @conf_offset: confidentiality offset (0, 30, or 50)
	 * @validation: frame validation policy (0 = Disabled, 1 = Checked,
	 *	2 = Strict)
	 * Returns: 0 on success, -1 on failure (or if not supported)
	 */
	int (*create_receive_sc)(void *priv, struct receive_sc *sc,
				 unsigned int conf_offset,
				 int validation);

	/**
	 * delete_receive_sc - delete secure connection for receiving
	 * @priv: private driver interface data from init()
	 * @sc: secure channel
	 * Returns: 0 on success, -1 on failure
	 */
	int (*delete_receive_sc)(void *priv, struct receive_sc *sc);

	/**
	 * create_receive_sa - create secure association for receive
	 * @priv: private driver interface data from init()
	 * @sa: secure association
	 * Returns: 0 on success, -1 on failure
	 */
	int (*create_receive_sa)(void *priv, struct receive_sa *sa);

	/**
	 * delete_receive_sa - Delete secure association for receive
	 * @priv: Private driver interface data from init()
	 * @sa: Secure association
	 * Returns: 0 on success, -1 on failure
	 */
	int (*delete_receive_sa)(void *priv, struct receive_sa *sa);

	/**
	 * enable_receive_sa - enable the SA for receive
	 * @priv: private driver interface data from init()
	 * @sa: secure association
	 * Returns: 0 on success, -1 on failure
	 */
	int (*enable_receive_sa)(void *priv, struct receive_sa *sa);

	/**
	 * disable_receive_sa - disable SA for receive
	 * @priv: private driver interface data from init()
	 * @sa: secure association
	 * Returns: 0 on success, -1 on failure
	 */
	int (*disable_receive_sa)(void *priv, struct receive_sa *sa);

	/**
	 * create_transmit_sc - create secure connection for transmit
	 * @priv: private driver interface data from init()
	 * @sc: secure channel
	 * @conf_offset: confidentiality offset (0, 30, or 50)
	 * Returns: 0 on success, -1 on failure
	 */
	int (*create_transmit_sc)(void *priv, struct transmit_sc *sc,
				  unsigned int conf_offset);

	/**
	 * delete_transmit_sc - delete secure connection for transmit
	 * @priv: private driver interface data from init()
	 * @sc: secure channel
	 * Returns: 0 on success, -1 on failure
	 */
	int (*delete_transmit_sc)(void *priv, struct transmit_sc *sc);

	/**
	 * create_transmit_sa - create secure association for transmit
	 * @priv: private driver interface data from init()
	 * @sa: secure association
	 * Returns: 0 on success, -1 on failure
	 */
	int (*create_transmit_sa)(void *priv, struct transmit_sa *sa);

	/**
	 * delete_transmit_sa - Delete secure association for transmit
	 * @priv: Private driver interface data from init()
	 * @sa: Secure association
	 * Returns: 0 on success, -1 on failure
	 */
	int (*delete_transmit_sa)(void *priv, struct transmit_sa *sa);

	/**
	 * enable_transmit_sa - enable SA for transmit
	 * @priv: private driver interface data from init()
	 * @sa: secure association
	 * Returns: 0 on success, -1 on failure
	 */
	int (*enable_transmit_sa)(void *priv, struct transmit_sa *sa);

	/**
	 * disable_transmit_sa - disable SA for transmit
	 * @priv: private driver interface data from init()
	 * @sa: secure association
	 * Returns: 0 on success, -1 on failure
	 */
	int (*disable_transmit_sa)(void *priv, struct transmit_sa *sa);
#endif /* CONFIG_MACSEC */

	/**
	 * set_band - Notify driver of band selection
	 * @priv: Private driver interface data
	 * @band: The selected band(s)
	 * Returns 0 on success, -1 on failure
	 */
	int (*set_band)(void *priv, enum set_band band);

	/**
	 * set_prob_oper_freq - Indicate probable P2P operating channel
	 * @priv: Private driver interface data
	 * @freq: Channel frequency in MHz
	 * Returns 0 on success, -1 on failure
	 *
	 * This command can be used to inform the driver of the operating
	 * frequency that an ongoing P2P group formation is likely to come up
	 * on. Local device is assuming P2P Client role.
	 */
	int (*set_prob_oper_freq)(void *priv, unsigned int freq);

	/**
	 * abort_scan - Request the driver to abort an ongoing scan
	 * @priv: Private driver interface data
	 * @scan_cookie: Cookie identifying the scan request. This is used only
	 *	when the vendor interface QCA_NL80211_VENDOR_SUBCMD_TRIGGER_SCAN
	 *	was used to trigger scan. Otherwise, 0 is used.
	 * Returns 0 on success, -1 on failure
	 */
	int (*abort_scan)(void *priv, u64 scan_cookie);

	/**
	 * configure_data_frame_filters - Request to configure frame filters
	 * @priv: Private driver interface data
	 * @filter_flags: The type of frames to filter (bitfield of
	 * WPA_DATA_FRAME_FILTER_FLAG_*)
	 * Returns: 0 on success or -1 on failure
	 */
	int (*configure_data_frame_filters)(void *priv, u32 filter_flags);

	/**
	 * p2p_lo_start - Start offloading P2P listen to device
	 * @priv: Private driver interface data
	 * @freq: Listening frequency (MHz) for P2P listen
	 * @period: Length of the listen operation in milliseconds
	 * @interval: Interval for running the listen operation in milliseconds
	 * @count: Number of times to run the listen operation
	 * @device_types: Device primary and secondary types
	 * @dev_types_len: Number of bytes for device_types
	 * @ies: P2P IE and WSC IE for Probe Response frames
	 * @ies_len: Length of ies in bytes
	 * Returns: 0 on success or -1 on failure
	 */
	int (*p2p_lo_start)(void *priv, unsigned int freq,
			    unsigned int period, unsigned int interval,
			    unsigned int count,
			    const u8 *device_types, size_t dev_types_len,
			    const u8 *ies, size_t ies_len);

	/**
	 * p2p_lo_stop - Stop P2P listen offload
	 * @priv: Private driver interface data
	 * Returns: 0 on success or -1 on failure
	 */
	int (*p2p_lo_stop)(void *priv);

	/**
	 * set_default_scan_ies - Set default scan IEs
	 * @priv: Private driver interface data
	 * @ies: Scan default IEs buffer
	 * @ies_len: Length of IEs in bytes
	 * Returns: 0 on success or -1 on failure
	 *
	 * The driver can use these by default when there are no scan IEs coming
	 * in the subsequent scan requests. Also in case of one or more of IEs
	 * given in set_default_scan_ies() are missing in the subsequent scan
	 * request, the driver should merge the missing scan IEs in the scan
	 * request from the IEs set by set_default_scan_ies() in the Probe
	 * Request frames sent.
	 */
	int (*set_default_scan_ies)(void *priv, const u8 *ies, size_t ies_len);

	/**
	 * set_tdls_mode - Set TDLS trigger mode to the host driver
	 * @priv: Private driver interface data
	 * @tdls_external_control: Represents if TDLS external trigger control
	 *  mode is enabled/disabled.
	 *
	 * This optional callback can be used to configure the TDLS external
	 * trigger control mode to the host driver.
	 */
	int (*set_tdls_mode)(void *priv, int tdls_external_control);

	/**
	 * get_bss_transition_status - Get candidate BSS's transition status
	 * @priv: Private driver interface data
	 * @params: Candidate BSS list
	 *
	 * Get the accept or reject reason code for a list of BSS transition
	 * candidates.
	 */
	struct wpa_bss_candidate_info *
	(*get_bss_transition_status)(void *priv,
				     struct wpa_bss_trans_info *params);
	/**
	 * ignore_assoc_disallow - Configure driver to ignore assoc_disallow
	 * @priv: Private driver interface data
	 * @ignore_disallow: 0 to not ignore, 1 to ignore
	 * Returns: 0 on success, -1 on failure
	 */
	int (*ignore_assoc_disallow)(void *priv, int ignore_disallow);

	/**
	 * set_bssid_blacklist - Set blacklist of BSSIDs to the driver
	 * @priv: Private driver interface data
	 * @num_bssid: Number of blacklist BSSIDs
	 * @bssids: List of blacklisted BSSIDs
	 */
	int (*set_bssid_blacklist)(void *priv, unsigned int num_bssid,
				   const u8 *bssid);

	/**
	 * set_4addr_mode - Set 4-address mode
	 * @priv: Private driver interface data
	 * @bridge_ifname: Bridge interface name
	 * @val: 0 - disable 4addr mode, 1 - enable 4addr mode
	 * Returns: 0 on success, < 0 on failure
	 */
	int (*set_4addr_mode)(void *priv, const char *bridge_ifname, int val);
};

/**
 * struct wpa_signal_info - Information about channel signal quality
 * @frequency: control frequency
 * @above_threshold: true if the above threshold was crossed
 *	(relevant for a CQM event)
 * @current_signal: in dBm
 * @avg_signal: in dBm
 * @avg_beacon_signal: in dBm
 * @current_noise: %WPA_INVALID_NOISE if not supported
 * @current_txrate: current TX rate
 * @chanwidth: channel width
 * @center_frq1: center frequency for the first segment
 * @center_frq2: center frequency for the second segment (if relevant)
 */

/* enum smps_mode - SMPS mode definitions */
enum smps_mode {
	SMPS_AUTOMATIC,
	SMPS_OFF,
	SMPS_DYNAMIC,
	SMPS_STATIC,

	/* Keep last */
	SMPS_INVALID,
};

struct wpa_signal_info {
	u32 frequency;
	int above_threshold;
	int current_signal;
	int avg_signal;
	int avg_beacon_signal;
	int current_noise;
	int current_txrate;
	enum chan_width chanwidth;
	int center_frq1;
	int center_frq2;
};

/**
 * struct wmm_params - WMM parameterss configured for this association
 * @info_bitmap: Bitmap of valid wmm_params info; indicates what fields
 *	of the struct contain valid information.
 * @uapsd_queues: Bitmap of ACs configured for uapsd (valid only if
 *	%WMM_PARAMS_UAPSD_QUEUES_INFO is set)
 */
struct wmm_params {
	u8 info_bitmap;
	u8 uapsd_queues;
};

/**
 * enum wpa_event_type - Event type for wpa_supplicant_event() calls
 */
enum wpa_event_type {
	/**
	 * EVENT_ASSOC - Association completed
	 *
	 * This event needs to be delivered when the driver completes IEEE
	 * 802.11 association or reassociation successfully.
	 * wpa_driver_ops::get_bssid() is expected to provide the current BSSID
	 * after this event has been generated. In addition, optional
	 * EVENT_ASSOCINFO may be generated just before EVENT_ASSOC to provide
	 * more information about the association. If the driver interface gets
	 * both of these events at the same time, it can also include the
	 * assoc_info data in EVENT_ASSOC call.
	 */
	EVENT_ASSOC,

	/**
	 * EVENT_DISASSOC - Association lost
	 *
	 * This event should be called when association is lost either due to
	 * receiving deauthenticate or disassociate frame from the AP or when
	 * sending either of these frames to the current AP. If the driver
	 * supports separate deauthentication event, EVENT_DISASSOC should only
	 * be used for disassociation and EVENT_DEAUTH for deauthentication.
	 * In AP mode, union wpa_event_data::disassoc_info is required.
	 */
	EVENT_DISASSOC,

	/**
	 * EVENT_MICHAEL_MIC_FAILURE - Michael MIC (TKIP) detected
	 *
	 * This event must be delivered when a Michael MIC error is detected by
	 * the local driver. Additional data for event processing is
	 * provided with union wpa_event_data::michael_mic_failure. This
	 * information is used to request new encyption key and to initiate
	 * TKIP countermeasures if needed.
	 */
	EVENT_MICHAEL_MIC_FAILURE,

	/**
	 * EVENT_SCAN_RESULTS - Scan results available
	 *
	 * This event must be called whenever scan results are available to be
	 * fetched with struct wpa_driver_ops::get_scan_results(). This event
	 * is expected to be used some time after struct wpa_driver_ops::scan()
	 * is called. If the driver provides an unsolicited event when the scan
	 * has been completed, this event can be used to trigger
	 * EVENT_SCAN_RESULTS call. If such event is not available from the
	 * driver, the driver wrapper code is expected to use a registered
	 * timeout to generate EVENT_SCAN_RESULTS call after the time that the
	 * scan is expected to be completed. Optional information about
	 * completed scan can be provided with union wpa_event_data::scan_info.
	 */
	EVENT_SCAN_RESULTS,

	/**
	 * EVENT_ASSOCINFO - Report optional extra information for association
	 *
	 * This event can be used to report extra association information for
	 * EVENT_ASSOC processing. This extra information includes IEs from
	 * association frames and Beacon/Probe Response frames in union
	 * wpa_event_data::assoc_info. EVENT_ASSOCINFO must be send just before
	 * EVENT_ASSOC. Alternatively, the driver interface can include
	 * assoc_info data in the EVENT_ASSOC call if it has all the
	 * information available at the same point.
	 */
	EVENT_ASSOCINFO,

	/**
	 * EVENT_INTERFACE_STATUS - Report interface status changes
	 *
	 * This optional event can be used to report changes in interface
	 * status (interface added/removed) using union
	 * wpa_event_data::interface_status. This can be used to trigger
	 * wpa_supplicant to stop and re-start processing for the interface,
	 * e.g., when a cardbus card is ejected/inserted.
	 */
	EVENT_INTERFACE_STATUS,

	/**
	 * EVENT_PMKID_CANDIDATE - Report a candidate AP for pre-authentication
	 *
	 * This event can be used to inform wpa_supplicant about candidates for
	 * RSN (WPA2) pre-authentication. If wpa_supplicant is not responsible
	 * for scan request (ap_scan=2 mode), this event is required for
	 * pre-authentication. If wpa_supplicant is performing scan request
	 * (ap_scan=1), this event is optional since scan results can be used
	 * to add pre-authentication candidates. union
	 * wpa_event_data::pmkid_candidate is used to report the BSSID of the
	 * candidate and priority of the candidate, e.g., based on the signal
	 * strength, in order to try to pre-authenticate first with candidates
	 * that are most likely targets for re-association.
	 *
	 * EVENT_PMKID_CANDIDATE can be called whenever the driver has updates
	 * on the candidate list. In addition, it can be called for the current
	 * AP and APs that have existing PMKSA cache entries. wpa_supplicant
	 * will automatically skip pre-authentication in cases where a valid
	 * PMKSA exists. When more than one candidate exists, this event should
	 * be generated once for each candidate.
	 *
	 * Driver will be notified about successful pre-authentication with
	 * struct wpa_driver_ops::add_pmkid() calls.
	 */
	EVENT_PMKID_CANDIDATE,

	/**
	 * EVENT_TDLS - Request TDLS operation
	 *
	 * This event can be used to request a TDLS operation to be performed.
	 */
	EVENT_TDLS,

	/**
	 * EVENT_FT_RESPONSE - Report FT (IEEE 802.11r) response IEs
	 *
	 * The driver is expected to report the received FT IEs from
	 * FT authentication sequence from the AP. The FT IEs are included in
	 * the extra information in union wpa_event_data::ft_ies.
	 */
	EVENT_FT_RESPONSE,

	/**
	 * EVENT_IBSS_RSN_START - Request RSN authentication in IBSS
	 *
	 * The driver can use this event to inform wpa_supplicant about a STA
	 * in an IBSS with which protected frames could be exchanged. This
	 * event starts RSN authentication with the other STA to authenticate
	 * the STA and set up encryption keys with it.
	 */
	EVENT_IBSS_RSN_START,

	/**
	 * EVENT_AUTH - Authentication result
	 *
	 * This event should be called when authentication attempt has been
	 * completed. This is only used if the driver supports separate
	 * authentication step (struct wpa_driver_ops::authenticate).
	 * Information about authentication result is included in
	 * union wpa_event_data::auth.
	 */
	EVENT_AUTH,

	/**
	 * EVENT_DEAUTH - Authentication lost
	 *
	 * This event should be called when authentication is lost either due
	 * to receiving deauthenticate frame from the AP or when sending that
	 * frame to the current AP.
	 * In AP mode, union wpa_event_data::deauth_info is required.
	 */
	EVENT_DEAUTH,

	/**
	 * EVENT_ASSOC_REJECT - Association rejected
	 *
	 * This event should be called when (re)association attempt has been
	 * rejected by the AP. Information about the association response is
	 * included in union wpa_event_data::assoc_reject.
	 */
	EVENT_ASSOC_REJECT,

	/**
	 * EVENT_AUTH_TIMED_OUT - Authentication timed out
	 */
	EVENT_AUTH_TIMED_OUT,

	/**
	 * EVENT_ASSOC_TIMED_OUT - Association timed out
	 */
	EVENT_ASSOC_TIMED_OUT,

	/**
	 * EVENT_WPS_BUTTON_PUSHED - Report hardware push button press for WPS
	 */
	EVENT_WPS_BUTTON_PUSHED,

	/**
	 * EVENT_TX_STATUS - Report TX status
	 */
	EVENT_TX_STATUS,

	/**
	 * EVENT_RX_FROM_UNKNOWN - Report RX from unknown STA
	 */
	EVENT_RX_FROM_UNKNOWN,

	/**
	 * EVENT_RX_MGMT - Report RX of a management frame
	 */
	EVENT_RX_MGMT,

	/**
	 * EVENT_REMAIN_ON_CHANNEL - Remain-on-channel duration started
	 *
	 * This event is used to indicate when the driver has started the
	 * requested remain-on-channel duration. Information about the
	 * operation is included in union wpa_event_data::remain_on_channel.
	 */
	EVENT_REMAIN_ON_CHANNEL,

	/**
	 * EVENT_CANCEL_REMAIN_ON_CHANNEL - Remain-on-channel timed out
	 *
	 * This event is used to indicate when the driver has completed
	 * remain-on-channel duration, i.e., may noot be available on the
	 * requested channel anymore. Information about the
	 * operation is included in union wpa_event_data::remain_on_channel.
	 */
	EVENT_CANCEL_REMAIN_ON_CHANNEL,

	/**
	 * EVENT_RX_PROBE_REQ - Indicate received Probe Request frame
	 *
	 * This event is used to indicate when a Probe Request frame has been
	 * received. Information about the received frame is included in
	 * union wpa_event_data::rx_probe_req. The driver is required to report
	 * these events only after successfully completed probe_req_report()
	 * commands to request the events (i.e., report parameter is non-zero)
	 * in station mode. In AP mode, Probe Request frames should always be
	 * reported.
	 */
	EVENT_RX_PROBE_REQ,

	/**
	 * EVENT_NEW_STA - New wired device noticed
	 *
	 * This event is used to indicate that a new device has been detected
	 * in a network that does not use association-like functionality (i.e.,
	 * mainly wired Ethernet). This can be used to start EAPOL
	 * authenticator when receiving a frame from a device. The address of
	 * the device is included in union wpa_event_data::new_sta.
	 */
	EVENT_NEW_STA,

	/**
	 * EVENT_EAPOL_RX - Report received EAPOL frame
	 *
	 * When in AP mode with hostapd, this event is required to be used to
	 * deliver the receive EAPOL frames from the driver.
	 */
	EVENT_EAPOL_RX,

	/**
	 * EVENT_SIGNAL_CHANGE - Indicate change in signal strength
	 *
	 * This event is used to indicate changes in the signal strength
	 * observed in frames received from the current AP if signal strength
	 * monitoring has been enabled with signal_monitor().
	 */
	EVENT_SIGNAL_CHANGE,

	/**
	 * EVENT_INTERFACE_ENABLED - Notify that interface was enabled
	 *
	 * This event is used to indicate that the interface was enabled after
	 * having been previously disabled, e.g., due to rfkill.
	 */
	EVENT_INTERFACE_ENABLED,

	/**
	 * EVENT_INTERFACE_DISABLED - Notify that interface was disabled
	 *
	 * This event is used to indicate that the interface was disabled,
	 * e.g., due to rfkill.
	 */
	EVENT_INTERFACE_DISABLED,

	/**
	 * EVENT_CHANNEL_LIST_CHANGED - Channel list changed
	 *
	 * This event is used to indicate that the channel list has changed,
	 * e.g., because of a regulatory domain change triggered by scan
	 * results including an AP advertising a country code.
	 */
	EVENT_CHANNEL_LIST_CHANGED,

	/**
	 * EVENT_INTERFACE_UNAVAILABLE - Notify that interface is unavailable
	 *
	 * This event is used to indicate that the driver cannot maintain this
	 * interface in its operation mode anymore. The most likely use for
	 * this is to indicate that AP mode operation is not available due to
	 * operating channel would need to be changed to a DFS channel when
	 * the driver does not support radar detection and another virtual
	 * interfaces caused the operating channel to change. Other similar
	 * resource conflicts could also trigger this for station mode
	 * interfaces. This event can be propagated when channel switching
	 * fails.
	 */
	EVENT_INTERFACE_UNAVAILABLE,

	/**
	 * EVENT_BEST_CHANNEL
	 *
	 * Driver generates this event whenever it detects a better channel
	 * (e.g., based on RSSI or channel use). This information can be used
	 * to improve channel selection for a new AP/P2P group.
	 */
	EVENT_BEST_CHANNEL,

	/**
	 * EVENT_UNPROT_DEAUTH - Unprotected Deauthentication frame received
	 *
	 * This event should be called when a Deauthentication frame is dropped
	 * due to it not being protected (MFP/IEEE 802.11w).
	 * union wpa_event_data::unprot_deauth is required to provide more
	 * details of the frame.
	 */
	EVENT_UNPROT_DEAUTH,

	/**
	 * EVENT_UNPROT_DISASSOC - Unprotected Disassociation frame received
	 *
	 * This event should be called when a Disassociation frame is dropped
	 * due to it not being protected (MFP/IEEE 802.11w).
	 * union wpa_event_data::unprot_disassoc is required to provide more
	 * details of the frame.
	 */
	EVENT_UNPROT_DISASSOC,

	/**
	 * EVENT_STATION_LOW_ACK
	 *
	 * Driver generates this event whenever it detected that a particular
	 * station was lost. Detection can be through massive transmission
	 * failures for example.
	 */
	EVENT_STATION_LOW_ACK,

	/**
	 * EVENT_IBSS_PEER_LOST - IBSS peer not reachable anymore
	 */
	EVENT_IBSS_PEER_LOST,

	/**
	 * EVENT_DRIVER_GTK_REKEY - Device/driver did GTK rekey
	 *
	 * This event carries the new replay counter to notify wpa_supplicant
	 * of the current EAPOL-Key Replay Counter in case the driver/firmware
	 * completed Group Key Handshake while the host (including
	 * wpa_supplicant was sleeping).
	 */
	EVENT_DRIVER_GTK_REKEY,

	/**
	 * EVENT_SCHED_SCAN_STOPPED - Scheduled scan was stopped
	 */
	EVENT_SCHED_SCAN_STOPPED,

	/**
	 * EVENT_DRIVER_CLIENT_POLL_OK - Station responded to poll
	 *
	 * This event indicates that the station responded to the poll
	 * initiated with @poll_client.
	 */
	EVENT_DRIVER_CLIENT_POLL_OK,

	/**
	 * EVENT_EAPOL_TX_STATUS - notify of EAPOL TX status
	 */
	EVENT_EAPOL_TX_STATUS,

	/**
	 * EVENT_CH_SWITCH - AP or GO decided to switch channels
	 *
	 * Described in wpa_event_data.ch_switch
	 * */
	EVENT_CH_SWITCH,

	/**
	 * EVENT_WNM - Request WNM operation
	 *
	 * This event can be used to request a WNM operation to be performed.
	 */
	EVENT_WNM,

	/**
	 * EVENT_CONNECT_FAILED_REASON - Connection failure reason in AP mode
	 *
	 * This event indicates that the driver reported a connection failure
	 * with the specified client (for example, max client reached, etc.) in
	 * AP mode.
	 */
	EVENT_CONNECT_FAILED_REASON,

	/**
	 * EVENT_DFS_RADAR_DETECTED - Notify of radar detection
	 *
	 * A radar has been detected on the supplied frequency, hostapd should
	 * react accordingly (e.g., change channel).
	 */
	EVENT_DFS_RADAR_DETECTED,

	/**
	 * EVENT_DFS_CAC_FINISHED - Notify that channel availability check has been completed
	 *
	 * After a successful CAC, the channel can be marked clear and used.
	 */
	EVENT_DFS_CAC_FINISHED,

	/**
	 * EVENT_DFS_CAC_ABORTED - Notify that channel availability check has been aborted
	 *
	 * The CAC was not successful, and the channel remains in the previous
	 * state. This may happen due to a radar being detected or other
	 * external influences.
	 */
	EVENT_DFS_CAC_ABORTED,

	/**
	 * EVENT_DFS_NOP_FINISHED - Notify that non-occupancy period is over
	 *
	 * The channel which was previously unavailable is now available again.
	 */
	EVENT_DFS_NOP_FINISHED,

	/**
	 * EVENT_SURVEY - Received survey data
	 *
	 * This event gets triggered when a driver query is issued for survey
	 * data and the requested data becomes available. The returned data is
	 * stored in struct survey_results. The results provide at most one
	 * survey entry for each frequency and at minimum will provide one
	 * survey entry for one frequency. The survey data can be os_malloc()'d
	 * and then os_free()'d, so the event callback must only copy data.
	 */
	EVENT_SURVEY,

	/**
	 * EVENT_SCAN_STARTED - Scan started
	 *
	 * This indicates that driver has started a scan operation either based
	 * on a request from wpa_supplicant/hostapd or from another application.
	 * EVENT_SCAN_RESULTS is used to indicate when the scan has been
	 * completed (either successfully or by getting cancelled).
	 */
	EVENT_SCAN_STARTED,

	/**
	 * EVENT_AVOID_FREQUENCIES - Received avoid frequency range
	 *
	 * This event indicates a set of frequency ranges that should be avoided
	 * to reduce issues due to interference or internal co-existence
	 * information in the driver.
	 */
	EVENT_AVOID_FREQUENCIES,

	/**
	 * EVENT_NEW_PEER_CANDIDATE - new (unknown) mesh peer notification
	 */
	EVENT_NEW_PEER_CANDIDATE,

	/**
	 * EVENT_ACS_CHANNEL_SELECTED - Received selected channels by ACS
	 *
	 * Indicates a pair of primary and secondary channels chosen by ACS
	 * in device.
	 */
	EVENT_ACS_CHANNEL_SELECTED,

	/**
	 * EVENT_DFS_CAC_STARTED - Notify that channel availability check has
	 * been started.
	 *
	 * This event indicates that channel availability check has been started
	 * on a DFS frequency by a driver that supports DFS Offload.
	 */
	EVENT_DFS_CAC_STARTED,

	/**
	 * EVENT_P2P_LO_STOP - Notify that P2P listen offload is stopped
	 */
	EVENT_P2P_LO_STOP,

	/**
	 * EVENT_BEACON_LOSS - Beacon loss detected
	 *
	 * This event indicates that no Beacon frames has been received from
	 * the current AP. This may indicate that the AP is not anymore in
	 * range.
	 */
	EVENT_BEACON_LOSS,

	/**
	 * EVENT_DFS_PRE_CAC_EXPIRED - Notify that channel availability check
	 * done previously (Pre-CAC) on the channel has expired. This would
	 * normally be on a non-ETSI DFS regulatory domain. DFS state of the
	 * channel will be moved from available to usable. A new CAC has to be
	 * performed before start operating on this channel.
	 */
	EVENT_DFS_PRE_CAC_EXPIRED,

	/**
	 * EVENT_EXTERNAL_AUTH - This event interface is used by host drivers
	 * that do not define separate commands for authentication and
	 * association (~WPA_DRIVER_FLAGS_SME) but offload the 802.11
	 * authentication to wpa_supplicant. This event carries all the
	 * necessary information from the host driver for the authentication to
	 * happen.
	 */
	EVENT_EXTERNAL_AUTH,

	/**
	 * EVENT_PORT_AUTHORIZED - Notification that a connection is authorized
	 *
	 * This event should be indicated when the driver completes the 4-way
	 * handshake. This event should be preceded by an EVENT_ASSOC that
	 * indicates the completion of IEEE 802.11 association.
	 */
	EVENT_PORT_AUTHORIZED,

	/**
	 * EVENT_STATION_OPMODE_CHANGED - Notify STA's HT/VHT operation mode
	 * change event.
	 */
	EVENT_STATION_OPMODE_CHANGED,

	/**
	 * EVENT_INTERFACE_MAC_CHANGED - Notify that interface MAC changed
	 *
	 * This event is emitted when the MAC changes while the interface is
	 * enabled. When an interface was disabled and becomes enabled, it
	 * must be always assumed that the MAC possibly changed.
	 */
	EVENT_INTERFACE_MAC_CHANGED,

	/**
	 * EVENT_WDS_STA_INTERFACE_STATUS - Notify WDS STA interface status
	 *
	 * This event is emitted when an interface is added/removed for WDS STA.
	 */
	EVENT_WDS_STA_INTERFACE_STATUS,
};


/**
 * union wpa_event_data - Additional data for wpa_supplicant_event() calls
 */
union wpa_event_data {
	/**
	 * struct assoc_info - Data for EVENT_ASSOC and EVENT_ASSOCINFO events
	 *
	 * This structure is optional for EVENT_ASSOC calls and required for
	 * EVENT_ASSOCINFO calls. By using EVENT_ASSOC with this data, the
	 * driver interface does not need to generate separate EVENT_ASSOCINFO
	 * calls.
	 */
	struct assoc_info {
		/**
		 * reassoc - Flag to indicate association or reassociation
		 */
		int reassoc;

		/**
		 * req_ies - (Re)Association Request IEs
		 *
		 * If the driver generates WPA/RSN IE, this event data must be
		 * returned for WPA handshake to have needed information. If
		 * wpa_supplicant-generated WPA/RSN IE is used, this
		 * information event is optional.
		 *
		 * This should start with the first IE (fixed fields before IEs
		 * are not included).
		 */
		const u8 *req_ies;

		/**
		 * req_ies_len - Length of req_ies in bytes
		 */
		size_t req_ies_len;

		/**
		 * resp_ies - (Re)Association Response IEs
		 *
		 * Optional association data from the driver. This data is not
		 * required WPA, but may be useful for some protocols and as
		 * such, should be reported if this is available to the driver
		 * interface.
		 *
		 * This should start with the first IE (fixed fields before IEs
		 * are not included).
		 */
		const u8 *resp_ies;

		/**
		 * resp_ies_len - Length of resp_ies in bytes
		 */
		size_t resp_ies_len;

		/**
		 * resp_frame - (Re)Association Response frame
		 */
		const u8 *resp_frame;

		/**
		 * resp_frame_len - (Re)Association Response frame length
		 */
		size_t resp_frame_len;

		/**
		 * beacon_ies - Beacon or Probe Response IEs
		 *
		 * Optional Beacon/ProbeResp data: IEs included in Beacon or
		 * Probe Response frames from the current AP (i.e., the one
		 * that the client just associated with). This information is
		 * used to update WPA/RSN IE for the AP. If this field is not
		 * set, the results from previous scan will be used. If no
		 * data for the new AP is found, scan results will be requested
		 * again (without scan request). At this point, the driver is
		 * expected to provide WPA/RSN IE for the AP (if WPA/WPA2 is
		 * used).
		 *
		 * This should start with the first IE (fixed fields before IEs
		 * are not included).
		 */
		const u8 *beacon_ies;

		/**
		 * beacon_ies_len - Length of beacon_ies */
		size_t beacon_ies_len;

		/**
		 * freq - Frequency of the operational channel in MHz
		 */
		unsigned int freq;

		/**
		 * wmm_params - WMM parameters used in this association.
		 */
		struct wmm_params wmm_params;

		/**
		 * addr - Station address (for AP mode)
		 */
		const u8 *addr;

		/**
		 * The following is the key management offload information
		 * @authorized
		 * @key_replay_ctr
		 * @key_replay_ctr_len
		 * @ptk_kck
		 * @ptk_kek_len
		 * @ptk_kek
		 * @ptk_kek_len
		 */

		/**
		 * authorized - Status of key management offload,
		 * 1 = successful
		 */
		int authorized;

		/**
		 * key_replay_ctr - Key replay counter value last used
		 * in a valid EAPOL-Key frame
		 */
		const u8 *key_replay_ctr;

		/**
		 * key_replay_ctr_len - The length of key_replay_ctr
		 */
		size_t key_replay_ctr_len;

		/**
		 * ptk_kck - The derived PTK KCK
		 */
		const u8 *ptk_kck;

		/**
		 * ptk_kek_len - The length of ptk_kck
		 */
		size_t ptk_kck_len;

		/**
		 * ptk_kek - The derived PTK KEK
		 * This is used in key management offload and also in FILS SK
		 * offload.
		 */
		const u8 *ptk_kek;

		/**
		 * ptk_kek_len - The length of ptk_kek
		 */
		size_t ptk_kek_len;

		/**
		 * subnet_status - The subnet status:
		 * 0 = unknown, 1 = unchanged, 2 = changed
		 */
		u8 subnet_status;

		/**
		 * The following information is used in FILS SK offload
		 * @fils_erp_next_seq_num
		 * @fils_pmk
		 * @fils_pmk_len
		 * @fils_pmkid
		 */

		/**
		 * fils_erp_next_seq_num - The next sequence number to use in
		 * FILS ERP messages
		 */
		u16 fils_erp_next_seq_num;

		/**
		 * fils_pmk - A new PMK if generated in case of FILS
		 * authentication
		 */
		const u8 *fils_pmk;

		/**
		 * fils_pmk_len - Length of fils_pmk
		 */
		size_t fils_pmk_len;

		/**
		 * fils_pmkid - PMKID used or generated in FILS authentication
		 */
		const u8 *fils_pmkid;
	} assoc_info;

	/**
	 * struct disassoc_info - Data for EVENT_DISASSOC events
	 */
	struct disassoc_info {
		/**
		 * addr - Station address (for AP mode)
		 */
		const u8 *addr;

		/**
		 * reason_code - Reason Code (host byte order) used in
		 *	Deauthentication frame
		 */
		u16 reason_code;

		/**
		 * ie - Optional IE(s) in Disassociation frame
		 */
		const u8 *ie;

		/**
		 * ie_len - Length of ie buffer in octets
		 */
		size_t ie_len;

		/**
		 * locally_generated - Whether the frame was locally generated
		 */
		int locally_generated;
	} disassoc_info;

	/**
	 * struct deauth_info - Data for EVENT_DEAUTH events
	 */
	struct deauth_info {
		/**
		 * addr - Station address (for AP mode)
		 */
		const u8 *addr;

		/**
		 * reason_code - Reason Code (host byte order) used in
		 *	Deauthentication frame
		 */
		u16 reason_code;

		/**
		 * ie - Optional IE(s) in Deauthentication frame
		 */
		const u8 *ie;

		/**
		 * ie_len - Length of ie buffer in octets
		 */
		size_t ie_len;

		/**
		 * locally_generated - Whether the frame was locally generated
		 */
		int locally_generated;
	} deauth_info;

	/**
	 * struct michael_mic_failure - Data for EVENT_MICHAEL_MIC_FAILURE
	 */
	struct michael_mic_failure {
		int unicast;
		const u8 *src;
	} michael_mic_failure;

	/**
	 * struct interface_status - Data for EVENT_INTERFACE_STATUS
	 */
	struct interface_status {
		unsigned int ifindex;
		char ifname[100];
		enum {
			EVENT_INTERFACE_ADDED, EVENT_INTERFACE_REMOVED
		} ievent;
	} interface_status;

	/**
	 * struct pmkid_candidate - Data for EVENT_PMKID_CANDIDATE
	 */
	struct pmkid_candidate {
		/** BSSID of the PMKID candidate */
		u8 bssid[ETH_ALEN];
		/** Smaller the index, higher the priority */
		int index;
		/** Whether RSN IE includes pre-authenticate flag */
		int preauth;
	} pmkid_candidate;

	/**
	 * struct tdls - Data for EVENT_TDLS
	 */
	struct tdls {
		u8 peer[ETH_ALEN];
		enum {
			TDLS_REQUEST_SETUP,
			TDLS_REQUEST_TEARDOWN,
			TDLS_REQUEST_DISCOVER,
		} oper;
		u16 reason_code; /* for teardown */
	} tdls;

	/**
	 * struct wnm - Data for EVENT_WNM
	 */
	struct wnm {
		u8 addr[ETH_ALEN];
		enum {
			WNM_OPER_SLEEP,
		} oper;
		enum {
			WNM_SLEEP_ENTER,
			WNM_SLEEP_EXIT
		} sleep_action;
		int sleep_intval;
		u16 reason_code;
		u8 *buf;
		u16 buf_len;
	} wnm;

	/**
	 * struct ft_ies - FT information elements (EVENT_FT_RESPONSE)
	 *
	 * During FT (IEEE 802.11r) authentication sequence, the driver is
	 * expected to use this event to report received FT IEs (MDIE, FTIE,
	 * RSN IE, TIE, possible resource request) to the supplicant. The FT
	 * IEs for the next message will be delivered through the
	 * struct wpa_driver_ops::update_ft_ies() callback.
	 */
	struct ft_ies {
		const u8 *ies;
		size_t ies_len;
		int ft_action;
		u8 target_ap[ETH_ALEN];
		/** Optional IE(s), e.g., WMM TSPEC(s), for RIC-Request */
		const u8 *ric_ies;
		/** Length of ric_ies buffer in octets */
		size_t ric_ies_len;
	} ft_ies;

	/**
	 * struct ibss_rsn_start - Data for EVENT_IBSS_RSN_START
	 */
	struct ibss_rsn_start {
		u8 peer[ETH_ALEN];
	} ibss_rsn_start;

	/**
	 * struct auth_info - Data for EVENT_AUTH events
	 */
	struct auth_info {
		u8 peer[ETH_ALEN];
		u8 bssid[ETH_ALEN];
		u16 auth_type;
		u16 auth_transaction;
		u16 status_code;
		const u8 *ies;
		size_t ies_len;
	} auth;

	/**
	 * struct assoc_reject - Data for EVENT_ASSOC_REJECT events
	 */
	struct assoc_reject {
		/**
		 * bssid - BSSID of the AP that rejected association
		 */
		const u8 *bssid;

		/**
		 * resp_ies - (Re)Association Response IEs
		 *
		 * Optional association data from the driver. This data is not
		 * required WPA, but may be useful for some protocols and as
		 * such, should be reported if this is available to the driver
		 * interface.
		 *
		 * This should start with the first IE (fixed fields before IEs
		 * are not included).
		 */
		const u8 *resp_ies;

		/**
		 * resp_ies_len - Length of resp_ies in bytes
		 */
		size_t resp_ies_len;

		/**
		 * status_code - Status Code from (Re)association Response
		 */
		u16 status_code;

		/**
		 * timed_out - Whether failure is due to timeout (etc.) rather
		 * than explicit rejection response from the AP.
		 */
		int timed_out;

		/**
		 * timeout_reason - Reason for the timeout
		 */
		const char *timeout_reason;

		/**
		 * fils_erp_next_seq_num - The next sequence number to use in
		 * FILS ERP messages
		 */
		u16 fils_erp_next_seq_num;
	} assoc_reject;

	struct timeout_event {
		u8 addr[ETH_ALEN];
	} timeout_event;

	/**
	 * struct tx_status - Data for EVENT_TX_STATUS events
	 */
	struct tx_status {
		u16 type;
		u16 stype;
		const u8 *dst;
		const u8 *data;
		size_t data_len;
		int ack;
	} tx_status;

	/**
	 * struct rx_from_unknown - Data for EVENT_RX_FROM_UNKNOWN events
	 */
	struct rx_from_unknown {
		const u8 *bssid;
		const u8 *addr;
		int wds;
	} rx_from_unknown;

	/**
	 * struct rx_mgmt - Data for EVENT_RX_MGMT events
	 */
	struct rx_mgmt {
		const u8 *frame;
		size_t frame_len;
		u32 datarate;

		/**
		 * drv_priv - Pointer to store driver private BSS information
		 *
		 * If not set to NULL, this is used for comparison with
		 * hostapd_data->drv_priv to determine which BSS should process
		 * the frame.
		 */
		void *drv_priv;

		/**
		 * freq - Frequency (in MHz) on which the frame was received
		 */
		int freq;

		/**
		 * ssi_signal - Signal strength in dBm (or 0 if not available)
		 */
		int ssi_signal;
	} rx_mgmt;

	/**
	 * struct remain_on_channel - Data for EVENT_REMAIN_ON_CHANNEL events
	 *
	 * This is also used with EVENT_CANCEL_REMAIN_ON_CHANNEL events.
	 */
	struct remain_on_channel {
		/**
		 * freq - Channel frequency in MHz
		 */
		unsigned int freq;

		/**
		 * duration - Duration to remain on the channel in milliseconds
		 */
		unsigned int duration;
	} remain_on_channel;

	/**
	 * struct scan_info - Optional data for EVENT_SCAN_RESULTS events
	 * @aborted: Whether the scan was aborted
	 * @freqs: Scanned frequencies in MHz (%NULL = all channels scanned)
	 * @num_freqs: Number of entries in freqs array
	 * @ssids: Scanned SSIDs (%NULL or zero-length SSID indicates wildcard
	 *	SSID)
	 * @num_ssids: Number of entries in ssids array
	 * @external_scan: Whether the scan info is for an external scan
	 * @nl_scan_event: 1 if the source of this scan event is a normal scan,
	 * 	0 if the source of the scan event is a vendor scan
	 * @scan_start_tsf: Time when the scan started in terms of TSF of the
	 *	BSS that the interface that requested the scan is connected to
	 *	(if available).
	 * @scan_start_tsf_bssid: The BSSID according to which %scan_start_tsf
	 *	is set.
	 */
	struct scan_info {
		int aborted;
		const int *freqs;
		size_t num_freqs;
		struct wpa_driver_scan_ssid ssids[WPAS_MAX_SCAN_SSIDS];
		size_t num_ssids;
		int external_scan;
		int nl_scan_event;
		u64 scan_start_tsf;
		u8 scan_start_tsf_bssid[ETH_ALEN];
	} scan_info;

	/**
	 * struct rx_probe_req - Data for EVENT_RX_PROBE_REQ events
	 */
	struct rx_probe_req {
		/**
		 * sa - Source address of the received Probe Request frame
		 */
		const u8 *sa;

		/**
		 * da - Destination address of the received Probe Request frame
		 *	or %NULL if not available
		 */
		const u8 *da;

		/**
		 * bssid - BSSID of the received Probe Request frame or %NULL
		 *	if not available
		 */
		const u8 *bssid;

		/**
		 * ie - IEs from the Probe Request body
		 */
		const u8 *ie;

		/**
		 * ie_len - Length of ie buffer in octets
		 */
		size_t ie_len;

		/**
		 * signal - signal strength in dBm (or 0 if not available)
		 */
		int ssi_signal;
	} rx_probe_req;

	/**
	 * struct new_sta - Data for EVENT_NEW_STA events
	 */
	struct new_sta {
		const u8 *addr;
	} new_sta;

	/**
	 * struct eapol_rx - Data for EVENT_EAPOL_RX events
	 */
	struct eapol_rx {
		const u8 *src;
		const u8 *data;
		size_t data_len;
	} eapol_rx;

	/**
	 * signal_change - Data for EVENT_SIGNAL_CHANGE events
	 */
	struct wpa_signal_info signal_change;

	/**
	 * struct best_channel - Data for EVENT_BEST_CHANNEL events
	 * @freq_24: Best 2.4 GHz band channel frequency in MHz
	 * @freq_5: Best 5 GHz band channel frequency in MHz
	 * @freq_overall: Best channel frequency in MHz
	 *
	 * 0 can be used to indicate no preference in either band.
	 */
	struct best_channel {
		int freq_24;
		int freq_5;
		int freq_overall;
	} best_chan;

	struct unprot_deauth {
		const u8 *sa;
		const u8 *da;
		u16 reason_code;
	} unprot_deauth;

	struct unprot_disassoc {
		const u8 *sa;
		const u8 *da;
		u16 reason_code;
	} unprot_disassoc;

	/**
	 * struct low_ack - Data for EVENT_STATION_LOW_ACK events
	 * @addr: station address
	 * @num_packets: Number of packets lost (consecutive packets not
	 * acknowledged)
	 */
	struct low_ack {
		u8 addr[ETH_ALEN];
		u32 num_packets;
	} low_ack;

	/**
	 * struct ibss_peer_lost - Data for EVENT_IBSS_PEER_LOST
	 */
	struct ibss_peer_lost {
		u8 peer[ETH_ALEN];
	} ibss_peer_lost;

	/**
	 * struct driver_gtk_rekey - Data for EVENT_DRIVER_GTK_REKEY
	 */
	struct driver_gtk_rekey {
		const u8 *bssid;
		const u8 *replay_ctr;
	} driver_gtk_rekey;

	/**
	 * struct client_poll - Data for EVENT_DRIVER_CLIENT_POLL_OK events
	 * @addr: station address
	 */
	struct client_poll {
		u8 addr[ETH_ALEN];
	} client_poll;

	/**
	 * struct eapol_tx_status
	 * @dst: Original destination
	 * @data: Data starting with IEEE 802.1X header (!)
	 * @data_len: Length of data
	 * @ack: Indicates ack or lost frame
	 *
	 * This corresponds to hapd_send_eapol if the frame sent
	 * there isn't just reported as EVENT_TX_STATUS.
	 */
	struct eapol_tx_status {
		const u8 *dst;
		const u8 *data;
		int data_len;
		int ack;
	} eapol_tx_status;

	/**
	 * struct ch_switch
	 * @freq: Frequency of new channel in MHz
	 * @ht_enabled: Whether this is an HT channel
	 * @ch_offset: Secondary channel offset
	 * @ch_width: Channel width
	 * @cf1: Center frequency 1
	 * @cf2: Center frequency 2
	 */
	struct ch_switch {
		int freq;
		int ht_enabled;
		int ch_offset;
		enum chan_width ch_width;
		int cf1;
		int cf2;
	} ch_switch;

	/**
	 * struct connect_failed - Data for EVENT_CONNECT_FAILED_REASON
	 * @addr: Remote client address
	 * @code: Reason code for connection failure
	 */
	struct connect_failed_reason {
		u8 addr[ETH_ALEN];
		enum {
			MAX_CLIENT_REACHED,
			BLOCKED_CLIENT
		} code;
	} connect_failed_reason;

	/**
	 * struct dfs_event - Data for radar detected events
	 * @freq: Frequency of the channel in MHz
	 */
	struct dfs_event {
		int freq;
		int ht_enabled;
		int chan_offset;
		enum chan_width chan_width;
		int cf1;
		int cf2;
	} dfs_event;

	/**
	 * survey_results - Survey result data for EVENT_SURVEY
	 * @freq_filter: Requested frequency survey filter, 0 if request
	 *	was for all survey data
	 * @survey_list: Linked list of survey data (struct freq_survey)
	 */
	struct survey_results {
		unsigned int freq_filter;
		struct dl_list survey_list; /* struct freq_survey */
	} survey_results;

	/**
	 * channel_list_changed - Data for EVENT_CHANNEL_LIST_CHANGED
	 * @initiator: Initiator of the regulatory change
	 * @type: Regulatory change type
	 * @alpha2: Country code (or "" if not available)
	 */
	struct channel_list_changed {
		enum reg_change_initiator initiator;
		enum reg_type type;
		char alpha2[3];
	} channel_list_changed;

	/**
	 * freq_range - List of frequency ranges
	 *
	 * This is used as the data with EVENT_AVOID_FREQUENCIES.
	 */
	struct wpa_freq_range_list freq_range;

	/**
	 * struct mesh_peer
	 *
	 * @peer: Peer address
	 * @ies: Beacon IEs
	 * @ie_len: Length of @ies
	 *
	 * Notification of new candidate mesh peer.
	 */
	struct mesh_peer {
		const u8 *peer;
		const u8 *ies;
		size_t ie_len;
	} mesh_peer;

	/**
	 * struct acs_selected_channels - Data for EVENT_ACS_CHANNEL_SELECTED
	 * @pri_channel: Selected primary channel
	 * @sec_channel: Selected secondary channel
	 * @vht_seg0_center_ch: VHT mode Segment0 center channel
	 * @vht_seg1_center_ch: VHT mode Segment1 center channel
	 * @ch_width: Selected Channel width by driver. Driver may choose to
	 *	change hostapd configured ACS channel width due driver internal
	 *	channel restrictions.
	 * hw_mode: Selected band (used with hw_mode=any)
	 */
	struct acs_selected_channels {
		u8 pri_channel;
		u8 sec_channel;
		u8 vht_seg0_center_ch;
		u8 vht_seg1_center_ch;
		u16 ch_width;
		enum hostapd_hw_mode hw_mode;
	} acs_selected_channels;

	/**
	 * struct p2p_lo_stop - Reason code for P2P Listen offload stop event
	 * @reason_code: Reason for stopping offload
	 *	P2P_LO_STOPPED_REASON_COMPLETE: Listen offload finished as
	 *	scheduled.
	 *	P2P_LO_STOPPED_REASON_RECV_STOP_CMD: Host requested offload to
	 *	be stopped.
	 *	P2P_LO_STOPPED_REASON_INVALID_PARAM: Invalid listen offload
	 *	parameters.
	 *	P2P_LO_STOPPED_REASON_NOT_SUPPORTED: Listen offload not
	 *	supported by device.
	 */
	struct p2p_lo_stop {
		enum {
			P2P_LO_STOPPED_REASON_COMPLETE = 0,
			P2P_LO_STOPPED_REASON_RECV_STOP_CMD,
			P2P_LO_STOPPED_REASON_INVALID_PARAM,
			P2P_LO_STOPPED_REASON_NOT_SUPPORTED,
		} reason_code;
	} p2p_lo_stop;

	/* For EVENT_EXTERNAL_AUTH */
	struct external_auth external_auth;

	/**
	 * struct sta_opmode - Station's operation mode change event
	 * @addr: The station MAC address
	 * @smps_mode: SMPS mode of the station
	 * @chan_width: Channel width of the station
	 * @rx_nss: RX_NSS of the station
	 *
	 * This is used as data with EVENT_STATION_OPMODE_CHANGED.
	 */
	struct sta_opmode {
		const u8 *addr;
		enum smps_mode smps_mode;
		enum chan_width chan_width;
		u8 rx_nss;
	} sta_opmode;

	/**
	 * struct wds_sta_interface - Data for EVENT_WDS_STA_INTERFACE_STATUS.
	 */
	struct wds_sta_interface {
		const u8 *sta_addr;
		const char *ifname;
		enum {
			INTERFACE_ADDED,
			INTERFACE_REMOVED
		} istatus;
	} wds_sta_interface;
};


/**
 * wpa_supplicant_event - Report a driver event for wpa_supplicant
 * @ctx: Context pointer (wpa_s); this is the ctx variable registered
 *	with struct wpa_driver_ops::init()
 * @event: event type (defined above)
 * @data: possible extra data for the event
 *
 * Driver wrapper code should call this function whenever an event is received
 * from the driver.
 */
void wpa_supplicant_event(void *ctx, enum wpa_event_type event,
			  union wpa_event_data *data);

/**
 * wpa_supplicant_event_global - Report a driver event for wpa_supplicant
 * @ctx: Context pointer (wpa_s); this is the ctx variable registered
 *	with struct wpa_driver_ops::init()
 * @event: event type (defined above)
 * @data: possible extra data for the event
 *
 * Same as wpa_supplicant_event(), but we search for the interface in
 * wpa_global.
 */
void wpa_supplicant_event_global(void *ctx, enum wpa_event_type event,
				 union wpa_event_data *data);

/* driver_common.c */
void wpa_scan_results_free(struct wpa_scan_results *res);

int wpa_drv_send_action(struct wpa_supplicant *wpa_s,
			unsigned int chan, unsigned int wait,
			const u8 *data, size_t data_len, int no_cck);

void wpa_supplicant_connect(struct wpa_supplicant *wpa_s,
			    struct wpa_bss *bss, char *ssid);
#endif /* DRIVER_H */
