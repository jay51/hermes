exec = hermes.out
sources = $(wildcard src/*.c)
objects = $(sources:.c=.o)
flags = -g -Wall -lm -ldl -fPIC -rdynamic -std=c99


$(exec): $(objects)
	gcc $(objects) $(flags) -o $(exec)

libhermes.a: $(objects)
	ar rcs $@ $^

%.o: %.c include/%.h
	gcc -c $(flags) $< -o $@

install:
	make
	make libhermes.a
	mkdir -p /usr/local/include/hermes
	cp -r ./src/include/* /usr/local/include/hermes/.
	cp ./libhermes.a /usr/local/lib/.
	cp ./hermes.out /usr/local/bin/hermes

clean:
	-rm *.out
	-rm *.o
	-rm *.a
	-rm src/*.o

lint:
	clang-tidy src/*.c src/include/*.h
