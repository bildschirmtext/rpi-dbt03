#!/bin/bash


for x in *.c
do
	gcc -o `basename $x .c` $x -Wall -Werror
done
