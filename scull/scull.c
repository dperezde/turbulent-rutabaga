/* required for loadable kernel modules*/
#include <linux/module.h>
#include <linux/init.h>
/* ****************************** */


#include <linux/fs.h>
#include <linux/types.h>
#include <linux/cdev.h>

#include <linux/uio.h>

#include "scull.h"


static void scull_setup_cdev(struct scull_dev *dev, int index){
	int err, devno = MKDEV(scull_major,scull_minor+index);   // DUDA: devno no debería ser un dev_t??

	cdev_init(&dev->cdev, &scull_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops   = &scull_fops;
	err = cdev_add(&dev->cdev, devno,1);

	/* Fail gracefully if need be */
	if(err){
	printk(KERN_NOTICE "Error %d adding scull%d", err, index);
	}
}

int scull_open(struct inode *inode, struct file *filp){
	struct scull_dev *dev /* device information */

	dev = container_of(inode->cdev, struct scull_dev, cdev);
	filp->private_data = dev; /*for other methods */

	/* now trim to 0 the length of the device if open waas write-only */
	if ((filp->f_flags & O_ACCMODE) == O_WRONLY){
		scull_trim(dev); /* ignore errors  */
	}
	return 0;    /* success */
}


int scull_trim(struct scull_dev *dev){
	struct scull_qset *next, *dptr;
	int qset = dev->qset; /* "dev" ist not-null */
	int i;

	for(dptr=dev->data ; dptr ; dptr=next){ /* all the list items */
		if(dptr->data){
			for (i=0; i< qset ; i++){
				kfree(dptr->data[i]);
			}
			kfree(dptr->data);
			dptr->data=NULL;
		}

		next =dptr->next;
		kfree(dptr);
	}

	dev->size = 0;
	dev->quantum = scull_quantum;
	dev->data = NULL;
	return 0;
}




ssize_t scull_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos){

	struct scull_dev *dev = filp->private_data;
	struct scull_qset *dptr;	/*the first listitem */
	int quantum = dev->quantum, qset = dev->qset;
	int itemsize= quantum * qset; /*how many bytes is the listitem */
	int item, s_pos, q_pos, rest;
	ssize_t retval = 0;

	if(down_interruptible(&dev->sem)){
		return -ERESTARTSYS;
	}
	if(*f_pos >= dev->size){
		goto out;
	}
	if (*f_pos + count > dev->size){
		count = dev->size - *f_pos;
	}

	/* find listitem, qset index, and offset in the quantum */
	item = (long)*f_pos / itemsize;
	rest = (long)*f_pos % itemsize;
	s_pos = rest / quantum;
	q_pos = rest % quantum;

	/* follow the list up tyo the right position (defined elsewhere) */
	dptr = scull_follow(dev,item);

	if(dptr == NULL || ! dptr->data || ! dptr->data[s_pos]){
		goto out; /* don't fill holes */
	}

	/* read only up to the end of this quantum */
	if(count > quantum - q_pos){
		count = quantum - q_pos;
	}

	if(copy_to_user(buf,dptr->data[s_pos] + q_pos, count)){
		retval = -EFAULT;
		goto out;
	}
	*f_pos += count;
	retval = count;

	out:
	  up(&dev->sem);
	  return retval;
}


ssize_t scull_write(struct file *filp,const char __user *buf, size_t count, loff_t *f_pos){

	struct scull_dev *dev = filp->private_data;
	struct scull_qset *dptr;
	int quantum = dev->quantum, qset = dev->qset;
	int itemsize = quantum * qset;
	int iem, s_pos, q_pos, rest;
	ssize_t retval = -ENOMEM; /* value used in "goto out" statements */

	if (down_interruptible(&dev->mem)){
		return -ERESTARTSYS;
	}

	/* find listitem, qset index and offset in the quantum */
	item = (long)*f_pos / itemsize;
	rest = (long)*f_pos % itemsize;
	s_pos = rest / quantum;
	q_pos = rest % quantum;

	/* follow the list up to the right position */
	dptr = scull_follow(dev,item);
	if (dptr == NULL){
		goto out;
	}
	if (! dptr->data){
		dptr->data = kmalloc(qset * sizeof(char *),GFP_KERNEL);
		if(!dptr->data){
			goto out;
		}
		memset(dptr->data, 0, qset * sizeof(char *));
	}

	if(!dptr->data[s_pos]){
		dptr->data[s_pos] = kmalloc(quantum, GFP_KERNEL);
		if(!dptr->data[s_pos]){
			goto out;
		}
	}

	/* write only up to the end of this quantum */
	if(count > quantum - q_pos){
		count = quantum - q_pos;
	}
	if(copy_from_user(dptr->data[s_pos]+q_pos,buf,count)){
		retval = -EFAULT;
		goto out;
	}
	*f_pos += count;
	retval = count;

	/*update the size */
	if(dev->size < *f_pos){
		dev->size = *f_pos;
	}

	out:
	  up(&dev->sem);
	  return retval;
}

/*

if(scull_major){
	dev = MKDEV(scull_major,scull_minor);
	result = register_chrdev_region(dev, scull_nr_devs,"scull");
}else{
	result = alloc_chrdev_region(&dev,scull_minor, scull_nr_devs,"scull");
	scull_major=MAJOR(dev);
}
if(result < 0){
	printk(KERN_WARNING "scull: can't get major %d\n",scull_major);
	return result;
}

*/



MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Daniel Perez");
MODULE_DESCRIPTION("First scull module implemented");


