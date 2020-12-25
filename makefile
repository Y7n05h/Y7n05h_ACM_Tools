object = Y7n05hACMTools.o DataGenerationTools.o

tools.out: $(object)
	g++ -o tool.out $(object)
Y7n05hACMTools.o : Y7n05hACMTools.cpp
    g++ -c Y7n05hACMTools.cpp -std=c++11 
DataGenerationTools.o : DataGenerationTools.cpp
    g++ -c DataGenerationTools.cpp -std=c++11 
clean :
    rm tools.out $(object)