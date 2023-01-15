all:
	gcc -L${MES_LIB_PATH} -ansi -Wall -pedantic-errors -g main.c -o oke -lmes
