#ifndef MY_IOCTL_H
#define MY_IOCTL_H

#include <asm-generic/ioctl.h>

#define IOC_MY_MAGIC 0xA4			

#define IOC_NR_OPENREADCNT 5
#define IOC_NR_OPENWRITE 6
#define IOC_NR_CLEAR 7
#define IOC_NR_READ_MAX_OPEN 8
#define IOC_NR_BUFFER_SIZE 9


// INT FOR MINOR DEVICE NUMBER
#define IOC_OPENREADCNT _IO(IOC_MY_MAGIC, IOC_NR_OPENREADCNT)
#define IOC_OPENWRITECNT _IO(IOC_MY_MAGIC, IOC_NR_OPENWRITE)
#define IOC_CLEAR _IO(IOC_MY_MAGIC, IOC_NR_CLEAR)
#define IOC_SET_READERS _IOW(IOC_MY_MAGIC, IOC_NR_READ_MAX_OPEN, int)
#define IOC_BUFFER_SIZE _IOW(IOC_MY_MAGIC, IOC_NR_BUFFER_SIZE, int);
#endif