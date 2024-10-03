#!/bin/bash

cd /usr/local/sbin


BTX_HOST="btx.clarke-3.de"
BTX_PORT="20000"

if [ ! -f rpi_dbt03 ]
then
	echo "Binary not found, compiling"
	gcc -o rpi_dbt03 rpi_dbt03.c -Wall -Werror
fi

if [ -f /boot/btx_config.txt ]
then
	echo "config file found"
	source /boot/btx_config.txt
fi
echo $BTX_HOST $BTX_PORT

./rpi_dbt03 $BTX_HOST $BTX_PORT
