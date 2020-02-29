#!/bin/bash


sudo ./install_packages.sh

cd firmware
make && sudo make program
cd ..

cd software
./compile.sh
sudo ./install.sh

sudo service rpi_dbt03 start
