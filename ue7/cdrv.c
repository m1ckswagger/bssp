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
#include <linux/seq_file.h>

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>

#include <linux/jiffies.h>
#include <linux/time.h>
#include <linux/delay.h>

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

#define MIN_VALUE_OF(a,b) ( (a) > (b) ? (b) : (a))


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
	size_t current_length;	
	int write_opened;
	long reader_count;
	unsigned long allowed_max_readers;

	int open_syscall_count;
	int read_syscall_count;
	int write_syscall_count;
	int close_syscall_count;
	int clear_count;
	
	long time_read_sec;
	long time_read_nsec;
	long time_write_sec;
	long time_write_nsec;

	wait_queue_head_t rwq;
	wait_queue_head_t ioctl_cleared;	
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

	pr_info("cdrv: Hello, kernel world!\n");
	
	my_devs = kmalloc(sizeof(my_cdev_t) * MINOR_COUNT, GFP_KERNEL);
	if(!my_devs) {
		return -ENOMEM;
	}

	if((result = alloc_chrdev_region(&dev_num, MINOR_START, MINOR_COUNT, DRVNAME))) {
		kfree(my_devs);
		return result;
	}
	pr_info("cdrv: Major %d, Minor %d\n", MAJOR(dev_num), MINOR(dev_num));
	
	// create a class (directory) in /sys/class
	my_class = class_create(THIS_MODULE, "my_driver_class");

	proc_parent = proc_mkdir(PROC_DIR_NAME, NULL);
	if (!proc_parent) {
		pr_devel(KERN_ERR "cdrv: create proc dir failed\n");
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
		my_devs[i].reader_count = 0;
		my_devs[i].allowed_max_readers = 0;

		my_devs[i].open_syscall_count = 0;
		my_devs[i].read_syscall_count = 0;
		my_devs[i].write_syscall_count = 0;
		my_devs[i].close_syscall_count = 0;
		my_devs[i].clear_count = 0;
		
		my_devs[i].time_read_sec = 0;
		my_devs[i].time_read_nsec = 0;
		my_devs[i].time_write_sec = 0;
		my_devs[i].time_write_nsec = 0;

		// initialisieren des semaphors und wait queues
		sema_init(&my_devs[i].sync, 1);
		init_waitqueue_head(&my_devs[i].rwq);
		init_waitqueue_head(&my_devs[i].ioctl_cleared);

		if(device_create(my_class, NULL, cur_devnr, NULL, "mydev%d", MINOR(cur_devnr)) == (struct device *)ERR_PTR) {
			pr_warn("cdrv: device creation failed!\n");
			// TODO cleanup
		}
		if((err = cdev_add(&my_devs[i].cdev, cur_devnr, 1)) < 0) {
			// TODO cleanup
			pr_warn("cdrv: cdev_add failed...!\n");
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
		

	pr_info("cdrv: Goodbye, kernel world!\n");
	for(i = 0; i < MINOR_COUNT; i++) {
		device_destroy(my_class, my_devs[i].cdev.dev);
		cdev_del(&my_devs[i].cdev);
		if(my_devs[i].buffer) {
			kfree(my_devs[i].buffer);
		}
		pr_devel("cdrv: cleanup device %d\n", i);
	}
	
	class_destroy(my_class);
	unregister_chrdev_region(dev_num, MINOR_COUNT);
	kfree(my_devs);
	pr_info("cdrv: cleanup my character driver %s\n", DRVNAME);	
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
		pr_info("cdrv: mydev%d opened\n", MINOR(dev->dev_num));
		
	}
	else if (filp->f_mode & FMODE_READ) {		// FMODE_READ --> mit lesen geoeffnet
		if (!dev->buffer) {
			pr_info("cdrv: open: device must be opened for write before you can read\n");
			up(&dev->sync);		
			return -ENOSR;
		}
		if (dev->allowed_max_readers > 0 && dev->reader_count >= dev->allowed_max_readers) {
			pr_devel("cdrv: open: allowed readers are %lu but there are %ld\n", dev->allowed_max_readers, dev->reader_count+1);
			up(&dev->sync);
			return -EBUSY;
		}
		dev->reader_count += 1;
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
	if (filp->f_mode & FMODE_WRITE) {
		dev->write_opened = 0;
	}
	if (filp->f_mode & FMODE_READ) {
		dev->reader_count -= 1;
	}
	pr_info("cdrv: mydev%d closed\n", MINOR(dev->dev_num));

	up(&dev->sync);
	
	return 0;
}

static ssize_t mydev_read(struct file *filp, char __user *buff, size_t count, loff_t *offset){
	my_cdev_t *dev = filp->private_data;
	size_t to_read;
	unsigned long not_copied;
	size_t rdbytes = 0;
	
	long sec;
	long nsec; 
	struct timespec start_time;
	struct timespec end_time;;

	start_time = current_kernel_time();
	// pr_devel("time sec %ld, nsec %09ld\n", ts.tv_sec, ts.tv_nsec);

	if (down_interruptible(&dev->sync)) {
		return -ERESTARTSYS;
	}
	(dev->read_syscall_count)++;	

	if (buff == NULL) {
		pr_devel("cdrv: read: got NULL buffer\n");
		end_time = current_kernel_time();
		calc_time_diff(&start_time, &end_time, &sec, &nsec); 
		add_syscall_time(dev, sec, nsec, READ_TIME); 
		up(&dev->sync);	
		return -EINVAL;
	}
	else if (count == 0) {
		pr_devel("cdrv: read: nothing requested\n");
		end_time = current_kernel_time();
		calc_time_diff(&start_time, &end_time, &sec, &nsec); 
		add_syscall_time(dev, sec, nsec, READ_TIME); 
		up(&dev->sync);	
		return 0;
	}
	else if (*offset > BUFFER_SIZE) {
		pr_devel("cdrv: read: given offset is out of buffer range\n");
		end_time = current_kernel_time();
		calc_time_diff(&start_time, &end_time, &sec, &nsec); 
		add_syscall_time(dev, sec, nsec, READ_TIME); 
		up(&dev->sync);	
		return -ESPIPE;
	}
	else if (*offset == BUFFER_SIZE) {
		pr_devel("cdrv: read: given offset is exactly buffersize (%lld)\n", *offset);
		end_time = current_kernel_time();
		calc_time_diff(&start_time, &end_time, &sec, &nsec); 
		add_syscall_time(dev, sec, nsec, READ_TIME); 
		up(&dev->sync);	
		return 0;
	}

	// kuerzen von count so dass max bis zum ende gelesen werden kann
	if (*offset + count > BUFFER_SIZE) {
		for (; *offset + count > BUFFER_SIZE; count -= (*offset + count) - BUFFER_SIZE) 
			;	
		pr_devel("cdrv: read: request way too much - reduced count to %zd\n", count);
		if (count == 0) {
			pr_devel("cdrv: read: due to reduction nothing requested\n");
			end_time = current_kernel_time();
			calc_time_diff(&start_time, &end_time, &sec, &nsec); 
			add_syscall_time(dev, sec, nsec, READ_TIME); 
			up(&dev->sync);	
			return 0;	
		}	
	}
	pr_devel("cdrv: read: FMODE_NDLAY: %d, may not block %d, O_NDELAY: %d, O_NONBLOCK: %d\n", filp->f_flags & FMODE_NDELAY, filp->f_flags & MAY_NOT_BLOCK, filp->f_flags & O_NDELAY, filp->f_flags & O_NONBLOCK);
	up(&dev->sync);
	
	do {
		if (down_interruptible(&dev->sync)) {
			return -ERESTARTSYS;
		}
		pr_devel("cdrv: read: already read: %zd, count=%zd\n", rdbytes, count);
		if (*offset >= dev->current_length) {
			pr_devel("cdrv: read: open write-processes: %d\n", dev->write_opened);
			if (filp->f_flags & O_NONBLOCK || dev->write_opened == 0) {
				end_time = current_kernel_time();
				calc_time_diff(&start_time, &end_time, &sec, &nsec); 
				add_syscall_time(dev, sec, nsec, READ_TIME); 
				up(&dev->sync);	
				return rdbytes;
			}
			else {
				pr_devel("cdrv: read: now at offset %lld and nothing more in buffer\n", *offset);
				pr_devel("cdrv: read: number of writers was: %d\n", dev->write_opened);
				up(&dev->sync);
				end_time = current_kernel_time();
				pr_devel("cdrv: read: waiting until file gets modified %ld.%09ld\n", end_time.tv_sec, end_time.tv_nsec);
				wait_event_interruptible(dev->rwq, (*offset < dev->current_length || dev->write_opened == 0));
				pr_devel("cdrv: read: woke up\n");
				end_time = current_kernel_time();
				pr_info("cdrv: read: file was modified  %ld.%09ld\n", end_time.tv_sec, end_time.tv_nsec);
				pr_devel("cdrv: read: offset: %lld current_length: %zd\n", *offset, dev->current_length);
				continue;
			}
		}
		to_read = MIN_VALUE_OF(count-rdbytes, dev->current_length - *offset);
		pr_devel("cdrv: read: read %zd bytes\n", to_read);
		not_copied = copy_to_user(buff, (dev->buffer + *offset), (unsigned long) to_read);
		if (not_copied != 0) {
			pr_devel("cdrv: read: count not read %lu bytes\n", not_copied);
			to_read -= not_copied;
		}
		*offset += to_read;
		rdbytes += to_read;
		up(&dev->sync);
	} while (rdbytes < count);
		
	mdelay(100L); // dealy 100 millisecs
	end_time = current_kernel_time();
	if (down_interruptible(&dev->sync)) {
		return -ERESTARTSYS;
	}
	calc_time_diff(&start_time, &end_time, &sec, &nsec); 
	add_syscall_time(dev, sec, nsec, READ_TIME); 

	up(&dev->sync);

	return rdbytes;
}

static ssize_t mydev_write(struct file *filp, const char __user *buff, size_t count, loff_t *offset) {
	my_cdev_t *dev = filp->private_data;
	size_t to_copy;
	size_t final_position = 0;
	size_t copied_total = 0;
	unsigned long not_copied;

	long sec;
	long nsec; 
	struct timespec start_time;
	struct timespec end_time;;

	start_time = current_kernel_time();

	pr_devel("cdrv: write: requested count: %lu, offset: %lld\n", count, *offset);
	if (down_interruptible(&dev->sync)) {
		return -ERESTARTSYS;
	}
	(dev->write_syscall_count)++;	

	if (buff == NULL) {
		pr_devel("cdrv: write: got NULL buffer\n");
		end_time = current_kernel_time();
		calc_time_diff(&start_time, &end_time, &sec, &nsec); 
		add_syscall_time(dev, sec, nsec, WRITE_TIME); 
		up(&dev->sync);
		return -EINVAL;
	}
	else if (*offset > BUFFER_SIZE) {
		pr_devel("cdrv: write: given offset %lld was out of buffer range (%d)\n", *offset, BUFFER_SIZE);
		end_time = current_kernel_time();
		calc_time_diff(&start_time, &end_time, &sec, &nsec); 
		add_syscall_time(dev, sec, nsec, WRITE_TIME); 
		up(&dev->sync);
		return -ESPIPE; 	
	}
	else if (*offset == BUFFER_SIZE && (filp->f_flags & O_NONBLOCK)) {
		pr_devel("cdrv: write: offset was exactly buffersize (%lld)\n", *offset);
		end_time = current_kernel_time();
		calc_time_diff(&start_time, &end_time, &sec, &nsec); 
		add_syscall_time(dev, sec, nsec, WRITE_TIME); 
		up(&dev->sync);
		return 0;
	}
	else if (count == 0) {
		pr_devel("cdrv: write: user requested 0 bytes\n");
		end_time = current_kernel_time();
		calc_time_diff(&start_time, &end_time, &sec, &nsec); 
		add_syscall_time(dev, sec, nsec, WRITE_TIME); 
		up(&dev->sync);
		return 0;
	}
	up(&dev->sync);

	mdelay(100L); // dealy 100 millisecs
	
	while (count > 0) {
		if (*offset >= BUFFER_SIZE) {
			if (filp->f_flags & O_NONBLOCK) {
				if (down_interruptible(&dev->sync)) {
					return -ERESTARTSYS;
				}	
				end_time = current_kernel_time();
				calc_time_diff(&start_time, &end_time, &sec, &nsec); 
				add_syscall_time(dev, sec, nsec, WRITE_TIME); 
				up(&dev->sync);	
				return copied_total;
			}
			else {
				pr_devel("cdrv: write: wait for ioctl\n");
				end_time = current_kernel_time();
				pr_info("cdrv: write: waiting started until the buffer get flushed %ld.%09ld\n", end_time.tv_sec, end_time.tv_nsec);
				wait_event_interruptible(dev->ioctl_cleared, (dev->current_length != BUFFER_SIZE));
				pr_devel("cdrv: write: ioctl done\n");
				end_time = current_kernel_time();
				pr_info("cdrv: write: waiting finished the buffer was flushed %ld.%09ld\n", end_time.tv_sec, end_time.tv_nsec);
				*offset = 0;
			}
		}
			
		if (down_interruptible(&dev->sync)) {
			return -ERESTARTSYS;
		}	
		to_copy = MIN_VALUE_OF(count, (BUFFER_SIZE - *offset));
		pr_devel("cdrv: write: copy %zd bytes\n", to_copy);

		not_copied = copy_from_user(dev->buffer + *offset, buff+copied_total, to_copy);
		if (not_copied != 0) {
			pr_devel("cdrv: write: could not copy %lu bytes\n", not_copied);
			to_copy -= not_copied;
		}
		
		final_position = *offset + to_copy;
		if (dev->current_length < final_position) {
			dev->current_length = final_position;
		}
		*offset += to_copy;
		
		count -= to_copy;
		up(&dev->sync);
		copied_total += to_copy;
	}
	
	if (to_copy > 0) {
		pr_devel("cdrv: write: waking up clients\n");
		wake_up_all(&dev->rwq);
		pr_devel("cdrv: write: woke up the clients\n");
	}
	end_time = current_kernel_time();
	if (down_interruptible(&dev->sync)) {
		return -ERESTARTSYS;
	}	
	calc_time_diff(&start_time, &end_time, &sec, &nsec); 
	add_syscall_time(dev, sec, nsec, WRITE_TIME); 

	up(&dev->sync);	

	return copied_total;
}
 
static long mydev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
	my_cdev_t *dev = filp->private_data;
	pr_devel("cdrv: ioctl: command %u\n", cmd);

	if (_IOC_TYPE(cmd) != IOC_MY_MAGIC) {
		pr_info("cdrv: ioctl: wrong magic number! was %u instead of %d\n", _IOC_TYPE(cmd), IOC_MY_MAGIC);
		return -EINVAL;
  }
	switch (_IOC_NR(cmd)) {
		case IOC_NR_OPENREADCNT:
			if (_IOC_DIR(cmd) != _IOC_NONE) {
				pr_devel("cdrv: ioctl: wrong direction. must be \"no data transfer\"!\n");
				break;
			}
			if (down_interruptible(&dev->sync)) {
				return -ERESTARTSYS;
			}	
			pr_info("cdrv: ioctl: the device is currently opened %ld times for read\n", dev->reader_count);
			up(&dev->sync);
			return dev->reader_count;			

		case IOC_NR_OPENWRITE:
			if (_IOC_DIR(cmd) != _IOC_NONE) {
				pr_devel("cdrv: ioctl: wrong direction. must be \"no data transfer\"!\n");
				break;
			}
			if (down_interruptible(&dev->sync)) {
				return -ERESTARTSYS;
			}
			if (dev->write_opened) {
				pr_info("cdrv: ioctl: the device is currently opened for write\n");
			}
			else {
				pr_info("cdrv: ioctl: the device is currently not opened for write\n");
			}
			up(&dev->sync);
			return dev->write_opened;


		case IOC_NR_CLEAR:
			if (_IOC_DIR(cmd) != _IOC_NONE) {
				pr_devel("cdrv: ioctl: wrong direction. must be \"no data transfer\"!\n");
				break;
			}
			if (down_interruptible(&dev->sync)) {
				return -ERESTARTSYS;
			}
			pr_info("cdrv: ioctl: resetting device\n");
			dev->current_length = 0;
			(dev->clear_count)++;
			wake_up_all(&dev->ioctl_cleared);
			pr_devel("cdrv: ioctl: completed clear\n");
			up(&dev->sync);
			return 0;
		
		case IOC_NR_READ_MAX_OPEN:
			if (_IOC_DIR(cmd) != _IOC_WRITE) {
				pr_devel("cdrv: ioctl: wrong direction for ioctl with IOC_IGNORE_MODE\n");
				break;
			}
			if (down_interruptible(&dev->sync)) {
				return -ERESTARTSYS;
			}
			dev->allowed_max_readers = arg;
			pr_devel("cdrv: limit for open reads set to %ld\n", arg);
			
		default:
			pr_info("cdrv: ioctl: unknown command\n");
			return -ENOIOCTLCMD;
	}
	return -EINVAL;
} 

static int my_seq_file_open(struct inode *inode, struct file *filp) {
	return seq_open(filp, &my_seq_ops);
}

static void *my_start(struct seq_file *sf, loff_t *pos) {
	pr_info("cdrv: start, pos %ld\n", (long)*pos);
	if (*pos == 0) {
		return my_devs;
	}
	return NULL;
}

static void *my_next(struct seq_file *sf, void *it, loff_t *pos) {
	pr_info("    next, pos before increase %ld\n", (long)*pos);
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
	//pr_info("cdrv: show for index %d\n", index);

	if (down_interruptible(&dev->sync)) {
		return -ERESTARTSYS;
	}
	seq_printf(sf, "cdrv: STATUS OF mydev%d:\n", index + MINOR_START);
	seq_printf(sf, "   Open count: %d\n", dev->open_syscall_count);
	seq_printf(sf, "   Read count: %d\n", dev->read_syscall_count);
	seq_printf(sf, "   Write count: %d\n", dev->write_syscall_count);
	seq_printf(sf, "   Close count: %d\n", dev->close_syscall_count);
	seq_printf(sf, "   Current lenth: %ld\n", dev->current_length);
	seq_printf(sf, "   Time reading: %ld.%09ld\n", dev->time_read_sec, dev->time_read_nsec);
	seq_printf(sf, "   Time writing: %ld.%09ld\n\n", dev->time_write_sec, dev->time_write_nsec);
	up(&dev->sync);	
	return 0;
}

static void my_stop(struct seq_file *sf, void *it) {
	pr_info("  stop\n");
	// kann cleanup gemacht werden. bei uns nicht notwendig
}

static void calc_time_diff(struct timespec *start_time, struct timespec *end_time, long *sec, long *nsec) {
	long nsec_diff;
	pr_devel("cdrv: calc_time_diff: entered\n");
	pr_devel("   Time start: %ld.%09ld\n", start_time->tv_sec, start_time->tv_nsec);
	pr_devel("   Time end  : %ld.%09ld\n", end_time->tv_sec, end_time->tv_nsec);
	*sec = end_time->tv_sec - start_time->tv_sec;
	nsec_diff = end_time->tv_nsec - start_time->tv_nsec;
	if (nsec_diff < 0) {
		*nsec = nsec_diff + 1000000000;
		(*sec)++;
	}
	else {
		*nsec = nsec_diff;
	}  
	pr_devel("   Difference: %ld.%09ld\n", *sec, *nsec);
}

static void add_syscall_time(my_cdev_t *dev, long sec, long nsec, int type) {
	int index = dev - my_devs;
	pr_devel("cdrv: add_syscall_time: entered\n");	
	switch (type) {
		case READ_TIME:
			dev->time_read_sec += sec;
			dev->time_read_nsec += nsec;
			if (dev->time_read_nsec > 1000000000) {
				dev->time_read_sec += 1;
				dev->time_read_nsec -= 1000000000;
			}
			pr_devel("cdrv: add_syscall_time: calculating read time for mydev%d\n", index + MINOR_START);			
			break;

		case WRITE_TIME:
			dev->time_write_sec += sec;
			dev->time_write_nsec += nsec;
			if (dev->time_write_nsec > 1000000000) {
				dev->time_write_sec += 1;
				dev->time_write_nsec -= 1000000000;
			}
			pr_devel("cdrv: add_syscall_time: calculating write time for mydev%d\n", index + MINOR_START);			
			break;

		default:
			break;
	}
}
