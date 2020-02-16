#define F_CPU 8000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include <string.h>


//LEDs

#define LED_STAT_CNT (6)
#define LED_PHASE_FACTOR (32)
//LED Modes
#define LED_ON (0x3f)
#define LED_OFF (0x00)
#define LED_BLINK (0x07)
#define LED_FAST (0x2A)
#define LED_PHASE_1 (0x24)
#define LED_PHASE_2 (LED_PHASE_1<<2)
#define LED_PHASE_3 (LED_PHASE_1<<4)



uint8_t led_stat[LED_STAT_CNT];
uint8_t led_phase=0;


inline void set_led_ports(uint8_t bits)
{
	PORTD=(PORTD & 0x1f) | (bits<<5);
}

ISR(TIMER0_OVF_vect) //Should be called with 32*6 Hz=192 Hz or so
{
	led_phase=led_phase+1;
	if (led_phase>=LED_STAT_CNT*LED_PHASE_FACTOR) led_phase=0;
	uint8_t phase=led_phase/LED_PHASE_FACTOR;
	uint8_t spha=led_phase%LED_PHASE_FACTOR;
	if (spha!=0) return;
	set_led_ports(led_stat[phase]);
}


inline void set_led_status_phase(uint8_t phase, uint8_t led, int8_t status)
{
	if (status==0) { //LED off
		led_stat[phase]=led_stat[phase] & (~(1<<led));
	}
	if (status==1) { //LED on
		led_stat[phase]=led_stat[phase] & (~(1<<led));
	}
}

inline void set_led_status(uint8_t led, int8_t status)
{
	if (led>2) return;
	int n;
	for (n=0; n<LED_STAT_CNT; n++) {
		set_led_status_phase(n, led, status & 0x01);
		status=status<<1;
	}
}

void init_led()
{
	//set LED variables
	int8_t n;
	for (n=0; n<LED_STAT_CNT; n++) led_stat[n]=0;
	//Init ports
	DDRD=DDRD | (0xe0);
	//init timer
	//Prescaler /256 
}



// Buffer
#define BSIZE (32)

typedef struct {
	uint8_t write_pointer;
	uint8_t read_pointer;
	uint8_t buffer[BSIZE];
} buffer_t;

inline void clear_buffer(buffer_t *b)
{
	if (b==NULL) return;
	memset(b, 0, sizeof(buffer_t));
}

inline int get_from_buffer(buffer_t *b)
{
	if (b->write_pointer==b->read_pointer) return -1;
	uint8_t v=b->buffer[b->read_pointer];
	b->buffer[b->read_pointer]=0;
	b->read_pointer=(b->read_pointer+1)%BSIZE;
	return v;
}


/*Returns the number of octets in the buffer */
inline int8_t buffer_used(buffer_t *b)
{
	if (b->write_pointer==b->read_pointer) return 0;
	if (b->write_pointer>b->read_pointer) return b->write_pointer-b->read_pointer;
	return b->write_pointer+BSIZE-b->read_pointer;
}

inline int8_t write_to_buffer(buffer_t *b, uint8_t v)
{
	if ((b->write_pointer+1)%BSIZE==b->read_pointer) return -1; //Buffer full
	b->buffer[b->write_pointer]=v;
	b->write_pointer=(b->write_pointer+1)%BSIZE;
	return buffer_used(b);
}


buffer_t rpi_to_term;
buffer_t term_to_rpi;

/*spi_protocol
 * MOSI cmd par $ff
 * SOMI und sta res
 *
 * par: parameter octet
 * und: undefined
 * sta: status octet
 * res: result / read data
 * cmd=$01 reset, par undefined
 * cmd=$02 write, parameter octet to Write
 * 	res=$ff: write error
 * 	res=<=BSIZE: free octets in write buffer
 * cmd=$03 read, parameter undefined, res=octet
 * cmd=$04 set tone
 * 	par=$01: silent
 * 	par=$02: 440 Hz
 * 	par=$03: 1700 Hz
 * 	par=$04: set to data
 */

/*spi_state -1: idle
 * $01-$0F: command byte received
 * $10: wait for $ff
 */
static int8_t spi_state=-1;

ISR(SPI_STC_vect)
{
	//read from SPI to spi_in
	uint8_t spi_in=SPDR; 
	uint8_t spi_out=0;
	if (spi_state==-1) {
		if (spi_in<0x10) {
			spi_state=spi_in;
		} else spi_state=0x10;
		//write status octet to SPI
	} else 
	if (spi_state==0x01) { //Reset
		clear_buffer(&rpi_to_term);
		clear_buffer(&term_to_rpi);
		spi_state=0x10; //Wait for $ff
	} else 
	if (spi_state==0x02) { //write
		int8_t res=write_to_buffer(&rpi_to_term, spi_in);
		if (res<0) {
			spi_out=0xff; //write $ff to error
		} else {
			//write BSIZE-result to SPI
			if (res>=BSIZE) spi_out=0xff;
			else spi_out=BSIZE-res;
		}
		spi_state=0x10;
	} else
	if (spi_state==0x03) { //Read
		int res=get_from_buffer(&term_to_rpi);
		if (res>=0) { //If there is data in the buffer
			//write res to 
			spi_out=res;
		} else {
			//write 0 to SPI
			spi_out=0;
		}
		spi_state=0x10; //Wait for $ff
	} else 
	if (spi_state==0x04) { //set tone
		//
		spi_state=0x10;
	} else 
	if ( (spi_state==0x10) && (spi_in==0xff) ){ //0xff when it's expected
		spi_state=-1;
	} else { //Error, handle this
	}
	//write spi_out to spi
	SPDR=spi_out;
}

void init_spi()
{
	//init SPI
}


//interface towards Terminal


uint8_t term_uart_state=0;
uint8_t term_uart_inb=0;

inline void term_in_uart(int8_t i)
{
	//If in the idle state and there is a one bit continue. Otherwise return
	if (term_uart_state==0) {
		if (i!=0) return;
		term_uart_state=0;
		term_uart_inb=0;
	}
	term_uart_state=term_uart_state+1;
	if (term_uart_state<8*16) { //data bit
		if ((term_uart_state%16)!=8) return; //If not int the middle of a bit exit
		term_uart_inb=(term_uart_inb<<1) | i;
		return;
	}
	if (term_uart_state<9*16) { //stop bit
		if ((term_uart_state%16)!=8) return; //If not int the middle of a bit exit
		if (i==0) write_to_buffer(&term_to_rpi, term_uart_inb);
		term_uart_inb=0;
		term_uart_state=0;
		return;
	}
}

int8_t term_state=0;
uint8_t term_outb=0;
/* term_state:
 * 	0: idle state 
 *	-1: beep state 1
 *	-2: beep state 2
 *	-3: high idle state (in between tones)
 */

inline void set_ed_line(uint8_t bit)
{
	PORTB=(PORTB & 0xfe) | (bit&0x01);
}

ISR(TIMER1_OVF_vect)
{
	//Read Port D1 and invert it
	term_in_uart(((PIND & 0x02)>>1)^0x01);

	//handle output
	if (term_state==-3) { //idle state in between tones
		set_ed_line(0);
		return;
	}
	if (term_state==-1) {
		//Set output bit
		set_ed_line(1);
		term_state=-2;
		return;
	}
	if (term_state==-2) {
		//Unset output bit
		set_ed_line(0);
		term_state=-1;
		return;
	}
	if (term_state==0) {
		int o=get_from_buffer(&rpi_to_term);
		if (o<0) return;
		term_outb=0;
		//send start bit
		set_ed_line(0);
		return;
	}
	if ( (term_state>0) && (term_state<9) ) {
		//output data bit
		set_ed_line(term_outb&0x01);
		term_outb=term_outb>>1;
		return;
	}
	if (term_state>=9) {
		//send stop bit
		set_ed_line(1);
		term_state=0;
		return;
	}
}

void init_term()
{
	DDRD=DDRD & (~0x03); //Set ports D0-D1 to Input
	DDRB=DDRB | 0x01; //Set port B0 as output
	//Init Timer1
}


int main (void) 
{
	clear_buffer(&rpi_to_term);
	clear_buffer(&term_to_rpi);
	init_spi();
	init_led();
	init_term();
	DDRB |= (1 << PB0);

	while(1) {
		set_led_status(0,2);
		_delay_ms(500);
	}

	return 0;
}

