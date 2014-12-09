##CC=g++ -ftest-coverage -fprofile-arcs
CC=g++

all : tri-prong

tri-prong : tri-prong.o auxiliary_function.o build_request.o output.o
	${CC} -lpthread -o tri-prong tri-prong.o auxiliary_function.o build_request.o output.o
	
tri-prong.o : tri-prong.cpp *.h
	${CC} -c tri-prong.cpp

auxiliary_function.o : auxiliary_function.cpp auxiliary_function.h
	${CC} -c auxiliary_function.cpp
	
build_request.o : build_request.cpp build_request.h common_struct.h common_constant.h version.h
	${CC} -c build_request.cpp
	
output.o : output.cpp common_struct.h common_constant.h version.h
	${CC} -c output.cpp
	
.PHONY : clean

clean :
	rm -f tri-prong *.o
##	rm -f *.gcda *.gcno *.info tri-prong *.o
