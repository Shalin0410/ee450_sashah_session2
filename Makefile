all: serverM serverA serverR serverD client

serverM: serverM.c
	gcc -o serverM serverM.c

serverA: serverA.c
	gcc -o serverA serverA.c

serverR: serverR.c
	gcc -o serverR serverR.c

serverD: serverD.c
	gcc -o serverD serverD.c

client: client.c
	gcc -o client client.c

clean:
	rm -f serverM serverA serverR serverD client
