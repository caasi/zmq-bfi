all:
	gcc ./bfi.c -o ./bfi `pkg-config --libs libzmq`
	gcc ./receiver.c -o ./receiver `pkg-config --libs libzmq`
clean:
	rm ./bfi -f
	rm ./receiver -f
