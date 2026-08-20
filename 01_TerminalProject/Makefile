CC=cc
LDLIBS=-lncursesw
CFLAGS=-Wall -Wextra -Werror -std=c11

%: %.c*
	$(CC) $(CFLAGS) $< $(LDLIBS) -o $@
    
all: Show

clean:
	rm -f Show
	

