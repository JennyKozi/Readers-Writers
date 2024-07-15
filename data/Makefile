all: verify

verify: verify.c
	gcc verify.c -o verify
	
run: verify
	./verify voters50.bin

clean:
	rm -rf verify verify.o
