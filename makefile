a: build
	mkdir -p test
	cd test \
		&& ../out/c main.lang \
		&& $(CC) out.c -o prog \
		&& ./prog

build:
	mkdir -p out
	$(CC) main.c -o out/c
