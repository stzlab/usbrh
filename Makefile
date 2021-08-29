SRC = usbrh.c
EXE = usbrh

$(EXE): $(SRC)
	gcc -Wall -O -o $@ $^ -lhidapi-hidraw

clean: 
	rm -f $(EXE)
