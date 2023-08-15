#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <string.h>
#include <sys/mman.h>
#include "bmp_plus.h"
#include "font.h"
#include <dirent.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>
#include <math.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
typedef struct LNode
{
    char *file_name;
    struct LNode *next;
    struct LNode *prev;
} LNode, *LinkList;

// 全局变量
int fd_touch, fd_touch2;
struct input_event buf;
unsigned int x, y;
unsigned int x_press, y_press;
unsigned int main_x, main_y;
unsigned int main_x_press, main_y_press;
unsigned int lock_x, lock_y;
unsigned int lock_x_press, lock_y_press;
LinkList head;
LNode *Tail;         // 尾插
sem_t input, output; // I/O信号量
int stop_account_flag = 0, account_end_flag = 0;
int exit_flag = 0;
int thread_stop_flag = 0;
int thread_exit = 0;
// 函数声明
int inittouch_device(int *fd);
int lock_menu();
int slide_lock_menu();
int main_menu();
int update_file();
struct LcdDevice *init_lcd();
int img_player();
int Draw();
void draw_cricle(int a, int b, int r, unsigned int color);
int my_start(); // 开机动画
int my_exit();  // 关机动画
// 链表函数
int initLinkList(LinkList *head);
int addLinkList(LinkList *head, const char *file_name);
int destroyLinkList(LinkList *head);
int formatLinkList(LinkList *head); // 格式化链表
void showLinkList(LinkList head);
int addLinkListData_to_File(LinkList head);
int isLinkListNull(LinkList head);
// 线程函数
void *touch_thread(void *arg);
void *account_thread(void *arg);

int my_start()
{
    // 每次进来先格式化链表数据
    formatLinkList(&head);
    // 初始化Lcd
    char cur_dir[] = ".";          // 当前目录
    char up_dir[] = "..";          // 上级目录
    int x_limit = 600, x_cur = 20; // 进度条的限制
    // 使用scandir保证文件读取顺序正确
    struct dirent **entry_list;
    int file_count;
    int i = 0;
    file_count = scandir("/root/my_test/movie/", &entry_list, 0, alphasort);
    while (i < file_count)
    {
        struct dirent *file_dir;
        file_dir = entry_list[i];
        printf("file:%s\n", file_dir->d_name);
        if (strcmp(file_dir->d_name, cur_dir) != 0 && strcmp(file_dir->d_name, up_dir) != 0)
        {
            addLinkList(&head, file_dir->d_name);
        }
        free(file_dir);
        i++;
    }
    free(entry_list);
    // 播放
    LNode *p = head;
    while (p->next != head)
    {
        char bmp_path[100] = "/root/my_test/movie/";
        char *new_bmp_path = strtok(p->file_name, "\n");
        strcat(bmp_path, new_bmp_path);
        lcd_draw_bmp(bmp_path, 0, 0);
        p = p->next;
        usleep(10000);
    }
    formatLinkList(&head);
    lcd_draw_bmp("start.bmp", 0, 0);
    // 下滑进入解锁
    struct input_event buf; // 触摸屏数据结构体
    int start_x, start_y;
    int slide_flag = 0;
    int count = 1;
    while (1)
    {
        if (count > 5)
        {
            count = 1;
        }
        printf("count=%d\n", count);
        // 读取触摸屏数据
        read(fd_touch, &buf, sizeof(buf));
        if (buf.type == EV_ABS)
        {
            // 如果事件类型为绝对位置事件，判断为触摸
            // 第一次返回X值，第二次返回Y值
            if (buf.code == ABS_X)
            {
                start_x = buf.value * 800 / 1024;
                printf("start_x=%d\n", start_x);
            }
            if (buf.code == ABS_Y)
            {
                start_y = buf.value * 480 / 600;
                printf("start_y=%d\n", start_y);
            }
        }
        // 读取触摸屏数据
        // sem_wait(&output);
        printf("start-x:%d,y:%d\n", start_x, start_y);
        if (count == 2)
        {
            if (slide_flag == 1 && start_y * 1024 / 800 <= 480)
            {
                lcd_draw_bmp_lock("my_lock.bmp", 0, 0, start_y * 1024 / 800);
                printf("lcd_x=%d\n", start_y * 1024 / 800);
            }
        }
        // 处于按键或触摸状态
        if (buf.type == EV_KEY && buf.code == BTN_TOUCH)
        {
            if (buf.value)
            {
                int start_y_press = start_y;
                int start_x_press = start_x;
                if (start_y_press <= 40 * 1024 / 800)
                {
                    printf("start_y_press=%d\n", start_y_press);
                    slide_flag = 1;
                }
            }
            else
            {
                if (slide_flag == 1)
                {
                    if (start_y >= 50 * 800 / 1024)
                    {
                        slide_flag = 0;
                        lcd_draw_bmp("my_lock.bmp", 0, 0);
                        // sem_post(&output);//唤醒锁屏
                        slide_lock_menu();
                        // lcd_draw_bmp("start_menu.bmp", 0, 0);
                        break;
                    }
                }
            }
        }
        count++;
    }
    // sem_post(&input);
    main_menu();
    return 0;
}

int initLinkList(LinkList *head)
{
    // *head = (LNode *)malloc(sizeof(LNode));
    // if (*head == NULL)
    // {
    //     perror("链表初始化失败\n");
    //     return 0;
    // }
    // (*head)->next = NULL;
    (*head) = NULL;
    printf("链表初始化成功\n");
    return 1;
}

int inittouch_device(int *fd)
{
    *fd = open("/dev/input/event0", O_RDONLY);
    if (fd < 0)
    {
        perror("触摸屏打开失败\n");
        return -1;
    }
    return 0;
}
int isLinkListNull(LinkList head)
{
    if (head == NULL)
    {
        return 1;
    }
    return 0;
}
int addLinkList(LinkList *head, const char *file_name)
{
    // 查找尾指针
    // LNode *temp = head->next, *Tail = head;
    int flag = 0;
    LNode *Tail = *head;
    printf("1\n");
    if (isLinkListNull(*head))
    {
        flag = 1;
    }
    else
    {
        LNode *temp = *head;
        while (temp->next != *head)
        {
            temp = temp->next;
            Tail = temp;
        }
    }

    printf("2\n");
    LNode *p = NULL;
    p = (LNode *)malloc(sizeof(LNode));
    if (p == NULL)
    {
        perror("新节点分配空间失败\n");
        return 0;
    }
    printf("list add file:%s,size:%d", file_name, sizeof(file_name));
    p->file_name = malloc(strlen(file_name) + 1);
    strcpy(p->file_name, file_name);
    for (int i = 0; i < strlen(p->file_name); i++)
    {
        printf("%d\t", p->file_name[i]);
    }
    if (flag == 1)
    {
        printf("7\n");
        *head = p;
    }
    else
    {
        Tail->next = p; // 此时Tail的地址没有改变，所以在首次加入时由于没有内存分配，不可使用内部的指针变量
    }
    printf("5\n");
    p->next = *head;
    (*head)->prev = p;
    p->prev = Tail;
    printf("6\n");
    Tail = p;
    printf("3\n");
    return 0;
}

void showLinkList(LinkList head)
{
    // LNode *p = head->next;
    LNode *p = head->next;
    printf("目前链表的数据如下:\n");
    printf("file_name:%s\n", head->file_name);
    while (p != head)
    {

        printf("file_name:%s\n", p->file_name);
        p = p->next;
    }
}

int addLinkListData_to_File(LinkList head)
{
    printf("11\n");
    FILE *fp = fopen("file_list.txt", "w+");
    // 初始化Lcd
    printf("12\n");
    struct LcdDevice *lcd = init_lcd("/dev/fb0");
    // 文件自动创建，每次写前都将清空
    if (fp == NULL)
    {
        perror("文件打开失败\n");
        return 0;
    }
    printf("13\n");
    // LNode *p = head->next;
    LNode *p = head->next;
    // 先读头结点
    // char buf1[strlen(head->file_name)];
    // setbuf(fp, buf1);
    printf("文件写入:%s\n", head->file_name);
    fprintf(fp, "%s\n", head->file_name);
    fflush(fp);
    char buf[50];
    memset(buf, 0, sizeof(buf));
    strcat(buf, "更新图片播放列表文件--");
    strcat(buf, head->file_name);
    // 加载字体
    font *f = fontLoad("simfang.ttf");
    // 字体大小的设置
    fontSetSize(f, 30);
    bitmap *bm = createBitmapWithInit(600, 30, 4, getColor(1, 0, 0, 0));
    fontPrint(f, bm, 0, 0, buf, getColor(0, 255, 255, 255), 0);
    show_font_to_lcd(lcd->mp, 200, 370, bm);
    // 卸载字体
    fontUnload(f);
    destroyBitmap(bm);
    usleep(10000);
    while (p != head)
    {
        // char buf2[strlen(p->file_name)];
        // setbuf(fp, buf2);
        printf("文件写入:%s\n", p->file_name);
        fprintf(fp, "%s\n", p->file_name);
        fflush(fp);
        char buf[50];
        memset(buf, 0, sizeof(buf));
        strcat(buf, "更新图片播放列表文件--");
        strcat(buf, p->file_name);
        // 加载字体
        font *f = fontLoad("simfang.ttf");
        // 字体大小的设置
        fontSetSize(f, 30);
        bitmap *bm = createBitmapWithInit(600, 30, 4, getColor(1, 0, 0, 0));
        fontPrint(f, bm, 0, 0, buf, getColor(0, 255, 255, 255), 0);
        show_font_to_lcd(lcd->mp, 200, 370, bm);
        // 卸载字体
        fontUnload(f);
        destroyBitmap(bm);
        p = p->next;
        usleep(10000);
    }
}
int formatLinkList(LinkList *head)
{
    // LNode *p = (*head)->next, *q = NULL;
    if (isLinkListNull(*head))
    {
        return 0;
    }
    LNode *p = (*head)->next, *q = NULL;
    while (p != *head)
    {
        q = p;
        p = p->next;
        q->next = NULL;
        free(q->file_name);
        free(q);
    }
    (*head)->next = NULL;
    free(*head);
    *head = NULL;
    return 0;
}

struct LcdDevice *init_lcd()
{
    // 申请空间
    struct LcdDevice *lcd = malloc(sizeof(struct LcdDevice));
    if (lcd == NULL)
    {
        return NULL;
    }

    // 打开LCD设备
    lcd->fd = fd_lcd;
    // 映射
    lcd->mp = mmap(NULL, 800 * 480 * 4, PROT_READ | PROT_WRITE, MAP_SHARED, lcd->fd, 0);

    return lcd;
}

int lock_menu()
{
    lcd_draw_bmp("my_lock.bmp", 0, 0);
    char pw[7];
    memset(pw, 0, sizeof(pw));
    int index = 0;
    int password_index = 0;
    struct input_event buf; // 触摸屏数据结构体
                            // 定义字体
    // 初始化Lcd
    struct LcdDevice *lcd = init_lcd();

    // 加载字体
    font *f = fontLoad("simfang.ttf");

    // 字体大小的设置
    fontSetSize(f, 30);

    while (1)
    {

        sem_wait(&output);
        printf("lock-x:%d,y:%d\n", x, y);
        // read(fd_touch2, &buf, sizeof(buf));
        // 判断当前点击操作是否释放
        // 1,4,7
        if (x >= 156 && x <= 234)
        {
            if (y >= 96 && y <= 168)
            {
                printf("1\n");
                if (index < 4)
                {
                    pw[index] = '1';
                    index++;
                    lcd_draw_bmp("password.bmp", 200 + (100 * password_index), 20);
                    password_index++;
                }
            }
            if (y > 168 && y <= 240)
            {
                printf("4\n");
                if (index < 4)
                {
                    pw[index] = '4';
                    index++;
                    lcd_draw_bmp("password.bmp", 200 + (100 * password_index), 20);
                    password_index++;
                }
            }
            if (y > 240 && y <= 312)
            {
                printf("7\n");
                if (index < 4)
                {
                    pw[index] = '7';
                    index++;
                    lcd_draw_bmp("password.bmp", 200 + (100 * password_index), 20);
                    password_index++;
                }
            }
        }
        else if (x >= 273 && x <= 351)
        {
            if (y >= 96 && y <= 168)
            {
                printf("2\n");
                if (index < 4)
                {
                    pw[index] = '2';
                    index++;
                    lcd_draw_bmp("password.bmp", 200 + 100 * password_index, 20);
                    password_index++;
                }
            }
            if (y > 168 && y <= 240)
            {
                printf("5\n");
                if (index < 4)
                {
                    pw[index] = '5';
                    index++;
                    lcd_draw_bmp("password.bmp", 200 + 100 * password_index, 20);
                    password_index++;
                }
            }
            if (y > 240 && y <= 312)
            {
                printf("8\n");
                if (index < 4)
                {
                    pw[index] = '8';
                    index++;
                    lcd_draw_bmp("password.bmp", 200 + 100 * password_index, 20);
                    password_index++;
                }
            }
            if (y > 312 && y <= 384)
            {
                printf("0\n");
                if (index < 4)
                {
                    pw[index] = '0';
                    index++;
                    lcd_draw_bmp("password.bmp", 200 + 100 * password_index, 20);
                    password_index++;
                }
            }
        }
        else if (x >= 390 && x <= 468)
        {
            if (y >= 96 && y <= 168)
            {
                printf("3\n");
                if (index < 4)
                {
                    pw[index] = '3';
                    index++;
                    lcd_draw_bmp("password.bmp", 200 + 100 * password_index, 20);
                    password_index++;
                }
            }
            if (y > 168 && y <= 240)
            {
                printf("6\n");
                if (index < 4)
                {
                    pw[index] = '6';
                    index++;
                    lcd_draw_bmp("password.bmp", 200 + 100 * password_index, 20);
                    password_index++;
                }
            }
            if (y > 240 && y <= 312)
            {
                printf("9\n");
                if (index < 4)
                {
                    pw[index] = '9';
                    index++;
                    lcd_draw_bmp("password.bmp", 200 + 100 * password_index, 20);
                    password_index++;
                }
            }
        }
        if (x >= 156 && x <= 234)
        {
            if (y > 312 && y <= 384)
            {
                printf("del\n");
                pw[index] = '\0';
                index--;
                password_index--;
                if (index >= 0)
                {
                    for (int y = 20; y <= 100; y++)
                    {
                        for (int x = 200 + 100 * password_index; x <= 200 + 100 * (password_index + 1); x++)
                        {
                            lcd_draw_point(x, y, 0X00FFFFFF);
                        }
                    }
                }
                if (password_index < 0)
                {
                    password_index = 0;
                }
                if (index < 0)
                {
                    index = 0;
                }
            }
        }
        else if (x >= 390 && x <= 468)
        {
            if (y > 312 && y <= 384)
            {
                printf("enter\n");
                for (int i = 0; i < 7; i++)
                {
                    printf("%c", pw[i]);
                }
                if (strncmp("1234", pw, 6) == 0)
                {
                    printf("登录成功\n");
                    // 创建一个画板（点阵图）
                    // 画板宽400，高90，色深32位色即4字节，画板颜色为白色
                    bitmap *bm = createBitmapWithInit(180, 90, 4, getColor(0, 0, 0, 0));
                    char buf[] = "登录成功\n2秒后自动\n转跳";

                    fontPrint(f, bm, 0, 0, buf, getColor(0, 0, 255, 0), 0);
                    show_font_to_lcd(lcd->mp, 20, 120, bm);
                    // 卸载字体
                    fontUnload(f);
                    destroyBitmap(bm);
                    free(lcd);
                    sleep(3);
                    // sem_post(&input);//阻塞触屏线程，主界面使用自己的触屏
                    main_menu();
                    if (exit_flag)
                    {
                        return 1;
                    }
                    // lcd_draw_bmp("01.bmp", 0, 0);
                    // lcd_draw_bmp("main_menu.bmp", 0, 0);
                    break;
                }
                else
                {
                    printf("登录失败\n");
                    index = 0;
                    memset(pw, 0, 7);
                    password_index = 0;
                    bitmap *bm = createBitmapWithInit(180, 90, 4, getColor(0, 0, 0, 0));
                    char buf[] = "登录失败\n密码有误";

                    fontPrint(f, bm, 0, 0, buf, getColor(0, 0, 0, 255), 0);

                    show_font_to_lcd(lcd->mp, 20, 120, bm);

                    for (int y = 20; y <= 100; y++)
                    {
                        for (int x = 200; x <= 600; x++)
                        {
                            lcd_draw_point(x, y, 0X00FFFFFF);
                        }
                    }
                    sleep(3);
                    // 自动清除提示
                    destroyBitmap(bm);
                }
            }
        }
        sem_post(&input);
    }
    return 1;
}

int slide_lock_menu()
{
    // lcd_draw_bmp("my_lock.bmp", 0, 0);
    char pw[7];
    memset(pw, 0, sizeof(pw));
    int index = 0;
    int password_index = 0;
    struct input_event buf; // 触摸屏数据结构体
                            // 定义字体
    // 初始化Lcd
    struct LcdDevice *lcd = init_lcd();

    // 加载字体
    font *f = fontLoad("simfang.ttf");

    // 字体大小的设置
    fontSetSize(f, 30);

    while (1)
    {
        // sem_wait(&output);
        // printf("阻塞锁屏\n");
        printf("lock-x:%d,y:%d\n", lock_x, lock_y);
        // read(fd_touch2, &buf, sizeof(buf));
        // 判断当前点击操作是否释放
        // 1,4,7
        read(fd_touch, &buf, sizeof(buf));
        if (buf.type == EV_ABS)
        {
            // 如果事件类型为绝对位置事件，判断为触摸
            // 第一次返回X值，第二次返回Y值
            if (buf.code == ABS_X)
            {
                lock_x = buf.value * 800 / 1024;
                printf("lock_x=%d\n", lock_x);
            }
            if (buf.code == ABS_Y)
            {
                lock_y = buf.value * 480 / 600;
                printf("lock_y=%d\n", lock_y);
            }
        }
        if (buf.type == EV_KEY && buf.code == BTN_TOUCH)
        {
            if (buf.value)
            {
                if (x > 400)
                {
                }
                if (x < 400)
                {
                }
            }
            else
            {

                if (lock_x >= 156 && lock_x <= 234)
                {
                    if (lock_y >= 96 && lock_y <= 168)
                    {
                        printf("1\n");
                        if (index < 4)
                        {
                            pw[index] = '1';
                            index++;
                            lcd_draw_bmp("password.bmp", 200 + (100 * password_index), 20);
                            password_index++;
                        }
                    }
                    if (lock_y > 168 && lock_y <= 240)
                    {
                        printf("4\n");
                        if (index < 4)
                        {
                            pw[index] = '4';
                            index++;
                            lcd_draw_bmp("password.bmp", 200 + (100 * password_index), 20);
                            password_index++;
                        }
                    }
                    if (lock_y > 240 && lock_y <= 312)
                    {
                        printf("7\n");
                        if (index < 4)
                        {
                            pw[index] = '7';
                            index++;
                            lcd_draw_bmp("password.bmp", 200 + (100 * password_index), 20);
                            password_index++;
                        }
                    }
                }
                else if (lock_x >= 273 && lock_x <= 351)
                {
                    if (lock_y >= 96 && lock_y <= 168)
                    {
                        printf("2\n");
                        if (index < 4)
                        {
                            pw[index] = '2';
                            index++;
                            lcd_draw_bmp("password.bmp", 200 + 100 * password_index, 20);
                            password_index++;
                        }
                    }
                    if (lock_y > 168 && lock_y <= 240)
                    {
                        printf("5\n");
                        if (index < 4)
                        {
                            pw[index] = '5';
                            index++;
                            lcd_draw_bmp("password.bmp", 200 + 100 * password_index, 20);
                            password_index++;
                        }
                    }
                    if (lock_y > 240 && lock_y <= 312)
                    {
                        printf("8\n");
                        if (index < 4)
                        {
                            pw[index] = '8';
                            index++;
                            lcd_draw_bmp("password.bmp", 200 + 100 * password_index, 20);
                            password_index++;
                        }
                    }
                    if (lock_y > 312 && lock_y <= 384)
                    {
                        printf("0\n");
                        if (index < 4)
                        {
                            pw[index] = '0';
                            index++;
                            lcd_draw_bmp("password.bmp", 200 + 100 * password_index, 20);
                            password_index++;
                        }
                    }
                }
                else if (lock_x >= 390 && lock_x <= 468)
                {
                    if (lock_y >= 96 && lock_y <= 168)
                    {
                        printf("3\n");
                        if (index < 4)
                        {
                            pw[index] = '3';
                            index++;
                            lcd_draw_bmp("password.bmp", 200 + 100 * password_index, 20);
                            password_index++;
                        }
                    }
                    if (lock_y > 168 && lock_y <= 240)
                    {
                        printf("6\n");
                        if (index < 4)
                        {
                            pw[index] = '6';
                            index++;
                            lcd_draw_bmp("password.bmp", 200 + 100 * password_index, 20);
                            password_index++;
                        }
                    }
                    if (lock_y > 240 && lock_y <= 312)
                    {
                        printf("9\n");
                        if (index < 4)
                        {
                            pw[index] = '9';
                            index++;
                            lcd_draw_bmp("password.bmp", 200 + 100 * password_index, 20);
                            password_index++;
                        }
                    }
                }
                if (lock_x >= 156 && lock_x <= 234)
                {
                    if (lock_y > 312 && lock_y <= 384)
                    {
                        printf("del\n");
                        pw[index] = '\0';
                        index--;
                        password_index--;
                        if (index >= 0)
                        {
                            for (int y = 20; y <= 100; y++)
                            {
                                for (int x = 200 + 100 * password_index; x <= 200 + 100 * (password_index + 1); x++)
                                {
                                    lcd_draw_point(x, y, 0X00FFFFFF);
                                }
                            }
                        }
                        if (password_index < 0)
                        {
                            password_index = 0;
                        }
                        if (index < 0)
                        {
                            index = 0;
                        }
                    }
                }
                else if (lock_x >= 390 && lock_x <= 468)
                {
                    if (lock_y > 312 && lock_y <= 384)
                    {
                        printf("enter\n");
                        for (int i = 0; i < 7; i++)
                        {
                            printf("%c", pw[i]);
                        }
                        if (strncmp("1234", pw, 6) == 0)
                        {
                            printf("登录成功\n");
                            // 创建一个画板（点阵图）
                            // 画板宽400，高90，色深32位色即4字节，画板颜色为白色
                            bitmap *bm = createBitmapWithInit(180, 90, 4, getColor(0, 0, 0, 0));
                            char buf[] = "登录成功\n2秒后自动\n转跳";

                            fontPrint(f, bm, 0, 0, buf, getColor(0, 0, 255, 0), 0);
                            show_font_to_lcd(lcd->mp, 20, 120, bm);
                            // 卸载字体
                            fontUnload(f);
                            destroyBitmap(bm);
                            free(lcd);
                            sleep(3);
                            // sem_post(&input);

                            //  main_menu();
                            //   lcd_draw_bmp("01.bmp", 0, 0);
                            //   lcd_draw_bmp("main_menu.bmp", 0, 0);
                            break;
                        }
                        else
                        {
                            printf("登录失败\n");
                            index = 0;
                            memset(pw, 0, 7);
                            password_index = 0;
                            bitmap *bm = createBitmapWithInit(180, 90, 4, getColor(0, 0, 0, 0));
                            char buf[] = "登录失败\n密码有误";

                            fontPrint(f, bm, 0, 0, buf, getColor(0, 0, 0, 255), 0);

                            show_font_to_lcd(lcd->mp, 20, 120, bm);

                            for (int y = 20; y <= 100; y++)
                            {
                                for (int x = 200; x <= 600; x++)
                                {
                                    lcd_draw_point(x, y, 0X00FFFFFF);
                                }
                            }
                            sleep(3);
                            // 自动清除提示
                            destroyBitmap(bm);
                        }
                    }
                }
            }
        }
        // sem_post(&input);
        // printf("唤醒触摸屏\n");
    }
    return 1;
}
int main_menu()
{
    lcd_draw_bmp("main_menu.bmp", 0, 0);
    struct input_event buf; // 触摸屏数据结构体
    int slide_flag = 0;
    int count = 1;
    while (1)
    {
        // sem_wait(&input);
        if (count > 5)
        {
            count = 1;
        }
        printf("count=%d\n", count);
        // 读取触摸屏数据
        read(fd_touch, &buf, sizeof(buf));
        if (buf.type == EV_ABS)
        {
            // 如果事件类型为绝对位置事件，判断为触摸
            // 第一次返回X值，第二次返回Y值
            if (buf.code == ABS_X)
            {
                main_x = buf.value * 800 / 1024;
                printf("main_x=%d\n", main_x);
            }
            if (buf.code == ABS_Y)
            {
                main_y = buf.value * 480 / 600;
                printf("main_y=%d\n", main_y);
            }
        }
        // 读取触摸屏数据
        // sem_wait(&output);
        printf("main-x:%d,y:%d\n", main_x, main_y);
        if (count == 2)
        {
            if (slide_flag == 1 && main_y * 1024 / 800 <= 480)
            {
                printf("下滑\n");
                lcd_draw_bmp_lock("my_lock.bmp", 0, 0, main_y * 1024 / 800);
                printf("lcd_x=%d\n", main_y * 1024 / 800);
            }
        }
        // 处于按键或触摸状态
        if (buf.type == EV_KEY && buf.code == BTN_TOUCH)
        {
            if (buf.value)
            {
                main_y_press = main_y;
                main_x_press = main_x;
                if (main_y_press <= 20 * 1024 / 800)
                {
                    printf("main_y_press=%d\n", main_y_press);
                    slide_flag = 1;
                }
            }
            else
            {
                if (slide_flag == 1)
                {
                    if (main_y >= 100 * 800 / 1024)
                    {
                        printf("thread_stop_flag=%d\n", thread_stop_flag);
                        slide_flag = 0;
                        lcd_draw_bmp("my_lock.bmp", 0, 0);
                        slide_lock_menu();
                        lcd_draw_bmp("main_menu.bmp", 0, 0);
                    }
                }
                if (main_x >= 20 * 800 / 1024 && main_x <= 120 * 800 / 1024)
                {
                    if (main_y >= 30 * 480 / 600 && main_y <= 100 * 480 / 600)
                    {
                        printf("更新资源\n");
                        update_file();
                        // main_x=main_y=0;
                        // sem_post(&input);
                    }
                }
                if (main_x > 120 * 800 / 1024 && main_x <= 220 * 800 / 1024)
                {
                    if (main_y >= 30 * 480 / 600 && main_y <= 100 * 480 / 600)
                    {
                        printf("播放器\n");
                        y = y_press = 0;
                        img_player();
                        // sem_post(&input);
                        // sleep(1);
                        // sem_post(&input);
                    }
                }
                if (main_x > 220 * 800 / 1024 && main_x <= 320 * 800 / 1024)
                {
                    if (main_y >= 30 * 480 / 600 && main_y <= 100 * 480 / 600)
                    {
                        printf("刮刮乐\n");
                        // sem_post(&input);
                        Draw();
                        // main_x = main_y = 0;
                    }
                }
                if (main_x >= 720 * 800 / 1024 && main_x <= 800 * 800 / 1024)
                {
                    if (main_y >= 380 * 480 / 600 && main_y <= 480 * 480 / 600)
                    {
                        printf("退出\n");
                        exit_flag = 1;
                        sem_post(&input);
                        sleep(1); // 等待触摸屏线程死亡
                        break;
                    }
                }
            }
        }
        count++;
    }
    // sem_post(&input);
    return 0;
}

int update_file()
{
    // 每次进来先格式化链表数据
    formatLinkList(&head);
    lcd_draw_bmp("update_file.bmp", 0, 0);
    DIR *dirp = opendir("/root/my_test/bmp_resource/");
    // 初始化Lcd
    struct LcdDevice *lcd = init_lcd();
    char cur_dir[] = ".";          // 当前目录
    char up_dir[] = "..";          // 上级目录
    int x_limit = 600, x_cur = 20; // 进度条的限制
    if (dirp == NULL)
    {
        perror("目录打开失败\n");
        return 0;
    }
    while (1)
    {
        struct dirent *file_dir = readdir(dirp);
        if (file_dir != NULL)
        {
            if (strcmp(file_dir->d_name, cur_dir) != 0 && strcmp(file_dir->d_name, up_dir) != 0)
            {
                // 不读取上级与当前目录
                char buf[30];
                memset(buf, 0, sizeof(buf));
                strcpy(buf, file_dir->d_name);
                // 加载字体
                font *f = fontLoad("simfang.ttf");
                // 字体大小的设置
                fontSetSize(f, 30);
                bitmap *bm = createBitmapWithInit(600, 30, 4, getColor(1, 0, 0, 0));
                // 将字体写到点阵图上
                // f:操作的字库
                // 70,0:字体在画板中的起始位置（y,x)
                // buf:显示的内容
                // getColor(0,100,100,100):字体颜色
                // 默认为0
                fontPrint(f, bm, 0, 0, buf, getColor(0, 255, 255, 255), 0);
                // 把字体框输出到LCD屏幕上
                // lcd->mp:mmap后内存映射首地址
                // 0,100:画板显示的起始位置(x,y)
                // bm:画板
                show_font_to_lcd(lcd->mp, 200, 370, bm);
                // 卸载字体
                fontUnload(f);
                destroyBitmap(bm);
                // 将数据存进链表
                printf("4\n");
                addLinkList(&head, buf);

                printf("文件名:%s\n", file_dir->d_name);
                // ~60%
                if (x_cur <= x_limit)
                {
                    int x = x_cur, y = 400;
                    while (x <= x_cur + 100)
                    {
                        for (int y = 400; y <= 440; y++)
                        {
                            lcd_draw_point(x, y, 0X00FFFFFF);
                        }
                        x++;
                        usleep(1000);
                    }
                    x_cur += 100;
                }
            }
        }
        else
        {
            printf("读取结束\n");
            showLinkList(head);
            break;
        }

        usleep(1000);
    }
    printf("end\n");
    closedir(dirp);
    // 将数据写进文件
    printf("goto\n");
    addLinkListData_to_File(head);
    // 40%
    for (int y = 400; y <= 440; y++)
    {
        for (int x = x_cur; x <= 780; x++)
        {
            lcd_draw_point(x, y, 0x00FFFFFF);
        }
    }
    sleep(1);
    lcd_draw_bmp("main_menu.bmp", 0, 0);
    free(lcd);
    formatLinkList(&head); // 防止链表内存爆炸
    return 0;
}
int img_player()
{

    lcd_draw_bmp("reading_file.bmp", 0, 0);
    FILE *fp = fopen("file_list.txt", "r");
    if (fp == 0)
    {
        perror("文件打开失败\n");
        return 0;
    }
    // 初始化Lcd
    struct LcdDevice *lcd = init_lcd();
    int x_limit = 600, x_cur = 20; // 进度条的限制
    char file_name[30];
    memset(file_name, 0, sizeof(file_name));
    while (!feof(fp))
    {
        if (fgets(file_name, sizeof(file_name), fp))
        {
            printf("file:%s--读取", file_name);
            printf("8\n");
            char buf[30];
            memset(buf, 0, sizeof(buf));
            strcpy(buf, file_name);
            printf("10\n");
            // 加载字体
            font *f = fontLoad("simfang.ttf");
            // 字体大小的设置
            fontSetSize(f, 30);
            bitmap *bm = createBitmapWithInit(600, 30, 4, getColor(1, 0, 0, 0));
            fontPrint(f, bm, 0, 0, buf, getColor(0, 255, 255, 255), 0);
            show_font_to_lcd(lcd->mp, 200, 370, bm);
            // 卸载字体
            fontUnload(f);
            destroyBitmap(bm);
            printf("9\n");
            addLinkList(&head, file_name);
            // ~60%
            if (x_cur <= 600)
            {
                int x = x_cur, y = 400;
                while (x <= x_cur + 100)
                {
                    for (int y = 400; y <= 440; y++)
                    {
                        lcd_draw_point(x, y, 0X00FFFFFF);
                    }
                    x++;
                    usleep(1000);
                }
                x_cur += 100;
            }
            // memset(file_name, 0, sizeof(file_name));
            usleep(1000);
        }
    }
    if (isLinkListNull(head))
    {
        char buf[] = "列表中无法找到图片资源，无法播放\n";
        font *f = fontLoad("simfang.ttf");
        fontSetSize(f, 30);
        bitmap *bm = createBitmapWithInit(500, 30, 4, getColor(1, 0, 0, 0));
        fontPrint(f, bm, 0, 0, buf, getColor(0, 255, 255, 255), 0);
        show_font_to_lcd(lcd->mp, 200, 370, bm);
        fontUnload(f);
        destroyBitmap(bm);
        sleep(2);
        lcd_draw_bmp("main_menu.bmp", 0, 0);
        return 0;
    }
    char buf[] = "读取完毕，将自动播放图片资源\n";
    // 加载字体
    font *f = fontLoad("simfang.ttf");
    // 字体大小的设置
    fontSetSize(f, 30);
    bitmap *bm = createBitmapWithInit(500, 30, 4, getColor(1, 0, 0, 0));
    fontPrint(f, bm, 0, 0, buf, getColor(0, 255, 255, 255), 0);
    show_font_to_lcd(lcd->mp, 200, 370, bm);
    // 卸载字体
    fontUnload(f);
    destroyBitmap(bm);
    sleep(1);
    for (int y = 400; y <= 440; y++)
    {
        for (int x = x_cur; x <= 780; x++)
        {
            lcd_draw_point(x, y, 0x00FFFFFF);
        }
    }
    sleep(1);
    // 播放图片资源
    LNode *p = head;
    showLinkList(head);
    pthread_t account;
    pthread_create(&account, NULL, account_thread, NULL);
    pthread_t touch;
    pthread_create(&touch, NULL, touch_thread, NULL);
    while (1)
    {
        stop_account_flag = 0;
        sem_wait(&output); // 0
        // 上滑解锁
        if (y_press - y > 100 && y_press > y)
        {
            stop_account_flag = 1;
            printf("解锁\n");
            sleep(1); // 等待字体线程反应
            if (account_end_flag == 1)
            {
                stop_account_flag = 0;
                account_end_flag = 0;
                // thread_stop_flag = 1; // 主动阻塞
                // sem_post(&input);     // 唤醒触摸屏
                thread_exit = 1;
                sem_post(&input);
                sleep(2);
                // sem_wait(&input);
                lcd_draw_bmp("my_lock.bmp", 0, 0);
                slide_lock_menu();
                // y_press = y = 0;
                break;
            }
        }
        sem_post(&output); // 即使没触摸到也要自动唤醒
        sem_post(&input);
        char bmp_path[100] = "/root/my_test/bmp_resource/";
        char *new_bmp_path = strtok(p->file_name, "\n");
        strcat(bmp_path, new_bmp_path);
        lcd_draw_bmp_swiper(bmp_path, 0, 0);
        p = p->next;
        sleep(3);
    }
    formatLinkList(&head);
    // pthread_join(account,NULL);
    // pthread_join(touch,NULL);
    lcd_draw_bmp("main_menu.bmp", 0, 0);
    return 0;
}
void draw_cricle(int a, int b, int r, unsigned int color)
{
    // 上半
    for (int x = a - r; x <= a + r; x++)
    {
        for (int y = b - sqrt(pow(r, 2) - pow(x - a, 2)); y <= b; y++)
        {
            *(pb + y * 800 + x) = color;
        }
    }
    // 下半
    for (int x = a - r; x <= a + r; x++)
    {
        for (int y = b; y <= b + sqrt(pow(r, 2) - pow(x - a, 2)); y++)
        {
            *(pb + y * 800 + x) = color;
        }
    }
}
int Draw()
{
    // 阻塞触摸屏线程
    // sem_wait(&output);
    // 使用该线程自己的触摸屏
    inittouch_device(&fd_touch);
    lcd_draw_bmp("draw.bmp", 0, 0);
    int draw_x, draw_y;
    struct input_event buf; // 触摸屏数据结构体
    int count = 1, draw_count = 0;
    int num = rand() % 10 + 1; // 1-10随机数
    while (1)
    {
        if (count > 6)
        {
            count = 1;
        }
        // 读取触摸屏数据
        read(fd_touch, &buf, sizeof(buf));
        if (buf.type == EV_ABS)
        {
            // 如果事件类型为绝对位置事件，判断为触摸
            // 第一次返回X值，第二次返回Y值
            if (buf.code == ABS_X)
            {
                draw_x = buf.value * 800 / 1024;
                printf("draw_x=%d\n", draw_x);
            }
            if (buf.code == ABS_Y)
            {
                draw_y = buf.value * 480 / 600;
                printf("draw_y=%d\n", draw_y);
            }
        }
        if (count == 2)
        {
            int cur_x = draw_x * 1024 / 800, cur_y = draw_y * 600 / 480;
            // 需要换算，因为画圆函数是以lcd的内存地址为准

            // 防止越界，边界-半径
            if (cur_x >= 100 && cur_x <= 715)
            {
                if (cur_y >= 255 && cur_y <= 440)
                {
                    printf("cur_x:%d,cur_y=%d,draw_count:%d\n", cur_x, cur_y, draw_count);
                    draw_cricle(cur_x, cur_y, 20, 0X00FFFFFF);
                    draw_count++;
                    // if (draw_count == 100)
                    // {
                    //     printf("num:%d\n", num);
                    //     if (num % 10 == 1)
                    //     {
                    //         lcd_draw_bmp("bouns1.bmp", 0, 0);
                    //     }
                    //     else if (num % 10 > 1 && num % 10 <= 3)
                    //     {
                    //         lcd_draw_bmp("bouns2.bmp", 0, 0);
                    //     }
                    //     else if (num % 10 > 3 && num % 10 <= 7)
                    //     {
                    //         lcd_draw_bmp("bouns3.bmp", 0, 0);
                    //     }
                    //     else if (num % 10 > 7 && num % 10 <= 9 || num % 10 == 0)
                    //     {
                    //         lcd_draw_bmp("bouns4.bmp", 0, 0);
                    //     }
                    //     sleep(4);
                    //     break;
                    // }
                    if (draw_count == 100)
                    {
                        printf("num:%d\n", num);
                        if (num % 10 == 1)
                        {
                            lcd_draw_bmp("bouns1.bmp", 0, 0);
                        }
                        else if (num % 10 > 1 && num % 10 <= 3)
                        {
                            lcd_draw_bmp("bouns2.bmp", 0, 0);
                        }
                        else if (num % 10 > 3 && num % 10 <= 7)
                        {
                            lcd_draw_bmp("bouns3.bmp", 0, 0);
                        }
                        else if (num % 10 > 7 && num % 10 <= 9 || num % 10 == 0)
                        {
                            lcd_draw_bmp("bouns4.bmp", 0, 0);
                        }
                        sleep(4);
                        break;
                    }
                    count = 1;
                    continue;
                }
            }
        }
        // 判断当前点击操作是否释放
        if (buf.type == EV_KEY && buf.code == BTN_TOUCH)
        {
            // 处于按键或触摸状态
            if (buf.value)
            {
            }
            else
            {
            }
        }
        count++;
    }
    // sem_post(&input);
    lcd_draw_bmp("main_menu.bmp", 0, 0);
    return 0;
}
void *touch_thread(void *arg)
{
    int flag = 0;
    struct input_event buf; // 触摸屏数据结构体
    while (1)
    {

        // 读取触摸屏数据
        read(fd_touch, &buf, sizeof(buf));
        printf("thread is running\n");
        if (flag == 0)
        {
            sem_wait(&input); // P
            printf("阻塞触摸屏\n");
            flag = 1;
        }
        if (thread_exit)
        {
            thread_exit = 0;
            printf("线程死亡\n");
            pthread_exit(NULL);
        }
        // if (thread_stop_flag == 1)
        // {
        //     printf("thread--thread_stop_flag=%d\n", thread_stop_flag);
        //     thread_stop_flag = 0;
        //     //asleep(3);
        //     flag = 0;
        //     continue;
        // }
        if (buf.type == EV_ABS)
        {
            // 如果事件类型为绝对位置事件，判断为触摸
            // 第一次返回X值，第二次返回Y值
            if (buf.code == ABS_X)
            {
                x = buf.value * 800 / 1024;
                printf("x=%d\n", x);
            }
            if (buf.code == ABS_Y)
            {
                y = buf.value * 480 / 600;
                printf("y=%d\n", y);
            }
        }

        // 判断当前点击操作是否释放
        if (buf.type == EV_KEY && buf.code == BTN_TOUCH)
        {
            // 处于按键或触摸状态
            if (buf.value)
            {
                x_press = x;
                y_press = y;
                printf("x_press=%d\n", x_press);
                printf("y_press=%d\n", y_press);
            }
            else
            {
                flag = 0;
                printf("唤醒output\n");
                sem_post(&output);
            }
        }
    }
}
void *account_thread(void *arg)
{
    // 初始化Lcd
    struct LcdDevice *lcd = init_lcd();

    while (1)
    {

        // lcd_draw_bmp("account.bmp", 0, 0);

        for (int i = 800; i >= 25; i--)
        {
            account_end_flag = 0;
            lcd_draw_bmp("account_icon.bmp", 0, 0);
            char buf[] = "你好，我是秦始皇,v我50,等我复活封你做王爷!!";
            // 加载字体
            font *f = fontLoad("simfang.ttf");
            // 字体大小的设置
            fontSetSize(f, 30);
            bitmap *bm = createBitmapWithInit(775, 30, 4, getColor(0, 180, 238, 255));
            fontPrint(f, bm, -25 + i, 0, buf, getColor(0, 0, 0, 255), 0);
            show_font_to_lcd(lcd->mp, 25, 0, bm);
            // 卸载字体
            fontUnload(f);
            destroyBitmap(bm);
            if (stop_account_flag == 1)
            {
                printf("字体滚动播报线程死亡\n");
                account_end_flag = 1;
                pthread_exit(NULL);
            }
            usleep(100);
        }
    }
}
int my_exit()
{
    // 每次进来先格式化链表数据
    formatLinkList(&head);
    // 初始化Lcd
    char cur_dir[] = ".";          // 当前目录
    char up_dir[] = "..";          // 上级目录
    int x_limit = 600, x_cur = 20; // 进度条的限制
    // 使用scandir保证文件读取顺序正确
    struct dirent **entry_list;
    int file_count;
    int i = 0;
    file_count = scandir("/root/my_test/movie/", &entry_list, 0, alphasort);
    while (i < file_count)
    {
        struct dirent *file_dir;
        file_dir = entry_list[i];
        printf("file:%s\n", file_dir->d_name);
        if (strcmp(file_dir->d_name, cur_dir) != 0 && strcmp(file_dir->d_name, up_dir) != 0)
        {
            addLinkList(&head, file_dir->d_name);
        }
        free(file_dir);
        i++;
    }
    free(entry_list);
    // 播放
    LNode *p = head;
    while (p->prev != head)
    {
        char bmp_path[100] = "/root/my_test/movie/";
        char *new_bmp_path = strtok(p->file_name, "\n");
        strcat(bmp_path, new_bmp_path);
        lcd_draw_bmp(bmp_path, 0, 0);
        p = p->prev;
        usleep(10000);
    }
    formatLinkList(&head);
    return 0;
}