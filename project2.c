#include<stdio.h>
#include"project.h"
#include<linux/input.h>
int main(){
    if(inittouch_device()<0){
        perror("初始化硬件失败\n");
        return 0;
    }
    lcd_open();
     pthread_t touch;
     pthread_create(&touch,NULL,touch_thread,NULL);   
    initLinkList(&head);//创建无头循环链表
    lcd_close();
    close(fd_touch);
     pthread_join(touch,NULL);
    return 0;
    
}