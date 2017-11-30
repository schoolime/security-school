PROGNAME = pssa_serv
# Comment #1
PSS_COMMON_DIR = ../../pss_common
PSS_COMMON_OBJS = pss_hash.o
PSS_COMMON_TMP =$(addprefix $(PSS_COMMON_DIR)/,$(PSS_COMMON_OBJS))
PSS_COMMON_SRCS =$(patsubst %.o, %.c, $(PSS_COMMON_TMP))

# Comment #2
PSSA_COMMON_DIR = ../common

# Comment #3
THIS_SRCS = $(wildcard *.c)
THIS_OBJS = $(patsubst %.c, %.o, $(THIS_SRCS))

CC = gcc
CFLAGS += -Wall -g -I$(PSS_COMMON_DIR) -I$(PSSA_COMMON_DIR)
LDFLAGS = -lpthread -lm -lssl -lcrypto

$(PROGNAME): $(THIS_OBJS) $(PSS_COMMON_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) 

$(PSS_COMMON_OBJS): $(PSS_COMMON_SRCS)
	$(CC) -c $< -o $@

$(THIS_OBJS): $(THIS_SRCS)
	$(CC) -c $^ -o $@ $(CFLAGS)

clean:
	rm -f $(PSS_COMMON_OBJS) $(THIS_OBJS) $(PROGNAME) depend
