obj-m += main.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	sudo ./sign.sh

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	