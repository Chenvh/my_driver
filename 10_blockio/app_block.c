#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"
#include "linux/ioctl.h"

/* io 中断测试 */

int main (int argc, char *argv[])
{
    int fd;
    int ret = 0;
    //int data = 0;
    char *filename;
    unsigned char data;

    if (argc != 2) {
		printf("Error Usage!\r\n");
		return -1;
    }

    filename = argv[1];
    fd = open (filename, O_RDWR);
	if (fd < 0) {
		printf("Can't open file %s\r\n", filename);
		return -1;
	}

    while (1) {
        ret = read (fd, &data, sizeof(data));
        if (ret < 0) {
            // 读取错误或者无效
        } else { // 读数据正确
            if (data) { //读到数据
                printf("key value = %#X\r\n", data);
            }
        }
    }

    close (fd);

    return ret;

}