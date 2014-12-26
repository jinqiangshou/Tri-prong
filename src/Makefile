##CC=g++ -ftest-coverage -fprofile-arcs
CC=g++

all : tri-prong

tri-prong : tri-prong.o auxiliary_function.o build_request.o output.o parse_url.o
	${CC} -lpthread -o tri-prong tri-prong.o auxiliary_function.o build_request.o output.o parse_url.o
	
tri-prong.o : tri-prong.cpp *.h
	${CC} -c tri-prong.cpp
	
parse_url.o : parse_url.cpp *.h
	${CC} -c parse_url.cpp
	
auxiliary_function.o : auxiliary_function.cpp *.h
	${CC} -c auxiliary_function.cpp
	
build_request.o : build_request.cpp *.h
	${CC} -c build_request.cpp
	
output.o : output.cpp *.h
	${CC} -c output.cpp
	
.PHONY : clean

clean :
	rm -f tri-prong *.o
##	rm -f *.gcda *.gcno *.info tri-prong *.o
