CC=cc
LDLIBS=-lncursesw
CFLAGS=-Wall

%: %.c*
	$(CC) $(CFLAGS) $< $(LDLIBS) -o $@
    
all: Show

clean:
	rm -f Show
	

