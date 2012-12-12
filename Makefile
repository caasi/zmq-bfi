all:
	gcc ./bfi.c -o ./bfi `pkg-config --libs libzmq`
clean:
	rm ./bfi -f
