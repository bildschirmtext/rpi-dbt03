#define F_CPU 8000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <stdint.h>
#include <string.h>


//LEDs

#define LED_STAT_CNT (6)
//LED Modes
#define LED_ON (0x3f)
#define LED_OFF (0x00)
#define LED_BLINK (0x07)
#define LED_FAST (0x2A)
#define LED_PHASE_1 (0x24)
#define LED_PHASE_2 (LED_PHASE_1<<2)
#define LED_PHASE_3 (LED_PHASE_1<<4)

#define DIV_1200HZ (6666)
#define DIV_880HZ (9090)
#define DIV_3400HZ (2352)

uint8_t led_stat[LED_STAT_CNT];
uint8_t led_phase=0;


inline void set_term_mode(int mode);

inline void set_led_ports(uint8_t bits)
{
	PORTD=(PORTD & 0x1f) | ((bits&0x7)<<5);
}

ISR(TIMER0_OVF_vect) //Will be called with 7812 Hz
{
	sei(); //Enable Interrupts again so this interrupt can be preempted
	led_phase=led_phase+1;
	if (led_phase>=LED_STAT_CNT) led_phase=0;
	set_led_ports(led_stat[led_phase]);
}


inline void set_led_status_phase(uint8_t phase, uint8_t led, int8_t status)
{
	if (phase>=LED_STAT_CNT) return;
	uint8_t map=(1<<led);
	if (status==0) { //LED off
		led_stat[phase]=led_stat[phase] & (~map);
	} else { //LED on
		led_stat[phase]=led_stat[phase] | (map);
	}
}

inline void set_led_status(uint8_t led, int8_t status)
{
	if (led>2) return;
	int n;
	for (n=0; n<LED_STAT_CNT; n++) {
		set_led_status_phase(n, led, status & 0x01);
		status=status>>1;
	}
}

void init_led()
{
	//set LED variables
	int8_t n;
	for (n=0; n<LED_STAT_CNT; n++) led_stat[n]=0;
	//Init ports
	DDRD=DDRD | (0xe0);
	//init timer to 8MHz/1024=7812Hz
	TCCR0= (1<<CS02)|(1<<CS00);       // Start Timer 0 with prescaler 1024
	TIMSK= TIMSK|(1<<TOIE0);                // Enable Timer 0 overflow interrupt
	sei();      
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

inline uint8_t get_status()
{
	uint8_t bits=0;
	if (buffer_used(&term_to_rpi)>0) bits=bits | (1<<7);
	if (buffer_used(&rpi_to_term)<BSIZE/2) bits=bits | (1<<6);
	if (buffer_used(&rpi_to_term)<BSIZE-8) bits=bits | (1<<5);
	//PD0 low => bit 4 high
	if ((PIND&0x01)==0) bits=bits | (1<<4);
	return bits;
}

/*spi_protocol
 * MOSI $ff cmd par $ff $ff
 * SOMI und und sta stb res
 *
 * par: parameter octet
 * und: undefined
 * sta: status octet A
 * 	Bit 7: Octet from terminal
 * 	Bit 6: Free space in buffer to terminal
 * 	Bit 5: More than 8 octets free in buffer
 * 	Bit 4: S input 1=> Terminal wants to dial
 * 	Bit 0: always 1
 * stb: status octet B
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
 * cmd=0x05 set LEDs
 * 	par:
 * 	  Bit 0-1: LED Number
 * 	  Bit 2-7: LED Pattern
 */

/*spi_state 
 * 	0: idle waiting for command
 *	1-2: receive parameters
 *	3: wait for $ff
 */
#define SPILEN (4)
uint8_t spi_state=0xff;

uint8_t rx[SPILEN]; //Received octets
uint8_t tx[SPILEN]; //Octets to send

ISR(SPI_STC_vect)
{
	//read from SPI to spi_in
	uint8_t spi_in=SPDR; 
	if ( (spi_state>=SPILEN)) {
		if (spi_in==0xff) {
			SPDR=0xAA;
			spi_state=0;
			//fill status octets
			tx[0]=get_status(); //STA
			tx[1]=0x55; //STB
			tx[2]=0xAA; //Result
		}
		return;
	}

	//Store incoming byte and send outgoing byte (this is the timing critical stuff
	rx[spi_state]=spi_in;
	SPDR= tx[spi_state];
	sei(); //enable interrupts from here
	//This should be completed within a single SPI clock cycle
	

	//if first octet is in the command range go to spi_state=1
	if ((spi_state==0)) {
		uint8_t cmd=rx[0];
		if (cmd==0x03) { //read command
			int res=get_from_buffer(&term_to_rpi);
			if (res<0) {
				tx[1]=0xff;
				tx[2]=0xff;
			} else {
				tx[1]=buffer_used(&term_to_rpi);
				tx[2]=res;
			}
		}
		spi_state=1;
		return;
	}

	//Handle parameter octet then go to spi_state=2
	if (spi_state==1) {
		uint8_t cmd=rx[0];
		uint8_t par=rx[1];
		if (cmd==0x02) { //write command
			int8_t res=write_to_buffer(&rpi_to_term, spi_in);
			if (res<0) {
				tx[2]=0xff;
			} else {
				tx[2]=BSIZE-res;
			}
		}
		if (cmd==0x04) { //set tone
			set_term_mode(par);
			tx[2]=0xAA;
		}
		if (cmd==0x05) { //set LED
			set_led_status((par & 0x03), par>>2);
			tx[2]=0xAA;
		}
		if (cmd==0x06) { //read number of free octets in output buffer
			tx[2]=(BSIZE-buffer_used(&rpi_to_term))|0x80; //set bit 7 to avoid 0
		}
		spi_state=2;
		return;
	}

	if (spi_state==2){
		spi_state=3;
		return;
	}
	if (spi_state==3){
		spi_state=0xff;
		return;
	}
}

void init_spi()
{
	//init SPI
	SPCR=(1<<SPIE) | (1<<SPE);
	DDRB=DDRB| (1<<4);
}


//interface towards Terminal


int8_t term_uart_bit=-1;
int8_t term_uart_phase=0;
uint8_t term_uart_inb=0;

inline void term_in_uart(int8_t i)
{
	//If in the idle state and there is a one bit continue. Otherwise return
	if (term_uart_bit<0) {
		if (i!=0) return;
		term_uart_bit=0;
		term_uart_phase=0;
		term_uart_inb=0;
		return;
	}
	term_uart_phase=term_uart_phase+1;
	if (term_uart_phase>=16) {
		term_uart_bit=term_uart_bit+1;
		if (term_uart_bit>9) term_uart_bit=-1; //After stop bit, go to idle
		term_uart_phase=0;
	}
	if (term_uart_phase==7) { //Middle of a bit
		if (term_uart_bit==0) { //Start bit
			if (i!=0) { //if Start bit is not 0 => framing error
				term_uart_bit=-1;
				term_uart_phase=-1;
			}
		} else if (term_uart_bit==9) { //Stop bit
			if (i==1) { //if correct, write to buffer
				write_to_buffer(&term_to_rpi, term_uart_inb);				
			}
		} else { //Data bit
			term_uart_inb=(term_uart_inb >> 1) | (i<<7); 
		}
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

ISR(TIMER1_COMPA_vect)
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
		if (o<0) {
			set_ed_line(1); //Just to make sure
			return;
		}
		term_outb=o;
		//send start bit
		set_ed_line(0);
		term_state=1;
		return;
	}
	if ( (term_state>0) && (term_state<9) ) {
		//output data bit
		set_ed_line(term_outb&0x01);
		term_outb=term_outb>>1;
		term_state=term_state+1;
		return;
	}
	if (term_state>=9) {
		//send stop bit
		set_ed_line(1);
		term_state=term_state+1;
		if (term_state>11) term_state=0;
		return;
	}
}

inline void set_timer1_rate(uint16_t divider)
{
	OCR1A=divider;
	TCCR1A = 0;
	TCCR1B = (1<<WGM12) | 1; //No prescaler
	TIMSK = TIMSK | (1<<OCIE1A);
}

inline void init_term()
{
	DDRD=DDRD & (~0x03); //Set ports D0-D1 to Input
	DDRB=DDRB | 0x01; //Set port B0 as output
	term_state=-1;
	//Init Timer1
	set_timer1_rate(DIV_880HZ);
}

 /* 	mode=$01: silent  (level 0)
 * 	mode=$02: 440 Hz
 * 	mode=$03: 1700 Hz
 * 	mode=$04: set to data
 */
inline void set_term_mode(int mode)
{
	if (mode==1) { //Silent 0
		term_state=-3; //Idle low state between tones
		set_timer1_rate(DIV_880HZ);
	} else 
	if (mode==2) { //440 Hz tone
		term_state=-1; //Tone state
		set_timer1_rate(DIV_880HZ);
	} else 
	if (mode==3) { //1700 Hz tone
		term_state=-1; //Tone state
		set_timer1_rate(DIV_3400HZ);
	} else 
	if (mode==4) { //Data
		term_state=0; //Idle high between data octets
		clear_buffer(&rpi_to_term);
		clear_buffer(&term_to_rpi);
		set_timer1_rate(DIV_1200HZ);
	};
}

int main (void) 
{
	clear_buffer(&rpi_to_term);
	clear_buffer(&term_to_rpi);
	init_spi();
	init_led();
	init_term();
	DDRB |= (1 << PB0);
	set_sleep_mode(SLEEP_MODE_IDLE);
	sleep_enable();
	sei();

	set_led_status(0,0);
	set_led_status(1,0);
	set_led_status(2,0);
	
	while(1) {
		sleep_cpu();
	}

	return 0;
}

