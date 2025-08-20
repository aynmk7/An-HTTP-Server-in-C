CC=		gcc
CFLAGS=		-g -gdwarf-2 -Wall -std=gnu99
LD=		gcc
LDFLAGS=	-L.
TARGETS=	httpServer

# source and object lists
SRCS=           mainServer.c socket.c single.c forking.c request.c handler.c utilities.c
OBJS=           $(SRCS:.c=.o)

all:            $(TARGETS)

# link the final binary
httpServer:         $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

# compile each .c into a .o
%.o:            %.c mainServer.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	@echo Cleaning...
	@rm -f $(TARGETS) *.o *.log *.input

.PHONY:		all clean
