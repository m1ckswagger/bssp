#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/printk.h>
#include <linux/moduleparam.h>

#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/semaphore.h>
#include <linux/device.h>
#include <linux/wait.h>
#include <linux/sched.h>

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>

#include "my_ioctl.h"

MODULE_LICENSE("Dual BSD/GPL");

#define DRVNAME KBUILD_MODNAME
#define BUFFER_SIZE 1024
#define MINOR_COUNT 5
#define MINOR_START 0	//start index
#define PROC_DIR_NAME "is151002"
#define PROC_FILE_NAME "info"
#define READ_TIME 1
#define WRITE_TIME 2

static int __init cdrv_init(void);
static void __exit cdrv_exit(void);

static void calc_time_diff(struct timespec *start_time, struct timespec *end_time, long *sec, long *nsec); 

static int mydev_open(struct inode *inode, struct file *filp);
static int mydev_release(struct inode *inode, struct file *filp);
static ssize_t mydev_read(struct file *filp, char __user *buff, size_t count, loff_t *offset);
static ssize_t mydev_write(struct file *filp, const char __user *buff, size_t count, loff_t *offset);
static long mydev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
// fuer proc interface
static int my_seq_file_open(struct inode *inode, struct file *filp);
static void *my_start(struct seq_file *sf, loff_t *pos);
static void *my_next(struct seq_file *sf, void *it, loff_t *pos);
static int my_show(struct seq_file *sf, void *it);
static void my_stop(struct seq_file *sf, void *it);


module_init(cdrv_init);
module_exit(cdrv_exit);

static struct file_operations mydev_fcalls = {
	.owner = THIS_MODULE,
	.open = mydev_open,
	.release = mydev_release,
	.read = mydev_read,
	.write = mydev_write,
	.unlocked_ioctl = mydev_ioctl
};

static struct file_operations my_proc_fcalls = {
	.owner = THIS_MODULE,
	.open = my_seq_file_open,		// eigenes open fuer /proc/is151002/info
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release
};

static struct seq_operations my_seq_ops = {
	.start = my_start,
	.next = my_next,
	.stop = my_stop,
	.show = my_show
};

// diese Struktur existiert pro device
typedef struct my_cdev {
	char *buffer;						// zeigt auf den buffer mit 1024 bytes
	int current_length;	
	int write_opened;
	int read_count;
	int open_syscall_count;
	int read_syscall_count;
	int write_syscall_count;
	int close_syscall_count;
	long time_read_sec;
	long time_read_nsec;
	long time_write_sec;
	long time_write_nsec;
	struct semaphore sync;	// zum sync der daten im device
	dev_t dev_num;					// entspricht u32 bzw uint32_t
	// wird vom kernel benoetigt
	struct cdev cdev;
} my_cdev_t;
static void add_syscall_time(my_cdev_t *dev, long sec, long nsec, int type); 

static struct class *my_class;
static my_cdev_t *my_devs;
static dev_t dev_num;		// entspricht u32 bzw uint32_t
static struct proc_dir_entry *proc_parent;	// fuer /proc/is151002
static struct proc_dir_entry *proc_entry;		// fuer /proc/is151002/info

static int __init cdrv_init(void)
{
	int result;
	int i;

	printk(KERN_INFO "cdrv: Hello, kernel world!\n");
	
	my_devs = kmalloc(sizeof(my_cdev_t) * MINOR_COUNT, GFP_KERNEL);
	if(!my_devs) {
		return -ENOMEM;
	}

	if((result = alloc_chrdev_region(&dev_num, MINOR_START, MINOR_COUNT, DRVNAME))) {
		kfree(my_devs);
		return result;
	}
	pr_info("Major %d, Minor %d\n", MAJOR(dev_num), MINOR(dev_num));
	
	// create a class (directory) in /sys/class
	my_class = class_create(THIS_MODULE, "my_driver_class");

	proc_parent = proc_mkdir(PROC_DIR_NAME, NULL);
	if (!proc_parent) {
		printk(KERN_ERR "create proc dir failed\n");
		goto proc_err_1;
	}
	proc_entry = proc_create_data(PROC_FILE_NAME, 0664, proc_parent, &my_proc_fcalls, NULL);
	if(!proc_entry) {
		goto proc_err_2;
	}

	for(i = 0; i < MINOR_COUNT; i++) {
		int err;

		// erstellt dev nummber fuer die einzelnen devices
		dev_t cur_devnr = MKDEV(MAJOR(dev_num), MINOR_START + i);
		
		// Zuweisung der file operations ans device
		cdev_init(&my_devs[i].cdev, &mydev_fcalls);
		my_devs[i].cdev.owner = THIS_MODULE;
		my_devs[i].dev_num = cur_devnr;
		my_devs[i].current_length = 0;
		my_devs[i].buffer = NULL;
		my_devs[i].write_opened = 0;
		my_devs[i].read_count = 0;
		my_devs[i].open_syscall_count = 0;
		my_devs[i].read_syscall_count = 0;
		my_devs[i].write_syscall_count = 0;
		my_devs[i].close_syscall_count = 0;
		my_devs[i].time_read_sec = 0;
		my_devs[i].time_read_nsec = 0;
		my_devs[i].time_write_sec = 0;
		my_devs[i].time_write_nsec = 0;

		// initialisieren des semaphors
		sema_init(&my_devs[i].sync, 1);

		if(device_create(my_class, NULL, cur_devnr, NULL, "mydev%d", MINOR(cur_devnr)) == (struct device *)ERR_PTR) {
			pr_warn("device creation failed!\n");
			// TODO cleanup
		}
		if((err = cdev_add(&my_devs[i].cdev, cur_devnr, 1)) < 0) {
			// TODO cleanup
			pr_warn("cdev_add failed...!\n");
		}
	}
	return 0;

	proc_err_2:
		remove_proc_entry(PROC_DIR_NAME, NULL);
	proc_err_1:
		class_destroy(my_class);
		unregister_chrdev_region(dev_num, MINOR_COUNT);
		kfree(my_devs);
		return -1;	// TODO correct makro for error

}

static void __exit cdrv_exit(void) {
	int i;

	// remove proc file and dir
	remove_proc_entry(PROC_FILE_NAME, proc_parent);
	remove_proc_entry(PROC_DIR_NAME, NULL);
		

	printk(KERN_INFO "cdrv: Goodbye, kernel world!\n");
	for(i = 0; i < MINOR_COUNT; i++) {
		device_destroy(my_class, my_devs[i].cdev.dev);
		cdev_del(&my_devs[i].cdev);
		if(my_devs[i].buffer) {
			kfree(my_devs[i].buffer);
		}
		printk("cleanup device %d\n", i);
	}
	
	class_destroy(my_class);
	unregister_chrdev_region(dev_num, MINOR_COUNT);
	kfree(my_devs);
	pr_info("cleanup my character driver %s\n", DRVNAME);	
}

static int mydev_open(struct inode *inode, struct file *filp) {
	
	// pointer auf die eigene struktur (my_cdev_t) wird mit containe_of berechnet
	my_cdev_t *dev = container_of(inode->i_cdev, my_cdev_t, cdev);
	
	// sperren des Semaphores
	if(down_interruptible(&dev->sync)) {
		return -ERESTARTSYS;
	}
	// damit spaeter bei read und write auf dev zugegriffen werden kann (weil spaeter 
	// inode nicht als parameter uebergeben werden kann)
	filp->private_data = dev;
	(dev->open_syscall_count)++;	
	if (filp->f_mode & FMODE_WRITE) {		// FMODE_WRITE --> mit schreiben geoeffnet
		
		// ueberpruefung ob gerade gelesen wird
		if (dev->write_opened != 0) {
			up(&dev->sync);
			return -EBUSY;
		}	
		

		// aufs device kann zugegriffen werden 
		dev->write_opened = 1;
		// zuweisen des speichers wenn dieser noch nicht vorhanden
		if (!dev->buffer) {
			dev->buffer = kmalloc(BUFFER_SIZE, GFP_KERNEL);
		}	
		pr_info("dev %d opened\n", MINOR(dev->dev_num));
		
	}
	else if (filp->f_mode & FMODE_READ) {		// FMODE_READ --> mit lesen geoeffnet
		if (!dev->buffer) {
			up(&dev->sync);		
			return -EPERM;
		}
		dev->read_count += 1;
	}
	// freigeben des Semaphores
	up(&dev->sync);
	return 0;
}

static int mydev_release(struct inode *inode, struct file *filp) {
	my_cdev_t *dev = filp->private_data;
	
	if (down_interruptible(&dev->sync)) {
		return -ERESTARTSYS;
	}
	
	(dev->close_syscall_count)++;	
	// zuruecksetzen des write_openeds
	dev->write_opened = 0;
	dev->read_count -= 1;
	pr_info("dev %d closed\n", MINOR(dev->dev_num));

	up(&dev->sync);
	
	return 0;
}

static ssize_t mydev_read(struct file *filp, char __user *buff, size_t count, loff_t *offset){
	my_cdev_t *dev = filp->private_data;
	int allowed_count;
	int to_copy;
	int not_copied;
	long sec;
	long nsec; 
	struct timespec start_time;
	struct timespec end_time;;

	start_time = current_kernel_time();
	// printk("time sec %ld, nsec %09ld\n", ts.tv_sec, ts.tv_nsec);

	if (down_interruptible(&dev->sync)) {
		return -ERESTARTSYS;
	}

	(dev->read_syscall_count)++;	
	allowed_count = dev->current_length;
	to_copy = (allowed_count < count) ? allowed_count : count;

	pr_info("dev %d to_copy: %d\n", MINOR(dev->dev_num), to_copy);
	pr_info("dev %d current_length: %d\n", MINOR(dev->dev_num), dev->current_length);
	pr_info("dev %d offset: %lld\n", MINOR(dev->dev_num), *offset);

	// ueberpruefen von eof
	if (*offset >= dev->current_length) {
		end_time = current_kernel_time();
		calc_time_diff(&start_time, &end_time, &sec, &nsec); 
		add_syscall_time(dev, sec, nsec, READ_TIME); 
		up(&dev->sync);
		return 0;
	}

	// copy_to_user liefert die anzahl der bytes, die nicht gelesen werden konnten
	not_copied = copy_to_user(buff, dev->buffer + *offset, to_copy);
	*offset = to_copy - not_copied;	// position nach dem read;
	end_time = current_kernel_time();
	calc_time_diff(&start_time, &end_time, &sec, &nsec); 
	add_syscall_time(dev, sec, nsec, READ_TIME); 

	up(&dev->sync);

	return *offset;
}

static ssize_t mydev_write(struct file *filp, const char __user *buff, size_t count, loff_t *offset) {
	my_cdev_t *dev = filp->private_data;
	int to_copy;
	int allowed_count;
	int not_copied;
	int length;	
	long sec;
	long nsec; 
	struct timespec start_time;
	struct timespec end_time;;

	start_time = current_kernel_time();

	if (down_interruptible(&dev->sync)) {
		return -ERESTARTSYS;
	}
	(dev->write_syscall_count)++;	

	// jump always to the end
	*offset = dev->current_length;
	allowed_count = BUFFER_SIZE - *offset;
	to_copy = (allowed_count < count) ? allowed_count : count;

	// copy_from_user liefert die anzahl der bytes, die nicht gelesen werden konnten
	not_copied = copy_from_user(dev->buffer + *offset, buff, to_copy);	
	length = *offset + (to_copy - not_copied);		// akt pos nach dem write (ohne lenght moeglich)

	if (dev->current_length < length) {
		dev->current_length = length;
	}

	*offset += (to_copy - not_copied);

	end_time = current_kernel_time();
	calc_time_diff(&start_time, &end_time, &sec, &nsec); 
	add_syscall_time(dev, sec, nsec, WRITE_TIME); 

	up(&dev->sync);	

	return to_copy - not_copied;
}
 
static long mydev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
	my_cdev_t *dev = filp->private_data;
	
	if (_IOC_TYPE(cmd) != IOC_MY_MAGIC) {
		return -1;
  }
	switch (_IOC_NR(cmd)) {
		case IOC_NR_OPENREADCNT:
			if (_IOC_DIR(cmd) != _IOC_NONE) {
				printk("Wrong direction. Must be \"no data transfer\"!\n");
				break;
			}
			if (down_interruptible(&dev->sync)) {
				return -ERESTARTSYS;
			}	
			printk("The device is currently opened %d times for read\n", dev->read_count);
			up(&dev->sync);
			break;			

		case IOC_NR_OPENWRITE:
			if (_IOC_DIR(cmd) != _IOC_NONE) {
				printk("Wrong direction. Must be \"no data transfer\"!\n");
				break;
			}
			if (down_interruptible(&dev->sync)) {
				return -ERESTARTSYS;
			}
			if (dev->write_opened) {
				printk("The device is currently opened for write\n");
			}
			else {
				printk("The device is currently not opened for write\n");
			}
			up(&dev->sync);
			break;


		case IOC_NR_CLEAR:
			if (_IOC_DIR(cmd) != _IOC_NONE) {
				printk("Wrong direction. Must be \"no data transfer\"!\n");
				break;
			}
			if (down_interruptible(&dev->sync)) {
				return -ERESTARTSYS;
			}
			dev->current_length = 0;
			up(&dev->sync);
			break;
		
		default:
			printk("ioctl: unknown operation\n");
	}
	return 0;
} 

static int my_seq_file_open(struct inode *inode, struct file *filp) {
	return seq_open(filp, &my_seq_ops);
}

static void *my_start(struct seq_file *sf, loff_t *pos) {
	pr_info("start, pos %ld\n", (long)*pos);
	if (*pos == 0) {
		return my_devs;
	}
	return NULL;
}

static void *my_next(struct seq_file *sf, void *it, loff_t *pos) {
	pr_info("next, pos before increase %ld\n", (long)*pos);
	(*pos)++;
	if (*pos >= MINOR_COUNT) {
		return NULL;
	}
	return my_devs + *pos; // gleich wie return &my_devs[*pos];
}


// it zeigt auf das, was bei start und next retourniert wird
// --> it zeigt auf akt. device
static int my_show(struct seq_file *sf, void *it) {
	my_cdev_t *dev = (my_cdev_t *)it;
	// index: 
	//	1) durch minor number
	//	2) durch Zeigerdifferenz
	int index  = dev - my_devs;
	pr_info("show for index %d\n", index);

	if (down_interruptible(&dev->sync)) {
		return -ERESTARTSYS;
	}
	seq_printf(sf, "STATUS OF mydev%d:\n", index + MINOR_START);
	seq_printf(sf, "  Open count: %d\n", dev->open_syscall_count);
	seq_printf(sf, "  Read count: %d\n", dev->read_syscall_count);
	seq_printf(sf, "  Write count: %d\n", dev->write_syscall_count);
	seq_printf(sf, "  Close count: %d\n", dev->close_syscall_count);
	seq_printf(sf, "  Current lenth: %d\n", dev->current_length);
	seq_printf(sf, "  Time reading: %ld.%09ld\n", dev->time_read_sec, dev->time_read_nsec);
	seq_printf(sf, "  Time writing: %ld.%09ld\n\n", dev->time_write_sec, dev->time_write_nsec);
	up(&dev->sync);	
	return 0;
}

static void my_stop(struct seq_file *sf, void *it) {
	pr_info("stop\n");
	// kann cleanup gemacht werden. bei uns nicht notwendig
}

static void calc_time_diff(struct timespec *start_time, struct timespec *end_time, long *sec, long *nsec) {
	*sec = end_time->tv_sec - start_time->tv_sec;
	*nsec = ((end_time->tv_nsec - start_time->tv_nsec) < 0) ? end_time->tv_nsec - start_time->tv_nsec + 1000000000 : end_time->tv_nsec - start_time->tv_nsec;
	if ((end_time->tv_nsec - start_time->tv_nsec) < 0) {
		(*sec)--;
	}
}

static void add_syscall_time(my_cdev_t *dev, long sec, long nsec, int type) {
	if (down_interruptible(&dev->sync)) {
		return;
	}
	switch (type) {
		case READ_TIME:
			dev->time_read_sec += sec;
			dev->time_read_nsec += nsec;
			if (dev->time_read_nsec > 1000000000) {
				dev->time_read_sec += 1;
				dev->time_read_nsec -= 1000000000;
			}			
			break;

		case WRITE_TIME:
			dev->time_write_sec += sec;
			dev->time_write_nsec += nsec;
			if (dev->time_write_nsec > 1000000000) {
				dev->time_write_sec += 1;
				dev->time_write_nsec -= 1000000000;
			}
			break;

		default:
			break;
	}
	up(&dev->sync);
}
