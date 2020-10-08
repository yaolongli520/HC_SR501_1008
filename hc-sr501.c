#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include "hc-sr501.h"


#define DEVICE_NAME "hc-sr501"


MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("LIYAOLONG");


struct hc_sr501_drvdata {
	int gpio;
	int active_low;
	unsigned int type;	/* input event type (EV_KEY, EV_SW, EV_ABS) */
	struct input_dev *input;
	int code;		/* axis value for EV_ABS */
	unsigned int irq;
	spinlock_t lock;
	struct mutex disable_lock;
	struct work_struct work;
	struct timer_list timer;
	int timer_debounce;
};


static void hc_sr501_gpio_report_event(struct hc_sr501_drvdata *ddata)
{
	struct input_dev *input = ddata->input;
	unsigned int type = ddata->type ?: EV_KEY;
	int state = (gpio_get_value_cansleep(ddata->gpio) ? 1 : 0) ^ ddata->active_low;

	if (type == EV_ABS) {
		if (state)
			input_event(input, type, ddata->code, !!state);
	} else {
		input_event(input, type, ddata->code, !!state);
	}
	input_sync(input);
}

static void hc_sr501_gpio_work_func(struct work_struct *work)
{
	struct hc_sr501_drvdata *ddata =
		container_of(work, struct hc_sr501_drvdata, work);
	hc_sr501_gpio_report_event(ddata);
}

static void hc_sr501_gpio_timer(unsigned long _data)
{
	struct hc_sr501_drvdata *ddata = (struct hc_sr501_drvdata *)_data;
	schedule_work(&ddata->work);
}

static irqreturn_t hc_sr501_gpio_isr(int irq, void *dev_id)
{
	struct hc_sr501_drvdata *ddata = dev_id;
	
	BUG_ON(irq != ddata->irq);
	
	if (ddata->timer_debounce) //延时执行
		mod_timer(&ddata->timer,
			jiffies + msecs_to_jiffies(ddata->timer_debounce));
	else
		schedule_work(&ddata->work);//直接执行
	
	return IRQ_HANDLED;
}

static int  hc_sr501_setup(struct platform_device *pdev,
					 struct input_dev *input,
					 struct hc_sr501_drvdata *ddata,
					 const struct hc_sr501_platform_data *pdata)
{
	const char *desc = pdata->desc ? pdata->desc : "hc-data";
	struct device *dev = &pdev->dev;
	irq_handler_t isr;
	unsigned long irqflags;
	int irq, error;
	
	spin_lock_init(&ddata->lock);
	
	if (gpio_is_valid(pdata->gpio)) {
		
		error = gpio_request(pdata->gpio, desc);
		if (error < 0) {
			dev_err(dev, "Failed to request GPIO %d, error %d\n",
				pdata->gpio, error);
			return error;
		}
		ddata->gpio = pdata->gpio;
		ddata->active_low = pdata->active_low;
		ddata->code = pdata->code;
		ddata->type = pdata->type;
		error = gpio_direction_input(pdata->gpio);
		if (error < 0) {
			dev_err(dev,"Failed to configure direction for GPIO %d, error %d\n",
				pdata->gpio, error);
			goto fail;
		}

		if (pdata->debounce_interval) {
			error = gpio_set_debounce(pdata->gpio,
					pdata->debounce_interval * 1000);
			/* use timer if gpiolib doesn't provide debounce */
			//一般GPIO无硬件消抖
			if (error < 0)
				ddata->timer_debounce =
						pdata->debounce_interval;
		}

		irq = gpio_to_irq(pdata->gpio);
		if (irq < 0) {
			error = irq;
			dev_err(dev,
				"Unable to get irq number for GPIO %d, error %d\n",
				pdata->gpio, error);
			goto fail;
		}
		ddata->irq = irq;
		
		INIT_WORK(&ddata->work, hc_sr501_gpio_work_func);
		
		setup_timer(&ddata->timer,
			    hc_sr501_gpio_timer, (unsigned long)ddata);
		
		isr = hc_sr501_gpio_isr;
		irqflags = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING;
	}
	
	input_set_capability(input, pdata->type ?: EV_KEY, pdata->code);

	error = request_any_context_irq(ddata->irq, isr, irqflags, desc, ddata);
	if (error < 0) {
		dev_err(dev, "Unable to claim irq %d; error %d\n",
			ddata->irq, error);
		goto fail;
	}
	
	return 0;

fail:
	if (gpio_is_valid(pdata->gpio))
		gpio_free(pdata->gpio);

	return error;	
	
}


static void hc_sr501_remove_cfg(struct hc_sr501_drvdata *ddata)
{
	free_irq(ddata->irq, ddata);
	if (ddata->timer_debounce)
		del_timer_sync(&ddata->timer);
	cancel_work_sync(&ddata->work);
	if (gpio_is_valid(ddata->gpio))
		gpio_free(ddata->gpio);
}

static int  hc_sr501_probe(struct platform_device *pdev)
{
	const struct hc_sr501_platform_data *pdata = pdev->dev.platform_data;
	struct hc_sr501_drvdata *ddata;
	struct device *dev = &pdev->dev;
	struct input_dev *input;
	int error;
	
	if (!pdata) {
		error = -ENODEV;
		return error;
	}
	
	ddata = kzalloc(sizeof(struct hc_sr501_drvdata),
			GFP_KERNEL);
	input = input_allocate_device();
	if (!ddata || !input) {
		dev_err(dev, "failed to allocate state\n");
		error = -ENOMEM;
		goto fail1;
	}
	
	ddata->input = input;
	mutex_init(&ddata->disable_lock);
	platform_set_drvdata(pdev, ddata);
	input_set_drvdata(input, ddata);
	
	input->name =  pdata->name ? : pdev->name;
	input->phys = "hc-sr501/input0";
	input->dev.parent = &pdev->dev;
	
	input->id.bustype = BUS_HOST;
	input->id.vendor = 0x0002;
	input->id.product = 0x0002;
	input->id.version = 0x0100;
	
	error = hc_sr501_setup(pdev,input,ddata,pdata);
	if (error)
		goto fail2;
	
	error = input_register_device(input);
	if (error) {
		dev_err(dev, "Unable to register input device, error: %d\n",
			error);
		goto fail3;
	}	
	
	return 0;

fail3:	
	
fail2:
	hc_sr501_remove_cfg(ddata);
	platform_set_drvdata(pdev, NULL);
	
fail1:
	input_free_device(input);
	kfree(ddata);
	
	return error;
	
}

static int  hc_sr501_remove(struct platform_device *pdev)
{
	struct hc_sr501_drvdata *ddata = platform_get_drvdata(pdev);
	struct input_dev *input = ddata->input;
	
	hc_sr501_remove_cfg(ddata);
	input_unregister_device(input);
	kfree(ddata);
	return 0;
}

static struct platform_driver hc_sr501_driver = {
	.probe = hc_sr501_probe,	
	.remove = hc_sr501_remove,
	.driver = {
		.name = DEVICE_NAME,
		.owner	= THIS_MODULE,
	},
};

static int hc_sr501_init(void)
{
	return platform_driver_register(&hc_sr501_driver);
}

static void hc_sr501_exit(void)
{
	platform_driver_unregister(&hc_sr501_driver);
}

module_init(hc_sr501_init);
module_exit(hc_sr501_exit);


