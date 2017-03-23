OBJS = $(patsubst %.c,%.o,$(wildcard *.c))

# add mcip include file path and mcip lib path
#CFLAGS = -Wall -I../mcip/include
#LDFLAGS = -L../mcip/libmcip

%.o: %.c
	$(CC) $(CFLAGS) -c $<

mcip-tool: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -lmcip -o $@ $(OBJS)

clean:
	rm -Rf mcip-tool *.o
