#include<stdio.h>
#include"project.h"
#include<linux/input.h>

int main(){
    if(inittouch_device()<0){
        perror("初始化硬件失败\n");
        return 0;
    }
    lcd_open();
    initLinkList(&head);//创建无头循环链表
    while(1){
    lock_menu();
    }

    lcd_close();
    close(fd_touch);
    return 0;
    
}