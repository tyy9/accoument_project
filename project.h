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

int fd_touch;
struct input_event buf;
unsigned int x, y;

int inittouch_device();
int lock_menu();
struct LcdDevice *init_lcd(const char *device);
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

struct LcdDevice *init_lcd(const char *device)
{
    // 申请空间
    struct LcdDevice *lcd = malloc(sizeof(struct LcdDevice));
    if (lcd == NULL)
    {
        return NULL;
    }

    // 打开LCD设备
    lcd->fd = open(device, O_RDWR);
    if (lcd->fd < 0)
    {
        perror("open lcd fail");
        free(lcd);
        return NULL;
    }

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
    struct LcdDevice *lcd = init_lcd("/dev/fb0");

    // 加载字体
    font *f = fontLoad("simfang.ttf");

    // 字体大小的设置
    fontSetSize(f, 30);

    // 创建一个画板（点阵图）
    // 画板宽400，高90，色深32位色即4字节，画板颜色为白色
    bitmap *bm = createBitmapWithInit(180, 90, 4, getColor(0, 0, 0, 0));

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
                        for (int y = 20; y <= 100; y++)
                        {
                            for (int x = 200 + 100 * password_index; x <= 200 + 100 * (password_index + 1); x++)
                            {
                                lcd_draw_point(x, y, 0X00FFFFFF);
                            }
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

                            char buf[] = "登录成功\n2秒后自动\n转跳";

                            // 将字体写到点阵图上
                            // f:操作的字库
                            // 70,0:字体在画板中的起始位置（y,x)
                            // buf:显示的内容
                            // getColor(0,100,100,100):字体颜色
                            // 默认为0
                            fontPrint(f, bm, 0,0, buf, getColor(0, 0, 255, 0), 0);

                            // 把字体框输出到LCD屏幕上
                            // lcd->mp:mmap后内存映射首地址
                            // 0,100:画板显示的起始位置
                            // bm:画板
                            show_font_to_lcd(lcd->mp, 20, 120, bm);
                            sleep(3);
                            lcd_draw_bmp("01.bmp", 0, 0);
                            break;
                        }
                        else
                        {
                            printf("登录失败\n");
                            index = 0;
                            memset(pw, 0, 7);
                            password_index = 0;
                            char buf[] = "登录失败\n密码有误";

                            // 将字体写到点阵图上
                            // f:操作的字库
                            // 70,0:字体在画板中的起始位置（y,x)
                            // buf:显示的内容
                            // getColor(0,100,100,100):字体颜色
                            // 默认为0
                            fontPrint(f, bm, 0,0, buf, getColor(0, 0, 0, 255), 0);

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
                            //自动清除提示
                            for (int y = 120; y <= 290; y++)
                            {
                                for (int x = 20; x <= 200; x++)
                                {
                                    lcd_draw_point(x, y, 0X00000000);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}