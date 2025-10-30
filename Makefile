all: server client

server: server.c allStruct.h Customer.h Employee.h Manager.h Admin.h
	gcc server.c -o server -pthread -lcrypt -lrt

client: client.c
	gcc client.c -o client

clean:
	rm -f server client
