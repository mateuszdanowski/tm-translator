tm_translator: tm_translator.cpp turing_machine.cpp turing_machine.h
	g++ -std=c++11 -Wall -Wshadow $(filter %.cpp,$^) -o $@

tm_interpreter: tm_interpreter.cpp turing_machine.cpp turing_machine.h
	g++ -std=c++11 -Wall -Wshadow $(filter %.cpp,$^) -o $@

clean:
	rm -rf tm_translator tm_interpreter *~