LIBS=-lm
ARGS=-O2 --std=gnu99

run: tetris
	@./tetris

tetris: main.c ledwand.o tetris.o
	gcc --std=gnu99 -o $@ $^ ${LIBS} ${ARGS} -lz

tetris.o: tetris.c tetris.h
	gcc -c $^ ${ARGS}

ledwand.o: ledwand.c ledwand.h
	gcc -c $^ ${ARGS}

.PHONY: clean
clean:
	rm -f tetris tetris.o ledwand.o

