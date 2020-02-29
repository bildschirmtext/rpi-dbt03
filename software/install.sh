#!/bin/bash

TARGET=/usr/local/sbin

mkdir -p $TARGET

cp rpi_dbt03 $TARGET/ 
cp rpi_dbt03.service /etc/systemd/system/
