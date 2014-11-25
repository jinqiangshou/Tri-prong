##CC=g++ -ftest-coverage -fprofile-arcs
CC=g++

all : tri-prong

tri-prong : tri-prong.o auxiliary_func.o build_request.o
    ${CC} -o tri-prong tri-prong.o auxiliary_func.o build_request.o -lpthread
	
tri-prong.o : tri-prong.cpp comm_struct.h comm_const.h version.h
	${CC} -c tri-prong.cpp

auxiliary_func.o : auxiliary_func.cpp auxiliary_func.h
	${CC} -c auxiliary_func.cpp
	
build_request.o : build_request.cpp build_request.h version.h
	${CC} -c build_request.cpp

.PHONY : clean

clean :
	rm -f tri-prong *.o
##	rm -f *.gcda *.gcno *.info
