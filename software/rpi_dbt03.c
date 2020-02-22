
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static void pabort(const char *s)
{
	perror(s);
	abort();
}

static const char *device = "/dev/spidev0.0";

int transfer(int fd, const uint8_t cmd, const uint8_t par, uint8_t *result)
{
	uint8_t tx[3] = {0xff, 0xFF, 0xff};

	tx[0]=cmd;
	tx[1]=par;

	uint8_t rx[3] = {0x00, 0x00, 0x00};
	struct spi_ioc_transfer tr = {
		.tx_buf = (unsigned long)tx,
		.rx_buf = (unsigned long)rx,
		.len = 3,
		.delay_usecs = 0,
		.speed_hz = 0,
		.bits_per_word = 8,
	};

	int res = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
	if (res < 1) return -1;

	int n;
	for (n=0; n<3; n++) {
		printf("%02x ", tx[n]);
	}
	printf("\n");
	for (n=0; n<3; n++) {
		printf("%02x ", rx[n]);
	}
	printf("\n");
	if (result!=NULL) *result=rx[2];
	return rx[1];
}

int set_led(int fd, const int led, const int state)
{
	if (led<0) return -1;
	if (led>2) return -1;
	return transfer(fd, 0x05, ((state&0x3f)<<2)|led, NULL); 
}

int open_spi()
{
	int fd=open(device, O_RDWR);
	if (fd<0) return fd;

	int ret=0;

	int mode=0;
	ret = ioctl(fd, SPI_IOC_WR_MODE, &mode);
	if (ret == -1) return ret;
	ret = ioctl(fd, SPI_IOC_RD_MODE, &mode);
	if (ret == -1) return ret;

	int bits=8;
	ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
	if (ret == -1) return ret;
	ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
	if (ret == -1) return ret;

	int speed=10000;
	ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
	if (ret == -1) return ret;
	ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
	if (ret == -1) return ret;
	return fd;
}

int main(int argc, char *argv[])
{
	int ret = 0;
	int fd=open_spi();
	if (fd<0) {
		printf("couldn't open device\n");
		return 1;
	}

	while (0==0) {
		int n;
		for (n=0; n<3; n++)
		{
			set_led(fd, n, 0x3f);
			usleep(1000000);
		}
		for (n=0; n<3; n++)
		{
			set_led(fd, n, 0x00);
			usleep(1000000);
		}

	}

	close(fd);

	return ret;
}
