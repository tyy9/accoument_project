#include<stdio.h>
#include"project.h"
#include<linux/input.h>

int main(){
    if(inittouch_device()<0){
        perror("初始化硬件失败\n");
        return 0;
    }
    lcd_open();
    lock_menu();
    lcd_close();
    close(fd_touch);
    return 0;
    
}