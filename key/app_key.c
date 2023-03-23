#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"


/* 定义按键值 */
#define KEY0VALUE	0XF0
#define INVAKEY		0X00

int main(int argc, char *argv[])
{
	int fd, ret;
	char *filename;
	int keyvalue;
	int fdl;
	unsigned char led_stat = 0;
	unsigned char databuf[1];

	// 检查参数
	if(argc < 2){
		printf("Error Usage!\r\n");
		return -1;
	}

    // 打开LED
    fdl = open ("/dev/myled", O_RDWR);
	if (fdl < 0)
	{
		perror ("open led faild!\n");
		return -1;      
	}

	filename = argv[1];

	/* 打开key驱动 */
	fd = open(filename, O_RDWR);
	if(fd < 0){
		printf("file %s open failed!\r\n", argv[1]);
		return -1;
	}

	/* 循环读取按键值数据！ */
	while(1) {
		read(fd, &keyvalue, sizeof(keyvalue));
		if (keyvalue == KEY0VALUE) {	/* KEY0 */
			databuf[0] = !databuf[0];
			write (fdl, databuf, sizeof(databuf) );					
			printf("\nKEY0 Press, value = %#X\r\n", keyvalue);	/* 按下 */	

		}
	}

	ret= close(fd); /* 关闭文件 */
	if(ret < 0){
		printf("file %s close failed!\r\n", argv[1]);
		return -1;
	}

	ret = close (fdl);
	
	if (ret < 0)
	{
		perror ("close led file faild!\n");
		return -1;
	}

	return 0;
}
