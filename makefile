a: build
	mkdir -p test
	cd test && ../out/c main.lang

build:
	mkdir -p out
	$(CC) main.c -o out/c
