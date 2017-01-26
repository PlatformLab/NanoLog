all: aggregate simpleRead

aggregate: aggregate.cpp
	g++ -std=c++11 -O3 -DNDEBUG aggregate.cpp -o aggregate

simpleRead: simpleRead.cpp
	g++ -std=c++11 -O3 -DNDEBUG simpleRead.cpp -o simpleRead

clean:
	rm -f aggregate simpleRead