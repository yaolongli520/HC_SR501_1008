#ifndef _HC_SR501_H
#define _HC_SR501_H

struct hc_sr501_platform_data {
	unsigned int code;	/* input event code (KEY_*, SW_*) */
	int gpio;		/* -1 if this key does not support gpio */
	int active_low;
	unsigned int type;	/* input event type (EV_KEY, EV_SW, EV_ABS) */
	const char *name;		/* input device name */
	const char *desc;
	int debounce_interval;  /*防抖动 毫秒*/
};



#endif




