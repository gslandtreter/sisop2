all: client server

client: client/client.c
	gcc -pthread -lncurses client/client.c -o bin/client
	
server: server/server.c server/chatclient.c server/chatclient.h
	gcc -pthread server/server.c server/chatclient.c -o bin/server

clean:
	rm bin/*
