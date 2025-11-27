#include "stddef.h"
#include "lib/printk.h"

void hex_dump(void *addr, size_t len) {
    unsigned char *ptr = (unsigned char *)addr;

    unsigned long num_addr = (unsigned long)ptr;
    unsigned long aligned_addr = num_addr & ~ 0xF;

    for (unsigned long row_addr = aligned_addr; row_addr < num_addr + len; row_addr += 16) {
        printk("0x%x:    ", row_addr);   
        
        for (int col = 0; col < 16; col++){
            unsigned long byte_addr = row_addr + col;
        
            if (byte_addr < num_addr || byte_addr >= num_addr + len) {
                printk(".. ");  
            } else {
                unsigned char byte = *(unsigned char *)byte_addr;
                if (byte < 0x10) {
                    printk("0");  
                }
                printk("%x ", byte );
            }
        } 
        
        printk("   ");
        for (int col = 0; col < 16; col++){
            unsigned long byte_addr = row_addr + col;
        
            if (byte_addr < num_addr || byte_addr >= num_addr + len) {
                printk(".");  
            } else {
                unsigned char byte = *(unsigned char *)byte_addr;
                if (byte > 31 && byte < 127) {
                    printk("%c", byte); 
                } else {
                    printk(".");  
                }
            }
        } 

        printk("\n");
    }
}
