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
typedef struct LNode
{
    char *file_name;
    struct LNode *next;
    struct LNode *prev;
} LNode, *LinkList;

// 全局变量
int fd_touch;
struct input_event buf;
unsigned int x, y;
LinkList head;
LNode *Tail; // 尾插
// 函数声明
int inittouch_device();
int lock_menu();
int main_menu();
int update_file();
struct LcdDevice *init_lcd();
int img_player();
// 链表函数
int initLinkList(LinkList *head);
int addLinkList(LinkList head, const char *file_name);
int destroyLinkList(LinkList *head);
int formatLinkList(LinkList *head); // 格式化链表
void showLinkList(LinkList head);
int addLinkListData_to_File(LinkList head);

int initLinkList(LinkList *head)
{
    *head = (LNode *)malloc(sizeof(LNode));
    if (*head == NULL)
    {
        perror("链表初始化失败\n");
        return 0;
    }
    (*head)->next = NULL;
    printf("链表初始化成功\n");
    return 1;
}

int inittouch_device()
{
    fd_touch = open("/dev/input/event0", O_RDONLY);
    if (fd_touch < 0)
    {
        perror("触摸屏打开失败\n");
        return -1;
    }
    return 0;
}

int addLinkList(LinkList head, const char *file_name)
{
    // 查找尾指针
    LNode *temp = head->next, *Tail = head;
    while (temp != NULL)
    {
        Tail = temp;
        temp = temp->next;
    }
    LNode *p = NULL;
    p = (LNode *)malloc(sizeof(LNode));
    if (p == NULL)
    {
        perror("新节点分配空间失败\n");
        return 0;
    }
    p->file_name = malloc(sizeof(file_name));
    strcpy(p->file_name, file_name);
    p->next = NULL;
    Tail->next = p;
    Tail = p;
    return 0;
}

void showLinkList(LinkList head)
{
    LNode *p = head->next;
    printf("目前链表的数据如下:\n");
    while (p != NULL)
    {
        printf("file_name:%s\n", p->file_name);
        p = p->next;
    }
}

int addLinkListData_to_File(LinkList head)
{
    FILE *fp = fopen("/root/my_test/file_list.txt", "w+");
    // 初始化Lcd
    struct LcdDevice *lcd = init_lcd("/dev/fb0");
    // 文件自动创建，每次写前都将清空
    if (fp == NULL)
    {
        perror("文件打开失败\n");
        return 0;
    }
    LNode *p = head->next;
    while (p != NULL)
    {
        printf("文件写入:%s\n", p->file_name);
        fprintf(fp, "%s\n", p->file_name);
        fflush(0);
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
    LNode *p = (*head)->next, *q = NULL;
    while (p != NULL)
    {
        q = p;
        p = p->next;
        free(q);
    }
    (*head)->next = NULL;
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
    fd_touch = open("/dev/input/event0", O_RDWR);
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
        // 读取触摸屏数据
        read(fd_touch, &buf, sizeof(buf));
        if (buf.type == EV_ABS)
        {
            // 如果事件类型为绝对位置事件，判断为触摸
            // 第一次返回X值，第二次返回Y值
            if (buf.code == ABS_X)
            {
                x = buf.value * 800 / 1024;
                printf("x=%d\n", x);
                printf("buf=%d\n", buf.value);
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
            }
            else
            {
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

                            // 将字体写到点阵图上
                            // f:操作的字库
                            // 70,0:字体在画板中的起始位置（y,x)
                            // buf:显示的内容
                            // getColor(0,100,100,100):字体颜色
                            // 默认为0
                            fontPrint(f, bm, 0, 0, buf, getColor(0, 0, 255, 0), 0);

                            // 把字体框输出到LCD屏幕上
                            // lcd->mp:mmap后内存映射首地址
                            // 0,100:画板显示的起始位置
                            // bm:画板
                            show_font_to_lcd(lcd->mp, 20, 120, bm);
                            // 卸载字体
                            fontUnload(f);
                            destroyBitmap(bm);
                            free(lcd);
                            sleep(3);
                            main_menu();
                            // lcd_draw_bmp("01.bmp", 0, 0);
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

                            // 将字体写到点阵图上
                            // f:操作的字库
                            // 70,0:字体在画板中的起始位置（y,x)
                            // buf:显示的内容
                            // getColor(0,100,100,100):字体颜色
                            // 默认为0
                            fontPrint(f, bm, 0, 0, buf, getColor(0, 0, 0, 255), 0);

                            // 把字体框输出到LCD屏幕上
                            // lcd->mp:mmap后内存映射首地址
                            // 0,100:画板显示的起始位置
                            // bm:画板
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
    }
    return 1;
}

int main_menu()
{
    unsigned int x, y;
    lcd_draw_bmp("main_menu.bmp", 0, 0);
    struct input_event buf; // 触摸屏数据结构体

    while (1)
    {
        // 读取触摸屏数据
        read(fd_touch, &buf, sizeof(buf));
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
            }
            else
            {
                if (x >= 20 * 800 / 1024 && x <= 120 * 800 / 1024)
                {
                    if (y >= 30 * 480 / 600 && y <= 100 * 480 / 600)
                    {
                        printf("更新资源\n");
                        update_file();
                    }
                }
                if (x > 120 * 800 / 1024 && x <= 220 * 800 / 1024)
                {
                    if (y >= 30 * 480 / 600 && y <= 100 * 480 / 600)
                    {
                        printf("播放器\n");
                        img_player();
                    }
                }
                if (x > 220 * 800 / 1024 && x <= 320 * 800 / 1024)
                {
                    if (y >= 30 * 480 / 600 && y <= 100 * 480 / 600)
                    {
                        printf("刮刮乐\n");
                    }
                }
                if (x >= 720 * 800 / 1024 && x <= 800 * 800 / 1024)
                {
                    if (y >= 380 * 480 / 600 && y <= 480 * 480 / 600)
                    {
                        printf("退出\n");
                        break;
                    }
                }
            }
        }
    }
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
                addLinkList(head, file_dir->d_name);
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
            char buf[30];
            memset(buf, 0, sizeof(buf));
            strcpy(buf, file_name);
            // 加载字体
            font *f = fontLoad("simfang.ttf");
            // 字体大小的设置
            fontSetSize(f, 30);
            bitmap *bm = createBitmapWithInit(500, 30, 4, getColor(1, 0, 0, 0));
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
            memset(file_name, 0, sizeof(file_name));
            usleep(1000);
        }
    }
    char buf[]="读取完毕，将自动播放图片资源\n";
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
    //lcd_draw_bmp("main_menu.bmp", 0, 0);
    return 0;
}