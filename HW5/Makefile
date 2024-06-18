all: clear compile run clean

valgrind1: clear compileV runV1 clean
valgrind2: clear compileV runV2 clean
valgrind3: clear compileV runV3 clean

clean:
	@rm -f *.out
	@rm -f *.o
	@rm -f MWCp*
	@rm -f main2Valgrind
	@rm -rf ../to*

clear:
	@clear

compile:
	@gcc -o MWCp 210104004087_main.c

compileV:
	@gcc -gdwarf-4 -g2 210104004087_main.c -o main2Valgrind

runV1:
	@valgrind --leak-check=full --leak-check=full --show-leak-kinds=all ./main2Valgrind 10 10 ../testdir/src/libvterm ../tocopy4

runV2:
	@valgrind --leak-check=full --leak-check=full --show-leak-kinds=all ./main2Valgrind 10 4 ../testdir/src/libvterm/src ../toCopy

runV3:
	@valgrind --leak-check=full --leak-check=full --show-leak-kinds=all ./main2Valgrind 10 10 ../testdir ../toCopy

run:
	@./MWCp 10 4 ../testdir/src/libvterm/src ../toCopy