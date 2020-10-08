#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/input.h>

#include <mach/gpio.h>
#include <asm/gpio.h>
#include <mach/platform.h> 

#include "hc-sr501.h"

#define DEVICE_NAME "hc-sr501"


MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("LIYAOLONG");


#define HDATA   	(PAD_GPIO_B +28)	



struct hc_sr501_platform_data hc_sr501_plat_data = {
	.gpio = HDATA,
	.active_low = 0, /*空闲状态电平*/
	.type = EV_MSC,
	.code = MSC_SCAN,
	.name =	"infrared",
	.desc = "hc-data",
	.debounce_interval = 5,
};



struct platform_device hc_sr501_dev = {
	.name	=	"hc-sr501",
	.id		= 	-1,
	.dev	=	{
		.platform_data	= &hc_sr501_plat_data,
	}
	
};

static int hc_sr501_dev_init(void)
{
	return platform_device_register(&hc_sr501_dev);
}

static void hc_sr501_dev_exit(void)
{
	platform_device_register(&hc_sr501_dev);
}

module_init(hc_sr501_dev_init);
module_exit(hc_sr501_dev_exit);


