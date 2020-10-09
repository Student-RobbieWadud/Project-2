all: palindrome master

%.out: %.cpp Makefile
        g++ $< -o $@ -std=c++0x

clean:
        rm $(EXEC) $(OBJS) palindrome master