#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/moduleparam.h>
#include <linux/ioctl.h>
#include <linux/init.h>

MODULE_LICENSE("GPL");

#define DEVICE_RANGE_NAME "encdev"
#define BUF_LEN 128
#define IOCTL_OP_GETMAJOR 10
#define IOCTL_OP_SETKEY 20
#define IOCTL_OP_ENCRYPT 30
#define IOCTL_OP_REWIND 40


static int buffLen=128;
module_param(buffLen, int, 0);
MODULE_PARM_DESC(buffLen, "Device's buffer length");
static int major;
static struct list_node* firstDeviceOnList;


int init_module(void);
static ssize_t device_read(struct file* filp, char __user* buffer, size_t length, loff_t* offset);
static ssize_t device_write(struct file* filp, const char __user* buffer, size_t length, loff_t* offset);
int device_open(struct inode* inode, struct file* filp);
int device_release(struct inode* inode, struct file* filp);
static long device_ioctl(struct file* filp, unsigned int ioctl_command_id, unsigned long ioctl_param);


struct list_node {
	int	minor;
	char* buffer;
	struct list_node* next;
};


struct file_data {
	char encryptionKey;
	int encryptionFlag;
	struct list_node* list_item;
};


struct file_operations fops = {
	.owner			= THIS_MODULE,
	.read			= device_read,
	.write			= device_write,
	.open			= device_open,
	.release		= device_release,
	.unlocked_ioctl = device_ioctl,
};


int init_module(void)
{
	printk("Module Initialize Procedure Begins\n");

	major = register_chrdev(0, DEVICE_RANGE_NAME, &fops);

	//System Call "register_chrdev" Error Check
	if (major < 0) 
	{
		printk(KERN_ALERT "%s Registration Has Failed: %d\n", DEVICE_RANGE_NAME, major);
		return major;
	}

	firstDeviceOnList = kmalloc(sizeof(struct list_node), GFP_KERNEL);

	//System Call "kmalloc" Error Check
	if (!firstDeviceOnList)
	{
		return -1;
	}

	firstDeviceOnList->minor = -1;
	firstDeviceOnList->buffer = NULL;
	firstDeviceOnList->next = NULL;

	printk("Module %s - %d Is Registered Successfuly\n", DEVICE_RANGE_NAME, major);

	printk("Registration is successful\n");
	printk("Major device number is: %d\n", major);
	printk("To talk to the device driver, create a device file:\n");
	printk("mknod /dev/<name> c %d 0\n", major);
	printk("Use echo/cat to/form device file\n");
	printk("Don't forget to rm the device file and rmmod the module!\n");

	return 0;
}


static ssize_t device_read(struct file* filp, char __user* userBuffer, size_t length, loff_t* offset)
{
	struct file_data* fileData = (struct file_data*)filp->private_data;
	char* deviceAssociatedBuffer = fileData->list_item->buffer;
	int i;
	int returnValue;
	int	encryptionKey = fileData->encryptionKey;
	int encryptionFlag = fileData->encryptionFlag;
	char encryptedChar;
	
	printk("Device Read Procedure Begins - (%p,%ld): ", filp, length);
	for (i = 0; i < length && *offset + i < buffLen; ++i)
	{
		//Deal With Encrypted Data
		if (encryptionFlag == 1)
		{
			//Xor Buffer's Data With Encryption Key
			encryptedChar = (deviceAssociatedBuffer[*offset + i] ^ encryptionKey);

			//Transfer All Encrypted Data To The User
			returnValue = put_user(encryptedChar, &userBuffer[i]);

			//In Case "put_user" Has Failed (If "put_user" Returns Anything Other Than Zero)
			if (returnValue != 0)
			{
				printk(KERN_ALERT "Warning! put_user() Has Failed\n");
				return -1;
			}
		}

		//Deal With Non-Encrypted Data - So "encryptionFlag" Has Value Of 0 (IOCTL_OP_ENCRYPT Will Verify "encryptionFlag" Has Value Of 1 OR 0)
		else
		{
			returnValue = put_user(deviceAssociatedBuffer[*offset + i], &userBuffer[i]);

			//In Case "put_user" Has Failed (If "put_user" Returns Anything Other Than Zero)
			if (returnValue != 0)
			{
				printk(KERN_ALERT "Warning! put_user() Has Failed\n");
				return -1;
			}
		}
	}

	//Offset Update Procedure
	*offset += i;
	return i;
}


static ssize_t device_write(struct file* filp, const char __user* userBuffer, size_t length, loff_t* offset)
{
	struct file_data* fileData = (struct file_data*)filp->private_data;
	char* deviceAssociatedBuffer = fileData->list_item->buffer;
	int i;
	int returnValue;
	int	encryptionKey = fileData->encryptionKey;
	int encryptionFlag = fileData->encryptionFlag;

	printk("Device Write Procedure Begins - (%p,%ld): ", filp, length);
	for (i = 0; i < length && *offset + i < buffLen; ++i) 
	{
		//Deal With Data - Base Case
		//Transfer All Data From User To Device's Buffer
		returnValue = get_user(deviceAssociatedBuffer[*offset + i], &userBuffer[i]);
		
		//In Case "get_user" Has Failed (If "get_user" Returns Anything Other Than Zero)
		if (returnValue != 0)
		{
			printk(KERN_ALERT "Warning! get_user() Has Failed\n");
			return -1;
		}
		else
		{
			//Deal With Encrypted Data
			if (encryptionFlag == 1)
			{
				//Xor Buffer's Data With Encryption Key
				deviceAssociatedBuffer[*offset + i] = deviceAssociatedBuffer[*offset + i] ^ encryptionKey;
			}
		}
	}

	//Offset Update Procedure
	*offset += i;
	return i;
}


int device_open(struct inode* inode, struct file* filp)
{
	struct list_node* newDevice;
	struct list_node* currentDevice = firstDeviceOnList;
	struct file_data* data = kmalloc(sizeof(struct file_data), GFP_KERNEL);
	int currentDeviceMinor = MINOR(inode->i_rdev);

	printk("Device Open Procedure Begins - %d\n", currentDeviceMinor);

	//System Call "kmalloc" Error Check
	if (!data)
	{
		return -1;
	}

	data->encryptionKey = 0;
	data->encryptionFlag = 0;

	//Check For Existence Of The Device In The List
	while (currentDevice != NULL)
	{
		if (currentDevice->minor == currentDeviceMinor)
		{
			data->list_item = currentDevice;
			filp->private_data = data;
			return 0;
		}
		
		currentDevice = currentDevice->next;
	}

	//If We Got Here So The Device Is Not Exist In The Devices List. We Will Set A New Device
	newDevice = kmalloc(sizeof(struct list_node), GFP_KERNEL);

	//System Call "kmalloc" Error Check
	if (!newDevice)
	{
		return -1;
	}
	
	newDevice->minor = currentDeviceMinor;
	newDevice->buffer = kcalloc(buffLen, sizeof(char), GFP_KERNEL);

	//System Call "kcalloc" Error Check
	if (!newDevice->buffer)
	{
		return -1;
	}
	
	//New Device Values Set
	newDevice->next = firstDeviceOnList->next;
	firstDeviceOnList->next = newDevice;
	data->list_item = newDevice;
	filp->private_data = data;	

	return 0;
}


int device_release(struct inode* inode, struct file* filp)	
{
	if (filp->private_data) 
	{
		struct file_data* data = (struct file_data*)filp->private_data;
		printk("Device Release Procedure Begins - %d, %d\n", data->encryptionFlag, data->encryptionKey);
		kfree(data);
	}

	return 0;
}


static long device_ioctl(struct file* filp, unsigned int ioctl_command_id, unsigned long ioctl_param)
{		
	//in case user has entered an "Get Major" OPERATION IOCTL COMMAND
	if (ioctl_command_id == IOCTL_OP_GETMAJOR) 
	{
		printk("Invoke IOCTL_OP_GETMAJOR Operation, Major Value Is: %d\n", major);
		return major;
	}

	//in case user has entered an "Set Key" OPERATION IOCTL COMMAND
	else if (ioctl_command_id == IOCTL_OP_SETKEY) 
	{
		((struct file_data*)filp->private_data)->encryptionKey = (char)ioctl_param;
		printk("Invoke IOCTL_OP_SETKEY Operation, Encryption Key Value Is: %c\n", ((struct file_data*)filp->private_data)->encryptionKey);
	}

	//in case user has entered an "Encrypt" OPERATION IOCTL COMMAND
	else if (ioctl_command_id == IOCTL_OP_ENCRYPT)
	{
		//base case - if Parameter value is 1 OR 0, then proceed
		if (ioctl_param == 0 || ioctl_param == 1)
		{
			((struct file_data*)filp->private_data)->encryptionFlag = (int)ioctl_param;
			printk("Invoke IOCTL_OP_ENCRYPT Operation, Encryption Flag Value Is: %d\n", ((struct file_data*)filp->private_data)->encryptionFlag);
		}
		else
		{
			printk(KERN_ALERT "Error With Setting Encryption Flag For The Device %d\n", ((struct file_data*)filp->private_data)->list_item->minor);
			return -EINVAL;
		}
	}

	//in case user has entered an "Rewind" OPERATION IOCTL COMMAND
	else if (ioctl_command_id == IOCTL_OP_REWIND) 
	{
			printk("Invoke IOCTL_OP_REWIND Operation: Offset's old Value Is: %lld \n", filp->f_pos);
			filp->f_pos = 0;
			printk("Finish IOCTL_OP_REWIND Operation: Offset's new Value Is: %lld \n", filp->f_pos);
	}

	
	printk("Device's Private Data Is: Enryption Key Value = %c, Encryption Flag Value = %d, Offset Value = %lld \n", ((struct file_data*)filp->private_data)->encryptionKey, ((struct file_data*)filp->private_data)->encryptionFlag, filp->f_pos);
	return 0;
}


void cleanup_module(void)
{
	struct list_node* deviceToCleanup;

	printk("Module Cleanup Procedure Begins\n");
	unregister_chrdev(major, DEVICE_RANGE_NAME);

	//System Call "unregister_chrdev()" Always Returns 0. There Is No Need To Check The Return Value
	//System Call "unregister_chrdev" Error Check
	//if (major < 0) 
	//{
	//	printk(KERN_ALERT "%s Unregistration Has Failed: %d\n", DEVICE_RANGE_NAME, major);
	//}

	while (firstDeviceOnList != NULL)
	{
		deviceToCleanup = firstDeviceOnList;
		firstDeviceOnList = firstDeviceOnList->next;

		kfree(deviceToCleanup->buffer);
		kfree(deviceToCleanup);
	}

	printk("Module %s - %d Is Unregistered Successfuly\n", DEVICE_RANGE_NAME, major);
}

		
		
