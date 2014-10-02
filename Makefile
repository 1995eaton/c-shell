CFLAGS=-Wall -pedantic -std=gnu11
LIBS=

SRC=main.c term.c

OBJ=$(SRC:.c=.o)
BUILD=build

all: clean $(SRC) $(BUILD)

$(BUILD): $(OBJ)
	gcc $(LIBS) $(OBJ) -o $@

.c.o:
	gcc -c $(CFLAGS) $(LIBS) $< -o $@

clean:
	rm -f *.o $(BUILD)
