/* required for loadable kernel modules*/
#include <linux/module.h>
#include <linux/init.h>
/* ****************************** */


#include <linux/fs.h>      /* The “filesystem” header is the header required for
			      writing device drivers. Many
			      important functions and data structures are
			      declared in here. (LDD3 p 89)

			      file_operations: it is a structure that is a
			      collection of function pointers. Each open file
			      (represented internally by a file
			      structure, which we will examine shortly) is
			      associated with its own set of functions
			      (by including a field called f_op that points to a
			      file_operations structure). The
			      operations are mostly in charge of implementing
			      the system calls and are therefore,
			      named open, read, and so on (LDD3 p 49)*/
#include <linux/types.h>   /* dev_t */
#include <linux/cdev.h>	   /* Functions for the management of cdev structures,
			      which represent char devices
			      within the kernel; char device registration */
#include <linux/fcntl.h>   /* flags (such as O_RDONLY,O_NONBLOCK, and O_SYNC) */
#include <linux/kernel.h>  /* printk, container_of MACRO */
#include <linux/slab.h>    /* memory management,allocation, etc.  of the "device" */


#include <asm/uaccess.h>   /* This include file declares functions used by
			      kernel code to move data to and
			      from user space.
			      Kernel supplied functions
			      [copy_to_user(read),copy_from_user(write)]  to access user-space
			      buffers (p82 LDD3) */

#include "scull.h"	   /* Local definitions */



MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Daniel Perez");
MODULE_DESCRIPTION("First scull module implemented");

static void scull_setup_cdev(struct scull_dev *dev, int index);
int scull_open(struct inode *inode, struct file *filp);
int scull_trim(struct scull_dev *dev);
ssize_t scull_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);
ssize_t scull_write(struct file *filp,const char __user *buf, size_t count, loff_t *f_pos);


/* fops struct declarations for scull */
struct file_operations scull_fops ={
	.owner   =  THIS_MODULE,
	.llseek  =  scull_llseek,
	.read    =  scull_read,
	.write   =  scull_write,
	.ioctl   =  scull_ioctl,
	.open    =  scull open,
	.release =  scull_release,
};

/* quantum set for scull 
 * used in scull_dev
 */
struct scull_qset {
	void **data;
	struct scull_qset *next;
};


/* Device Registration in Scull
 */
struct scull_dev{
	struct scull_qset *data;  /* Pointer to first quantum set */
	int quantum;		  /* the current quantum size */
	int qset;		  /* the current array size */
	unsigned long size;	  /* amount of data stored here */
	unsigned int access_key   /* used by sculluid and scullpriv */
	struct semaphore sem;	  /* mutual exclusion semaphore */
	struct cdev cdev;         /* Char device structure*/
};

/* 
 * Parameters that can be modified at load time
 */

int scull_quantum = SCULL_QUANTUM;
int scull_qset = SCULL_QSET;
int scull_major = SCULL_MAJOR;
int scull_minor = SCULL_MINOR;



/*
 * Module init function
 *
 */

int scull_init_module(void){
	if(scull_major){
		dev = MKDEV(scull_major,scull_minor);
		result = register_chrdev_region(dev, scull_nr_devs,"scull");
	}else{
		result = alloc_chrdev_region(&dev,scull_minor, scull_nr_devs,"scull");
		scull_major=MAJOR(dev);
	}
	if(result < 0){
		pr_warn("scull: can't get major %d\n",scull_major);
		return result;
	}
}


/*
 * Setup and initialize char_dev struct for scull 
 */
}
static void scull_setup_cdev(struct scull_dev *dev, int index){
	int err, devno = MKDEV(scull_major,scull_minor+index);  /*dev_t == u32 */
	cdev_init(&dev->cdev, &scull_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops   = &scull_fops;
	err = cdev_add(&dev->cdev, devno,1);

	/* Fail gracefully if need be */
	if(err)
		printk(KERN_NOTICE "Error %d adding scull%d", err, index);

}
/*
 * File Operations (scull_fops) open function 
 *
 * SIMPLIFIED VERSION OF scull_open
 */
int scull_open(struct inode *inode, struct file *filp){
	struct scull_dev *dev /* device information */

	dev = container_of(inode->i_cdev, struct scull_dev, cdev);
	/* container_of macro: A convenience macro that may be used to obtain a
	 * pointer to a structure from a
	 * pointer to some other structure contained within it.
	 *
	 * i.e. obtain a pointer to scull_dev dev structure from the pointer
	 * inode->i_cdev, which points to a structure of type cdev
	 */
	filp->private_data = dev; /*for other methods */

	/* now trim to 0 the length of the device if open was write-only */
	if ((filp->f_flags & O_ACCMODE) == O_WRONLY){
		scull_trim(dev); /* ignore errors  */
	}
	return 0;    /* success */
}

/*
 * File Operations (scull_fops) release (close) function
 *
 * Functions: *Deallocate anything that open allocated in filp->private_dataç
 *	      *Shut down the device on last close
 * The basic form of scull has no hardware to shut down, so the code required is
 * minimal
 */
int scull_release(struct inode *inode, struct file *filp){

	return 0;
}


/*
 * The function scull_trim is in charge of freeing the whole data
 * area and is invoked by scull_open when the file is opened for writing.
 *
 * shows in practice how struct scull_dev and struct scull_qset
 * are used to hold data
 */

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




ssize_t scull_read(struct file *filp, char __user *buf, size_t count, loff_t
		   *f_pos){

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

	/* follow the list up to the right position (defined elsewhere) */
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


ssize_t scull_write(struct file *filp,const char __user *buf, size_t count,
		    loff_t *f_pos){

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



*/





module_init(scull_init_module);
module_exit(scull_exit_module);
