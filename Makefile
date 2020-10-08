obj-m+=hc-sr501.o
obj-m+=hc-sr501_dev.o

pwd:=`pwd`
kdir:=/root/linux-3.4.y

all:
	make -C $(kdir) M=$(pwd) modules

clean:
	rm -rf *.o *.ko