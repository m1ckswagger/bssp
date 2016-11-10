#ifndef MY_IOCTL_H
#define MY_IOCTL_H

// see: http://git.kernel.org/cgit/linux/kernel/git/stable/linux-stable.git/plain/Documentation/ioctl/ioctl-number.txt?id=HEAD
#define IOC_MY_MAGIC 0xCE // freie Nummer laut link (oberhalb)

#define IOC_NR_OPENREADCNT 5
#define IOC_NR_OPENWRITECNT 6
#define IOC_NR_CLEAR 7
#define IOC_NR_SET_IGNORE_MODE 8

// makro erklaerung siehe: Linux Device Drivers (eCampus pdf Buch) S.138 (pdf 156)

#define IOC_OPENREADCNT _IO(IOC_MY_MAGIC, IOC_NR_OPENREADCNT)
// Abfragen des read counts per im Usermode:
// int open_read_cnt = ioctl(fd, IOC_OPENREADCNT);
// Testprogramm muss natuerlich die my_ioctl.h inkludieren.

// TODO define IOC_OPENWRITECNT
// TODO define IOC_CLEAR

/*
Hier noch ein fiktives Beispiel fuer die Verwendung des optionalen
Arguments vom ioctl Aufruf.
int als dritter Parameter im _IOW Makro gibt an, dass ein Integer bei
der Verwendung von ioctl als dritter Parameter verwendet werden muss.
Dass ein dritter Parameter bei ioctl verwendet werden muss,
kann durch die Makros _IOW, _IOR und _IORW bestimmt werden. _IO hingegen
bedeutet, dass kein dritter Parameter verwendet werden soll (siehe IOC_OPENREADCNT).
Fiktives Beispiel (nur zur Erklaerung. Muss euer Treiber nicht unterstuetzen!!!!):
Bsp: Wenn der Wert 1 uebergeben wird, soll das O_NDELAY Flag vom Device ignoriert
werden, egal ob es beim Oeffnen angegeben wurde oder nicht.
Wenn der Wert 0 verwendet wird, soll das Flag (falls vorhanden) wieder richtig
interpretiert werden. Somit kann wie folgt der gewuenschte Modus gesetzt werden:
*/
// Setzen des Ignore Modes im Usermode:
// ioctl(fd, IOC_SET_IGNORE_MODE, 1);
// Reseten des Ignore Modes im Usermode:
// ioctl(fd, IOC_SET_IGNORE_MODE, 0);
#define IOC_SET_IGNORE_MODE _IOW(IOC_MY_MAGIC, IOC_NR_SET_IGNORE_MODE, int)
// Dieses Beispiel zeigt, wie der Wert 1 oder 0 per ioctl uebergeben wurden.
// Natuerlich kann jeder beliebige Integerwert so per ioctl uebergeben werden.

#endif

