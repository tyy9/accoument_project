#include <stdio.h>
#include "project2.h"
#include <linux/input.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <sys/ipc.h>
#include <semaphore.h>
int main()
{
    lcd_open();
    sem_init(&input, 0, 0);
    sem_init(&output, 0, 1);
    inittouch_device();
    pthread_t touch;
    pthread_create(&touch,NULL, touch_thread, NULL);
    initLinkList(&head); // 创建无头循环链表
    while (1)
    {
        lock_menu();
    }
    lcd_close();
    close(fd_touch);
    pthread_join(touch, NULL);
    return 0;
}