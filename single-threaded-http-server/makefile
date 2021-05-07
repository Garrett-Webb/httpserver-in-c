httpserver : httpserver.o
	g++ -o httpserver httpserver.o
httpserver.o : httpserver.cpp
	g++ -c -std=gnu++11 -Wall -Wextra -Wpedantic -Wshadow httpserver.cpp

clean:
	rm -f httpserver httpserver.o