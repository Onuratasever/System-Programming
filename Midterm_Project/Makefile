all: clean clear compile run

clean:
	@rm -f *.out
	@rm -f *.o
	@rm -f FIFO
	@rm -f CLIENT_FIFO*
	@rm -rf CLIENT_*
	@rm -rf *.tar


clear:
	@clear

compile:
	@gcc -o server server.c

run:
	@./server midterm 3