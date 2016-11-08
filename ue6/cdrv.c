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

MODULE_LICENSE("Dual BSD/GPL");

#define DRVNAME KBUILD_MODNAME
#define BUFFER_SIZE 1024
#define MINOR_COUNT 5
#define MINOR_START 0	//start index

static int __init cdrv_init(void);
static void __exit cdrv_exit(void);

static int mydev_open(struct inode *inode, struct file *filp);
static int mydev_release(struct inode *inode, struct file *filp);
static ssize_t mydev_read(struct file *filp, char __user *buff, size_t count, loff_t *offset);
static ssize_t mydev_write(struct file *filp, const char __user *buff, size_t count, loff_t *offset);
static long mydev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);


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

// diese Struktur existiert pro device
typedef struct my_cdev {
	char *buffer;						// zeigt auf den buffer mit 1024 bytes
	int current_length;	
	int write_counter;
	struct semaphore sync;	// zum sync der daten im device
	dev_t dev_num;					// entspricht u32 bzw uint32_t
	// wird vom kernel benoetigt
	struct cdev cdev;
} my_cdev_t;

static struct class *my_class;
static my_cdev_t *my_devs;
static dev_t dev_num;		// entspricht u32 bzw uint32_t


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
		my_devs[i].write_counter = 0;

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
}

static void __exit cdrv_exit(void) {
	int i;

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
	
	// damit spaeter bei read und write auf dev zugegriffen werden kann (weil spaeter 
	// inode nicht als parameter uebergeben werden kann)
	filp->private_data = dev;
	if (filp->f_mode & FMODE_READ) {		// FMODE_READ --> mit lesen geoeffnet
		if (!dev->buffer) {
			return -EPERM;
		}
	}
	else if (filp->f_mode & FMODE_WRITE) {		// FMODE_WRITE --> mit schreiben geoeffnet
		
		// ueberpruefung ob gerade gelesen wird
		if (dev->write_counter != 0) {
			return -EBUSY;
		}	
		// sperren des Semaphores
		if(down_interruptible(&dev->sync)) {
			return -ERESTARTSYS;
		}

		// aufs device kann zugegriffen werden --- start
		dev->write_counter = 1;
		// zuweisen des speichers wenn dieser noch nicht vorhanden
		if (!dev->buffer) {
			dev->buffer = kmalloc(BUFFER_SIZE, GFP_KERNEL);
		}	
		pr_info("dev %d opened\n", MINOR(dev->dev_num));
		
		// freigeben des Semaphores
		up(&dev->sync);
	}
	return 0;
}

static int mydev_release(struct inode *inode, struct file *filp) {
	my_cdev_t *dev = filp->private_data;
	
	if (down_interruptible(&dev->sync)) {
		return -ERESTARTSYS;
	}
	
	// zuruecksetzen des write_counters
	dev->write_counter = 0;
	pr_info("dev %d closed\n", MINOR(dev->dev_num));

	up(&dev->sync);
	
	return 0;
}

static ssize_t mydev_read(struct file *filp, char __user *buff, size_t count, loff_t *offset){
	my_cdev_t *dev = filp->private_data;
	int allowed_count;
	int to_copy;
	int not_copied;

	if (down_interruptible(&dev->sync)) {
		return -ERESTARTSYS;
	}

	allowed_count = dev->current_length;
	to_copy = (allowed_count < count) ? allowed_count : count;

	pr_info("dev %d to_copy: %d\n", MINOR(dev->dev_num), to_copy);
	pr_info("dev %d current_length: %d\n", MINOR(dev->dev_num), dev->current_length);
	pr_info("dev %d offset: %lld\n", MINOR(dev->dev_num), *offset);

	// ueberpruefen von eof
	if (*offset >= dev->current_length) {
		up(&dev->sync);
		return 0;
	}

	// copy_to_user liefert die anzahl der bytes, die nicht gelesen werden konnten
	not_copied = copy_to_user(buff, dev->buffer + *offset, to_copy);
	*offset = to_copy - not_copied;	// position nach dem read;
	
	up(&dev->sync);

	return *offset;
}

static ssize_t mydev_write(struct file *filp, const char __user *buff, size_t count, loff_t *offset) {
	my_cdev_t *dev = filp->private_data;
	int to_copy;
	int allowed_count;
	int not_copied;
	int length;	

	if (down_interruptible(&dev->sync)) {
		return -ERESTARTSYS;
	}

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

	up(&dev->sync);	

	return to_copy - not_copied;
}
 
static long mydev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
	pr_info("TODO ioctl\n");
	//my_cdev_t *dev = filp->private_data;
	return 0;
} 


