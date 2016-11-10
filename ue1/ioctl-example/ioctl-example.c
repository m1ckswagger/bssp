
static long mydev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct my_file_extension *my_file_ext = filp->private_data;
	struct my_cdev_t *dev = my_file_ext->dev;
	long rv = 0; // return value

	if (_IOC_TYPE(cmd) != IOC_MY_MAGIC) {
		return -1;
	}

	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	// ACHTUNG: Sobald auf das Device zugegriffen wird, muss gesynct werden!!!!
	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

	switch (_IOC_NR(cmd)) {
		case IOC_NR_OPENREADCNT:
			if (_IOC_DIR(cmd) != _IOC_NONE) {
				// wrong direction. Must be "no data transfer" (because arg is not used)
				// ...
				break;
			}
			// TODO set rv to readcount from device
			break;
		case IOC_NR_SET_IGNORE_MODE:
		{
			int ignore_mode;
			if (_IOC_DIR(cmd) != _IOC_WRITE) {
				// wrong direction. Must be "writing to the device"
				printk(KERN_ERR "WRONG direction for ioctl with IOC_IGNORE_MODE\n");
				// ...
				break;
			}
			// access_ok() muss hier NICHT verwendet werden.
			// Wird nur benoetigt, wenn ein Puffer per arg uebergeben wird. (Also
			// wenn arg als Zeiger verwendet wird. Ist aber hier nicht der Fall.
			// es wurde einfach nur ein Integerwert uebergeben.)
			ignore_mode = arg;
			printk("ioctl from user space use the value %d\n", ignore_mode);
			// ....
			break;
		}
		default:
			// ...
	}

	return rv;
}

