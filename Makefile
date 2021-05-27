output: prefix_distribution.o dir.o leaf.o stopwatch.o level_poptrie.o level_sail.o level_cptrie.o cptrie_ip6.o  poptrie_ip6.o sail_u_ip6.o sail_l_ip6.o main_ip6.c
	g++ -O2 prefix_distribution.o dir.o leaf.o stopwatch.o level_poptrie.o level_sail.o level_cptrie.o cptrie_ip6.o  poptrie_ip6.o sail_u_ip6.o sail_l_ip6.o main_ip6.c  -Wall -std=c++11 -w -o main_ip6

cptrie_ip6.o: cptrie_ip6.c cptrie_ip6.h
	g++ -O2 -Wall -std=c++11 -c -w cptrie_ip6.c

poptrie_ip6.o: poptrie_ip6.c poptrie_ip6.h
	g++ -O2 -Wall -std=c++11 -c -w  poptrie_ip6.c 

sail_u_ip6.o: sail_u_ip6.c sail_u_ip6.h
	g++ -O2 -Wall -std=c++11 -c -w sail_u_ip6.c

sail_l_ip6.o: sail_l_ip6.c sail_l_ip6.h
	g++ -O2 -Wall -std=c++11 -c -w sail_l_ip6.c

leaf.o: leaf.c leaf.h
	g++ -O2 -Wall -std=c++11 -c -w leaf.c

dir.o: dir.c dir.h
	g++ -O2 -Wall -std=c++11 -c -w dir.c

prefix_distribution.o: prefix_distribution.c prefix_distribution.h
	g++ -O2 -Wall -std=c++11 -c -w prefix_distribution.c

level_poptrie.o: level_poptrie.c level_poptrie.h
	g++ -O2 -Wall -std=c++11 -c -w level_poptrie.c

level_cptrie.o: level_cptrie.c level_cptrie.h
	g++ -O2 -Wall -std=c++11 -c -w level_cptrie.c


level_sail.o: level_sail.c level_sail.h
	g++ -O2 -Wall -std=c++11 -c -w level_sail.c

stopwatch.o: stopwatch.c stopwatch.h
	g++ -O2 -Wall -std=c++11 -c -w stopwatch.c

clean:
	rm *.o main_ip6

