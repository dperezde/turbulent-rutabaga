ssize_t (*readv) (struct file *filp, const struct iovec *iov, unsigned long count, loff_t *ppos);
ssize_t (*writev) (struct file *filp, const struct iovec *iov, unsigned long count, loff_t *ppos);

struct file_operations scull_ops ={
	.owner   =  THIS_MODULE,
	.llseek  =  scull_llseek,
	.read    =  scull_read,
	.write   =  scull_write,
	.ioctl   =  scull_ioctl,
	.open    =  scull open,
	.release =  scull_release,
};


struct scull_qset {
	void **data;
	struct scull_qset *next;
};

struct scull_dev{
	struct scull_qset *data;  /* Pointer to first quantum set */
	int quantum;		  /* the current quantum size */
	int qset;		  /* the current array size */
	unsigned long size;	  /* amount of data stored here */
	unsigned int access_key   /* used by sculluid and scullpriv */
	struct semaphore sem;	  /* mutual exclusion semaphore */
	struct cdev cdev;         /* Char device structure*/
};







ssize_t read(struct file *filp, char __user *buff, size_t count ,loff_t *offp);

ssize_t write(struct file *filp, const char __user *buff, size_t count, loff_t *offp);

