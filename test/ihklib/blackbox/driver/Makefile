CC = gcc
obj-m += test_driver.o

KERN_DIR=/usr/src/kernels/`uname -r`/

TARGET=modules

CPPFLAGS = 
LDFLAGS = 

all: $(TARGET)

modules:
	$(MAKE) -C $(KERN_DIR) M=$(PWD) modules

clean:
	rm -f $(TARGET) *.o 
	$(MAKE) -C $(KERN_DIR) M=$(PWD) clean
