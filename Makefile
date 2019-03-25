all : main.o
	gcc -Wall main.o

lib : main.o
	gcc -shared -fPIC main.o -o libso_stdio.so

main.o : main.c
	gcc -c -fPIC main.c

clean:
	rm -rf *.o