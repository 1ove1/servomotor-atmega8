#include <avr/io.h>
#include <avr/interrupt.h>


// port C configuration
#define RS              PC0
#define RW              PC1
#define E               PC2
#define BTN_SWTCH       PC3
#define BTN_SWTCH_CHCK  ~PINC & (1 << BTN_SWTCH)

// some constant events codes
#define EVENT_BTN_SWTCH 8

// timer configuration
#define F_CPU           1000000
#define MS              1000
#define KOD_TIME1	      (F_CPU / 256) / MS
#define LOW_KOD_TIME1   KOD_TIME1
#define HIGH_KOD_TIME1  KOD_TIME1 >> 8

// lcd configuration
#define LCD_BUFFER      20
#define RIGHT_CAPTION   " clockwise\0         "
#define LEFT_CAPTION    " counter  \0clockwisе"
#define LCD_DELAY       3
          
// VOLATILE FIELDS
volatile uint16_t mCount1ms = 0;
volatile unsigned char dispData[LCD_BUFFER] = RIGHT_CAPTION;
volatile unsigned char eventController = 0;

// MAIN FUNCTIONS >>>>
// PORT
void port_ini (void);

// EVENT
volatile unsigned char get_swtch_event (void);

// TIMER
void timer1_ini (void);
void sync_timer1ms(uint16_t);

// LCD
void lcd_init();    
void display_str (unsigned char str[LCD_BUFFER]);

int main (void)
{
  port_ini();
  timer1_ini();
  lcd_init();

  while (1) {

    display_str(RIGHT_CAPTION);
    while (!get_swtch_event()) {
      PORTD = PORTD | (1<<0); // вращаем двигатель по часовой стрелке
      PORTD = PORTD & ~(1<<1);
    }
    
    display_str(LEFT_CAPTION);
    while (get_swtch_event()) {
      PORTD = PORTD | (1<<1); // вращаем двигатель против часовой стрелки
      PORTD = PORTD & ~(1<<0);
    }
  }
}

/*
<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
                GET/INC mCount1ms 
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
*/

volatile uint16_t get_mcount1ms(void) 
{
  return mCount1ms;
}

void inc_mcount1ms(void)
{
  if (mCount1ms < MS) {
    mCount1ms++;
  } else {
    mCount1ms = 0;
  }
}

/*
<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
                GET/SET dispData
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
*/

volatile unsigned char get_disp_data_char(unsigned char index) 
{
  return dispData[index];
}

void set_disp_data_char(unsigned char symb, unsigned char index) 
{
  dispData[index] = symb;
}

/*
<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
                GET/SET eventController
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
*/

volatile unsigned char get_event(void) 
{
  return eventController;
}

void set_event (unsigned char eventCode) 
{
  eventController = eventCode;

}


/*
<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
                PORTS FUNCTIONS
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
*/

void out_port_b (unsigned char data) 
{
  PORTB = data;
}

void out_port_c (unsigned char data) 
{
  data = data & ((1 << RS) | (1 << RW) | (1 << E));
  PORTC = PINC | data;
  data = ~data ^ ((1 << RS) | (1 << RW) | (1 << E));
  PORTC = PINC & data;
}

void out_port_d (unsigned char data) 
{
  PORTD = data;
}

void port_ini (void)
{
  // обнуляем порты
  out_port_b(0x00);
  PORTC   = 0xff;       // для положительного напряжения на портах ввода
  out_port_c(0x00);
  out_port_d(0x00);

  DDRB  = 0xFF;
  DDRC  = DDRC | ((1 << RS) | (1 << RW) | (1 << E));
  DDRC  = DDRC & ~(1 << BTN_SWTCH);
  DDRD  = DDRD | ((1 << 0) | (1 << 1)); // настраиваем порты на вывод для двигателя
}		

/*
<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
                EVENT FUNCTIONS
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
*/


volatile unsigned char get_swtch_event (void) 
{
  static unsigned char old_event;
  static unsigned char ret = 0;
  unsigned char new_event;

  new_event = get_event();

  if ((new_event & EVENT_BTN_SWTCH) && (~old_event & EVENT_BTN_SWTCH)) {
    ret = ~ret;
  }

  old_event = new_event;

  return ret;
}


/*
<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
                TIMER FUNCTIONS
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
*/


void timer1_ini (void)
{
  TCCR1B  = TCCR1B | (1<<WGM12); 
  TIMSK   = TIMSK | (1<<OCIE1A);	
  OCR1AH  = HIGH_KOD_TIME1;
  OCR1AL  = LOW_KOD_TIME1;
  TCCR1B  = TCCR1B | (1<<CS12);

  sei();
}

ISR (TIMER1_COMPA_vect)
{
  inc_mcount1ms();
  set_event( BTN_SWTCH_CHCK );
}

void sync_timer1ms (uint16_t inp_delay)
{
  if (inp_delay != 0) {
    inp_delay = (inp_delay + get_mcount1ms()) % MS;
    while(inp_delay != get_mcount1ms());
  }
}


/*
<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
                LCD DISPLAY FUNCTIONS
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
*/


void lcd_cmd (unsigned char command)  //Function to send command instruction to LCD
{
  out_port_b(command);
  out_port_c( ((0 << RS) | (0 << RW) | (1 << E)) );
  
  sync_timer1ms(LCD_DELAY);

  out_port_c( (0 << E) );

}

void lcd_data(unsigned char data)  //Function to send display data to LCD
{
  out_port_b(data);
  out_port_c( ((1 << RS) | (0 << RW) | (1 << E)) );

  sync_timer1ms(LCD_DELAY);

  out_port_c( ((1 << RS) | (0 << RW) | (0 << E)) );

}

void lcd_clr(void)
{
  lcd_cmd(0x01);  // clear screen
  lcd_cmd(0x02);  // return home
}

void lcd_init()    //Function to prepare the LCD  and get it ready
{
  lcd_cmd(0x38);  // for using 2 lines and 5X7 matrix of LCD
  lcd_cmd(0x0C);  // turn display ON
  lcd_cmd(0x01);  // clear screen
  lcd_cmd(0x81);  // bring cuRSor to position 1 of line 1
}


void lcd_show (void) 
{
  unsigned char iter = 0;
  unsigned char curr_char;

  lcd_clr();

  for (iter = 0; iter < LCD_BUFFER; iter++) {
    curr_char = get_disp_data_char(iter);

    if (curr_char == '\0') {
      lcd_cmd(0xC1);
    } else {
      lcd_data(curr_char);
    }
  }
}

void display_str (unsigned char str[LCD_BUFFER]) 
{
  unsigned char iter;

  for(iter = 0; iter < LCD_BUFFER; iter++) {
    set_disp_data_char(str[iter], iter);
  }
  lcd_show();
}