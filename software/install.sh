#!/bin/bash

TARGET=/usr/local/sbin

mkdir -p $TARGET

cp rpi_dbt03.c rpi_dbt03_service.sh $TARGET/ 
rm $TARGET/rpi_dbt03
cp rpi_dbt03.service /etc/systemd/system/
systemctl enable rpi_dbt03
