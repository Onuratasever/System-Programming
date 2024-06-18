all: clear compile run clean

clean:
	@rm -f *.out
	@rm -f *.o
	@rm -f FIFO1
	@rm -f FIFO2
	@rm -f hw2_executable

clear:
	@clear

compile:
	@gcc -o hw2_executable hw2.c
run:
	@./hw2_executable 8