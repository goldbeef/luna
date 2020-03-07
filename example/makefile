all: example

INC =  -I/usr/local/Cellar/lua/5.3.5_1/include/lua5.3  -I../
LIB = -L/usr/local/Cellar/lua/5.3.5_1/lib -L.
FLAG = -llua -lluna
example: example.cpp
	g++ -std=c++17 example.cpp -o example $(INC) $(LIB) $(FLAG)
	g++ -E -std=c++17 example.cpp -o example.pre.cpp $(INC) 

clean:
	rm -rf  example example.pre.cpp
