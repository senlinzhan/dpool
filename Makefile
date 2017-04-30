main: main.o
	g++ -std=c++11 -Wall -o main main.o -lpthread

main.o: main.cpp
	g++ -std=c++11 -Wall -g -c main.cpp

clean:
	rm main *.o

test: main
	./main
