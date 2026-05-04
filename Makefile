all: terrain_generator

terrain_generator: terrain_generator.cpp pokedex.hpp pokemon_gen.hpp battle.hpp
	g++ -o terrain_generator terrain_generator.cpp -std=c++17 -g -Wall -Werror -lncurses -lm

clean:
	rm -f terrain_generator *~ *.o core

clobber: clean
	rm -f terrain_generator