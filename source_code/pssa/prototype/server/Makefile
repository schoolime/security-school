CC = gcc
CFLAGS = -Wall -g
LDFLAGS = -lpthread
 
PROGNAME = pssap_server
SRCS = $(wildcard *.c)
OBJS = $(patsubst %.c, %.o, $(SRCS))
 
$(PROGNAME): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) 
 
clean:
	rm -f $(OBJS) $(PROGNAME) depend
 
depend: $(SRCS)
	$(CC) $(CFLAGS) *.c > $@