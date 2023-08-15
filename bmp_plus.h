#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

typedef short WORD;
typedef int DWORD;
typedef long LONG;

typedef struct tagBITMAPFILEHEADER
{
    WORD bfType;      // 位图文件的类型，必须为BM(1-2字节）
    DWORD bfSize;     // 位图文件的大小，以字节为单位（3-6字节，低位在前）【*】
    WORD bfReserved1; // 位图文件保留字，必须为0(7-8字节）
    WORD bfReserved2; // 位图文件保留字，必须为0(9-10字节）
    DWORD bfOffBits;  // 位图数据的起始位置，以相对于位图（11-14字节，低位在前）
                     // 文件头的偏移量表示，以字节为单位
} __attribute__((packed)) BITMAPFILEHEADER;

typedef struct tagBITMAPINFOHEADER
{
    DWORD biSize;    // 本结构所占用字节数（15-18字节）
    LONG biWidth;    // 位图的宽度，以像素为单位（19-22字节）【*】
    LONG biHeight;   // 位图的高度，以像素为单位（23-26字节）【*】
    WORD biPlanes;   // 目标设备的级别，必须为1(27-28字节）
    WORD biBitCount; // 每个像素所需的位数，必须是1（双色），（29-30字节）【*】
    // 4(16色），8(256色）16(高彩色)或24（真彩色）之一
    DWORD biCompression; // 位图压缩类型，必须是0（不压缩），（31-34字节）
    // 1(BI_RLE8压缩类型）或2(BI_RLE4压缩类型）之一
    DWORD biSizeImage;    // 位图的大小(其中包含了为了补齐列数是4的倍数而添加的空字节)，以字节为单位（35-38字节）
    LONG biXPelsPerMeter; // 位图水平分辨率，每米像素数（39-42字节）
    LONG biYPelsPerMeter; // 位图垂直分辨率，每米像素数（43-46字节)
    DWORD biClrUsed;      // 位图实际使用的颜色表中的颜色数（47-50字节）
    DWORD biClrImportant; // 位图显示过程中重要的颜色数（51-54字节）
} __attribute__((packed)) BITMAPINFOHEADER;

int fd_lcd;
int *pb;

int lcd_open();
void lcd_draw_point(int x, int y, unsigned int color);
void lcd_draw_point_mydraw(int x, int y, char *color);
int lcd_draw_bmp(const char *pathname, int x, int y);
int lcd_draw_bmp_swiper(const char *pathname, int x, int y);
int lcd_draw_bmp_lock(const char *pathname, int x, int y, int end);
int lcd_close(void);

int lcd_open()
{
    fd_lcd = open("/dev/fb0", O_RDWR);
    if (fd_lcd == -1)
    {
        perror("打开lcd失败\n");
        return -1;
    }
    pb = mmap(NULL,
              800 * 480 * 4,
              PROT_READ | PROT_WRITE,
              MAP_SHARED,
              fd_lcd,
              0);
    if (pb == NULL)
    {
        perror("mmap error\n");
        return -1;
    }
    return 0;
}

void lcd_draw_point(int x, int y, unsigned int color)
{
    *(pb + y * 800 + x) = color;
}
void lcd_draw_point_mydraw(int x, int y, char *color)
{
    for(int i=y;i<y+20;i++){
        int color_buf=color[i*800*3+x*3+2]<<16|color[i*800*3+x*3+1]<<8|color[i*800*3+x*3];
        *(pb+i*800+x)=color_buf;
    }
}

void lcd_clear(unsigned int color)
{
    int x, y;
    for (int y = 479; y >= 0; y--)
    {
        for (int x = 0; x < 800; x++)
        {
            lcd_draw_point(x, y, color);
        }
    }
}

int lcd_draw_bmp(const char *pathname, int x, int y)
{
    int fd_bmp = open(pathname, O_RDWR);
    if (fd_bmp < 0)
    {
        perror("open file error\n");
        return -1;
    }
    // 位图文件头(54字节)
    BITMAPFILEHEADER file_head;
    read(fd_bmp, &file_head, sizeof(file_head));
    printf("%s图片大小:%d bytes\n", pathname, file_head.bfSize);
    // 位图信息段
    BITMAPINFOHEADER info_head;
    read(fd_bmp, &info_head, sizeof(info_head));
    printf("%s图片尺寸:宽%d 高%d\n", pathname, info_head.biWidth, info_head.biHeight);
    printf("%s图片颜色深度:%d\n", pathname, info_head.biBitCount);
    int bmp_rgb_size = info_head.biWidth * info_head.biHeight * info_head.biBitCount / 8;

    // 定义一个变长数组
    char bmp_buf[bmp_rgb_size];

    read(fd_bmp, bmp_buf, bmp_rgb_size);

    close(fd_bmp);

    // 显示图片
    // 起始坐标与结束坐标
    int x_s = x;
    int y_s = y;
    int x_e = x_s + info_head.biWidth;
    int y_e = y_s + info_head.biHeight;

    int x_pos, y_pos;

    unsigned int color;
    int i = 0;
    // 防止倒置输出
    for (y_pos = y_e - 1; y_pos >= y_s; y_pos--)
    {
        for (x_pos = x_s; x_pos < x_e; x_pos++, i += 3)
        {

            color = (bmp_buf[i + 2] << 16) | (bmp_buf[i + 1] << 8) | bmp_buf[i];
            lcd_draw_point(x_pos, y_pos, color);
        }
    }
    return 0;
}
int lcd_draw_bmp_lock(const char *pathname, int x, int y, int end)
{
    int fd_bmp = open(pathname, O_RDWR);
    if (fd_bmp < 0)
    {
        perror("open file error\n");
        return -1;
    }
    // 位图文件头(54字节)
    BITMAPFILEHEADER file_head;
    read(fd_bmp, &file_head, sizeof(file_head));
    printf("%s图片大小:%d bytes\n", pathname, file_head.bfSize);
    // 位图信息段
    BITMAPINFOHEADER info_head;
    read(fd_bmp, &info_head, sizeof(info_head));
    printf("%s图片尺寸:宽%d 高%d\n", pathname, info_head.biWidth, info_head.biHeight);
    printf("%s图片颜色深度:%d\n", pathname, info_head.biBitCount);
    int bmp_rgb_size = info_head.biWidth * info_head.biHeight * info_head.biBitCount / 8;

    // 定义一个变长数组
    char bmp_buf[bmp_rgb_size];

    read(fd_bmp, bmp_buf, bmp_rgb_size);

    close(fd_bmp);

    // 显示图片
    // 起始坐标与结束坐标
    int x_s = x;
    int y_s = y;
    int x_e = x_s + info_head.biWidth;
    int y_e = y_s + end;

    int x_pos, y_pos;

    unsigned int color;
    int i = 0;
    // 防止倒置输出
    for (y_pos = y_e - 1; y_pos >= y_s; y_pos--)
    {
        for (x_pos = x_s; x_pos < x_e; x_pos++, i += 3)
        {

            color = (bmp_buf[i + 2] << 16) | (bmp_buf[i + 1] << 8) | bmp_buf[i];
            lcd_draw_point(x_pos, y_pos, color);
        }
    }
    return 0;
}
int lcd_draw_bmp_swiper(const char *pathname, int x, int y)
{
    int fd_bmp = open(pathname, O_RDWR);
    if (fd_bmp < 0)
    {
        perror("open file error\n");
        return -1;
    }
    // 位图文件头(54字节)
    BITMAPFILEHEADER file_head;
    read(fd_bmp, &file_head, sizeof(file_head));
    printf("%s图片大小:%d bytes\n", pathname, file_head.bfSize);
    // 位图信息段
    BITMAPINFOHEADER info_head;
    read(fd_bmp, &info_head, sizeof(info_head));
    printf("%s图片尺寸:宽%d 高%d\n", pathname, info_head.biWidth, info_head.biHeight);
    printf("%s图片颜色深度:%d\n", pathname, info_head.biBitCount);
    int bmp_rgb_size = info_head.biWidth * info_head.biHeight * info_head.biBitCount / 8;

    // 定义一个变长数组
    char bmp_buf[bmp_rgb_size];

    read(fd_bmp, bmp_buf, bmp_rgb_size);

    close(fd_bmp);

    // 显示图片
    // 起始坐标与结束坐标
    int x_s = x;
    int y_s = y;
    int x_e = x_s + info_head.biWidth;
    int y_e = y_s + info_head.biHeight;

    int x_pos, y_pos;

    unsigned int color;
    int i = 0;
    // 防止倒置输出
    for (y_pos = y_e - 1; y_pos >= y_s; y_pos--)
    {
        if (y_pos <= 30)
        {
            continue;
        }
        for (x_pos = x_s; x_pos < x_e; x_pos++, i += 3)
        {

            color = (bmp_buf[i + 2] << 16) | (bmp_buf[i + 1] << 8) | bmp_buf[i];
            lcd_draw_point(x_pos, y_pos, color);
        }
    }
    return 0;
}
int lcd_close(void)
{
    if (fd_lcd > 0)
    {
        close(fd_lcd);
    }
    if (pb)
    {
        munmap(pb, 800 * 480 * 4);
    }
    return 0;
}
