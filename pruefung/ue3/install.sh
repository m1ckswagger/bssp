#!/bin/bash

sudo rmmod cdrv
sudo insmod cdrv.ko
sudo chmod 666 /dev/mydev*
dmesg
