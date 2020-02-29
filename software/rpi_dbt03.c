
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

#define SPILEN (5)

uint16_t transfer(int fd, const uint8_t cmd, const uint16_t par, uint8_t *result)
{
	uint8_t tx[SPILEN] = {0xff, 0x00, 0x00, 0x00, 0x00};

	tx[1]=cmd;
	tx[2]=(par & 0xff);
	tx[3]=(par >>8 )&0xff;

	uint8_t rx[SPILEN] = {0x00, 0x00, 0x00, 0x00, 0x00};
	struct spi_ioc_transfer tr = {
		.tx_buf = (unsigned long)tx,
		.rx_buf = (unsigned long)rx,
		.len = SPILEN,
		.delay_usecs = 0,
		.speed_hz = 0,
		.bits_per_word = 8,
	};

	int res = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
	if (res < 1) return -1;

	/*int n;
	printf(">");
	for (n=0; n<SPILEN; n++) {
		printf("%02x ", tx[n]);
	}
	printf("\n<");
	for (n=0; n<SPILEN; n++) {
		printf("%02x ", rx[n]);
	}
	printf("\n");*/
	if (result!=NULL) *result=rx[4];
	return rx[2] | (rx[3]<<8);
}

int set_led(int fd, const int led, const int state)
{
	if (led<0) return -1;
	if (led>2) return -1;
	return transfer(fd, 0x05, ((state&0x3f)<<2)|led, NULL); 
}

int set_tone(int fd, const int tone)
{
	if (tone<1) return -1;
	if (tone>4) return -1;
	return transfer(fd, 0x04, tone, NULL);
}

int send_octet(int fd, const int octet, uint8_t *res)
{
	if (octet<0) return -1;
	if (octet>255) return -1;
	return transfer(fd, 0x02, octet, res);
}

int get_free_octets(int fd)
{
	uint8_t res=0;
	transfer(fd, 0x06, 0x00, &res);
	return res&0x7f;
}

int read_octet(int fd, uint8_t *res)
{
	return transfer(fd,0x03, 0, res);
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

	int speed=50000;
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
		int n=0;
		int s=0;
		set_tone(fd, 1); //Silence;
		printf("Waiting for S to goLo\n");
		while (s!=0) {
			int status=set_led(fd, 1, n);
			if (n==0) n=0x3f; else n=0;
			usleep(250000);
			s=(status>>4)&0x01;
		}
		set_led(fd, 1, 0);
		set_tone(fd, 2); //440Hz 
		printf("Waiting for S to go high\n");
		while (s==0) {
			int status=set_led(fd, 0, n);
			if (n==0) n=0x3f; else n=0;
			usleep(250000);
			s=(status>>4)&0x01;
		}
		set_led(fd, 0, 0x3f);
		set_tone(fd, 2);
		usleep(1000000);
		set_tone(fd, 1);
		usleep(2000000);
		set_tone(fd, 2);
		usleep( 500000);
		set_tone(fd, 3);
		usleep(1000000);
		set_tone(fd, 4);
		set_led(fd, 0, 0);
		set_led(fd, 1, 0x15);

		usleep(2000000);
		s=1;
		n=32;
		int bf=get_free_octets(fd);
		while (s!=0) {
			int status=0;
			uint8_t res=0;
			status=read_octet(fd, &res);
			printf("read_octet res=%02x status=%04x\n", res, status);
			usleep(1000000);
			if (bf>8) {
				status=send_octet(fd, n, &res);
				printf("%c %02x %02x %02x\n", n, n, status, res);
				n=n+1;
				if (n>=32+41) n=32;
				if (res==0xff) {
					printf("Transmission error\n");
					break; //Error, hang up
				} else {
					printf("%d octets free in buffer\n", res);
					bf=res;
				}
			} else {
				bf=get_free_octets(fd);
				printf("waiting: %d octets free in buffer\n", bf);
			}
			status=set_led(fd, 0, n);
			s=(status>>4)&0x01;
			usleep(100);
		}

	}

	close(fd);

	return ret;
}
