
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <bcm2835.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h> 

#define PIN_LED_BLUE 6 
#define PIN_LED_GREEN 13
#define PIN_RESET 25

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
int open_spi();
void close_spi(int);

static const char *device = "/dev/spidev0.0";

#define SPILEN (5)

uint16_t spi_transfer(int fd, const uint8_t cmd, const uint16_t par, uint8_t *result)
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

/*	int n;
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


int set_mcu_led(int fd, const int led, const int state)
{
	if (led<0) return -1;
	if (led>2) return -1;
	return spi_transfer(fd, 0x05, ((state&0x3f)<<2)|led, NULL); 
}

int set_tone(int fd, const int tone)
{
	if (tone<1) return -1;
	if (tone>4) return -1;
	return spi_transfer(fd, 0x04, tone, NULL);
}

int send_octet(int fd, const int octet, uint8_t *res)
{
	if (octet<0) return -1;
	if (octet>255) return -1;
	return spi_transfer(fd, 0x02, octet, res);
}

int get_free_octets(int fd)
{
	uint8_t res=0;
	spi_transfer(fd, 0x06, 0x00, &res);
	return res&0x7f;
}

int read_octet(int fd, uint8_t *res)
{
	return spi_transfer(fd,0x03, 0, res);
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

void close_spi(int fd)
{
	close(fd);
}

void set_leds(int led)
{
	if ( (led>>0)!=0) 
		bcm2835_gpio_write(PIN_LED_BLUE, HIGH);
		else
		bcm2835_gpio_write(PIN_LED_BLUE, LOW);
	if ( (led>>1)!=0) 
		bcm2835_gpio_write(PIN_LED_GREEN, HIGH);
		else
		bcm2835_gpio_write(PIN_LED_GREEN, LOW);
}


/* Initialize the terminal and wait for it to start*/
int term_start(int fd)
{
	int n=0;
	int s=0;
	set_leds(0x01);
	set_tone(fd, 1); //Silence
	printf("Waiting for S to go low\n");
	//If S is high (terminal picked up) wait for it to go now)
	while (s!=0) {
		int status=set_mcu_led(fd, 1, n); //Make LED 1 on the MCU blink
		if (n==0) n=0x3f; else n=0;
		usleep(250000);
		s=(status>>4)&0x01;
	}
	set_mcu_led(fd, 1, 0); //Turn of LED 1 on the MCU
	usleep(250000);

	printf("Waiting for S to go high\n");
	while (s==0) {
		int status=set_mcu_led(fd, 0, n); //Make LED 0 on the MCU blink
		if (n==0) n=0x3f; else n=0;
		usleep(250000);
		s=(status>>4)&0x01;
	}
	set_mcu_led(fd, 0, 0); //Turn of LED 0 on the MCU
	set_tone(fd, 2); //440Hz 
	return 0;
}

/* Acknowledge the connection */
int term_constart(int fd)
{
	int status=set_mcu_led(fd, 0, 0x15);
	if (((status>>4)&0x01)==0) return -1;
	status=set_tone(fd, 2); //440 Hz
	if (((status>>4)&0x01)==0) return -1;
	usleep(1000000);
	status=set_tone(fd, 1);
	if (((status>>4)&0x01)==0) return -1;
	usleep(2000000);
	status=set_tone(fd, 2);
	if (((status>>4)&0x01)==0) return -1;
	usleep( 500000);
	status=set_tone(fd, 3);
	if (((status>>4)&0x01)==0) return -1;
	usleep(1000000);
	status=set_tone(fd, 4);
	if (((status>>4)&0x01)==0) return -1;
	status=set_mcu_led(fd, 0, 0);
	if (((status>>4)&0x01)==0) return -1;
	usleep(1000000);
	return 0;
}

int do_connect(const char *target)
{
	char addr[256];
	memset(addr, 0, sizeof(addr));
	char *colon=strchr(target, ':');
	if (colon==NULL) {
		printf("Error invalid address/port\n");
		return -1;
	}
	int alen=colon-target;
	if (alen<=0) {
		printf("Error address to short\n");
		return -1;
	}
	if (alen>=255) {
		printf("Error address to long\n");
		return -1;
	}
	strncpy(addr, target, alen);
	int port=atoi(colon+1);
	printf("Addr: %s Port: %d\n", addr, port);

	int sockfd = 0;
	struct sockaddr_in serv_addr; 

	if((sockfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) < 0)
	{
		printf("\n Error : Could not create socket \n");
		return -1;
	} 

	memset(&serv_addr, 0, sizeof(serv_addr)); 

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port); 

	if(inet_pton(AF_INET, addr, &serv_addr.sin_addr)<=0)
	{
		printf("inet_pton error occured\n");
		return -1;
	} 

	if( connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		if (errno==EINPROGRESS) return sockfd;
		printf("Error : Connect Failed  %d (%s)\n", errno, strerror(errno));
		return -1;
	} 
	return sockfd;
}


void reset_mcu()
{
	bcm2835_gpio_fsel(PIN_RESET, BCM2835_GPIO_FSEL_OUTP);
	printf("Reset MCU...");
	bcm2835_gpio_write(PIN_RESET, LOW);
	usleep(200000);
	bcm2835_gpio_write(PIN_RESET, HIGH);
	usleep(500000);
	printf("done\n");
}

int handle_socket_to_term(int fd, int sock_fd)
{
	int status=0;
	char buf[8];
	int l=read(sock_fd, buf, sizeof(buf));
	if (l<0) { //Some socket error, probably connection broke
		if (errno==EAGAIN) return 0;
		printf("Error : read Failed  %d (%s)\n", errno, strerror(errno));
		return -1;
	}
	if (l>0) {
		uint8_t res=0;
		int n;
		for (n=0; n<l; n++) {
			status=send_octet(fd, buf[n], &res);
			if ( ((status>>4)&0x01)==0) {
				return -1;
			}
		}
	}
	return 0;
}


int handle_term_to_socket(int fd, int sock_fd)
{
	uint8_t oct=0;
	int status=read_octet(fd, &oct);
	if ( ((status>>4)&0x01)==0) {
		return -1;
	}
	if (oct!=0xff) {
		char buf[1];
		buf[0]=oct;
		write(sock_fd, buf, 1);
	}
	return 0;
}

int socket_term_loop(int fd, int sock_fd)
{
	int status=0;
	while (0==0) {
		int bf=get_free_octets(fd);
		if (bf<=31) {
			status=set_mcu_led(fd, 1, 0x3f);
			if (((status>>4)&0x01)==0) return -1;
		} else {
			status=set_mcu_led(fd, 1, 0);
			if (((status>>4)&0x01)==0) return -1;
		}
		if (bf>8) {
			status=handle_socket_to_term(fd, sock_fd);
			if (status<0) return status;
		}
		status=handle_term_to_socket(fd, sock_fd);
		if (status<0) return status;
		usleep(10000);
	}
}


int main(int argc, char *argv[])
{
	int fd=open_spi();
	if (fd<0) {
		printf("couldn't open device\n");
		return 1;
	}
	if (!bcm2835_init()) {
		printf("Couldn't open BCM2835\n");
		return 1;
	}

	char c_string[128];
	char *connection=NULL;
	
	FILE *cf=fopen("/boot/btx_ip.txt","r");
	if (cf==NULL) {
		if (argc!=2) {
			printf("Usage: %s <ip>:<port>\n", argv[0]);
			return 1;
		}
		connection=argv[1];
	} else {
		memset(c_string, 0, sizeof(c_string));
		fread(c_string, sizeof(c_string)-1, 1, cf);
		connection=c_string;
		fclose(cf);
	}

	bcm2835_gpio_fsel(PIN_LED_BLUE, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_fsel(PIN_LED_GREEN, BCM2835_GPIO_FSEL_OUTP);


	term_start(fd);

	int sock_fd=do_connect(connection);
	if (fd<0) {
		reset_mcu();
		return 1;
	}

	term_constart(fd);

	
	socket_term_loop(fd, sock_fd);
	reset_mcu();

	close(fd);
	bcm2835_close();
	return 0;
}
