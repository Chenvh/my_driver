#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
	int ret = 0;
    int i = 0;
	unsigned char databuf[1];
	int fd = open ("/dev/myled", O_RDWR);
	if (argc < 2) 
	{
		perror ("Too few parameters\n");
		return -1;
	}

	if (fd < 0)
	{
		perror ("open faild!\n");
		return -1;
	}

	//printf ("open success!\n");

	// 控制：写数据
	databuf[0] = atoi (argv[1]);
	ret = write(fd, databuf, sizeof(databuf));
	if (ret < 0)
	{
		perror ("led contrl failed!\n");
		close (fd);
		return -1;
	}

	i = 0;
	while (1) {
		sleep (5);
		i++;
		printf ("App %d running times %d\r\n", databuf[0], i);
		if (i >= 5) break;
	}

	ret = close (fd);
	
	if (ret < 0)
	{
		perror ("close file faild!\n");
		return -1;
	}

	printf ("led file close\n");

	return 0;

}