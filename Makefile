OBJECTS=main.o w65c02.o w65c02_trace.o mem.o iwm.o acia.o console.o debugger.o gui.o
CFLAGS=-Wall -Wextra -DHIRES_GUI_WINDOW
LDFLAGS=-lcurses -lSDL2

all: a2c

a2c: ${OBJECTS}
	gcc -o a2c $^ ${LDFLAGS}

main.o: main.c
	gcc -c $^ ${CFLAGS}

w65c02.o: w65c02.c
	gcc -c $^ ${CFLAGS}

w65c02_trace.o: w65c02_trace.c
	gcc -c $^ ${CFLAGS}

mem.o: mem.c
	gcc -c $^ ${CFLAGS}

iwm.o: iwm.c
	gcc -c $^ ${CFLAGS}

acia.o: acia.c
	gcc -c $^ ${CFLAGS}

console.o: console.c
	gcc -c $^ ${CFLAGS}

debugger.o: debugger.c
	gcc -c $^ ${CFLAGS}

gui.o: gui.c
	gcc -c $^ ${CFLAGS}

.PHONY: clean
clean:
	rm -f *.o a2c

