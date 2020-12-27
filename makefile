object = Y7n05hACMTools.o

tools.out: $(object)
	g++ -o tool.out $(object)
Y7n05hACMTools.o : Y7n05hACMTools.cpp DataGenerationTools.h
    g++ -c Y7n05hACMTools.cpp -std=c++11 
clean :
    rm tools.out $(object)