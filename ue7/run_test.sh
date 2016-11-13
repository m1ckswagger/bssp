#!/bin/bash
make
sudo rmmod cdrv
sudo dmesg --clear
sudo insmod cdrv.ko
dmesg
sudo dmesg --clear

gcc -Wall -O -g -o test_cdrv test_cdrv.c -pthread
sudo ./test_cdrv
#sudo dmesg
rm -f test_cdrv
