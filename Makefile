all: myprog reader writer

myprog: myprog.o
	gcc -pthread -o myprog myprog.o
	
myprog.o: myprog.c
	gcc -pthread -c -o myprog.o myprog.c

reader: reader.o
	gcc -pthread -o reader reader.o

reader.o: reader.c
	gcc -pthread -c -o reader.o reader.c

writer: writer.o
	gcc -pthread -o writer writer.o
	
writer.o: writer.c
	gcc -pthread -c -o writer.o writer.c
	
#######################################################

run: myprog
	./myprog exec2.txt

valgrind: myprog
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --show-reachable=yes ./myprog exec2.txt

clean:
	rm -rf myprog myprog.o reader reader.o writer writer.o