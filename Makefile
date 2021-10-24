CFLAGS="-std=gnu17"

test-target: RingBuffer.o test.o
	gcc -pthread -o test-target RingBuffer.o test.o $(CFLAGS)
	rm -rf *.o

RingBuffer.o:
	gcc -c RingBuffer.c $(CFLAGS)

test.o:
	gcc -c test.c $(CFLAGS)

clean:
	rm -rf *.o ./test-target