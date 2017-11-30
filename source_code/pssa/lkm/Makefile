# Comment #1
PSS_COMMON_DIR = $(PWD)/../../pss_common
PSS_COMMON_FILES = pss_ksock.c pss_hash.c
PSS_COMMON_OBJS = $(patsubst %.c, %.o, $(PSS_COMMON_FILES))

# Comment #2
PSSA_COMMON_DIR = $(PWD)/../common

# Comment #3
obj-m += pssa_lkm.o
pssa_lkm-objs := pssal_filter.o pss_ksock.o $(PSS_COMMON_OBJS)

EXTRA_CFLAGS += -I$(PSS_COMMON_DIR) -I$(PSSA_COMMON_DIR) -DDEBUG

all: 
	for f in $(PSS_COMMON_FILES); do cp $(PSS_COMMON_DIR)/$$f ./; done
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	rm -f $(PSS_COMMON_FILES)
	rm -f $(PSSA_COMMON_FILES)
clean:
	rm -f $(PSS_COMMON_FILES)
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
