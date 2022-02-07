#define F_CPU           1000000UL                     // частота микроконтроллера
#include <avr/io.h>
#include <avr/interrupt.h>

// ножка под PWM
#define PWM_PIN       PD0
#define PWM           (1 << PWM_PIN)
#define MAX_PWM_DELAY 90

// конфигурация порта ввода / вывода
#define RS              PC0   // линия RS (LCD)
#define RW              PC1   // линия RW (LCD)
#define E               PC2   // линия E  (LCD)
#define BTN_UP          PC3   // кнопка "UP"
#define BTN_DOWN        PC4   // кнопка "DOWN"

// события нажатия на кнопки
// позиция регистра кнопки "UP"
#define EVENT_BTN_UP    (1 << BTN_UP)             
// позиция регистра кнопки "DOWN"
#define EVENT_BTN_DOWN  (1 << BTN_DOWN)           
// проверка нажатия на кнопку "UP"
#define BTN_UP_CHCK     ~PINC & EVENT_BTN_UP      
// проверка нажатия на кнопку "DOWN"
#define BTN_DOWN_CHCK   ~PINC & EVENT_BTN_DOWN    

// конфигурация таймера
// верхнее ограничение тиков
#define MS              1000                    
// режим 256 prec/clk, сброс по совпадению
#define CONF_TIME1      (1<<WGM12) | (1<<CS12)  
// расчётное значение таймера на 1 мс
#define KOD_TIME1	      (F_CPU / 256) / MS      
// нижний регистр таймера
#define LOW_KOD_TIME1   KOD_TIME1               
// верхний регистр таймера
#define HIGH_KOD_TIME1  KOD_TIME1 >> 8          


// конфигурация ЖКИ
// кол-во разрядов на цифры
#define NUMBERS         2                             
// кол-во разрядов на буквы
#define LETTERS         13                            
// общий размер буфера
#define LCD_BUFFER      NUMBERS + LETTERS             
// стандартная надпись
#define CAPTION         " speed (ms): 50" 
// задержка чтения ЖКИ
#define LCD_DELAY       3  
// работа ЖКИ в две линии                           
#define LCD_TWO_LINES   0x38
// включение дисплея
#define LCD_DISPLAY_ON  0x0C
// очистка дисплея
#define LCD_CLEAR       0x0C
// возрат на начало
#define LCD_RETURN      0x81
// переход на новую строку
#define LCD_NEW_LINE    0xC1

// глобальные переменные (поля)
// программаня задержка синуса
volatile uint16_t pwmDelay = 50;                             
// счётчик числа переполнений таймера
volatile uint16_t mCount1ms = 0;                            
// буфер вывода
volatile unsigned char dispData[LCD_BUFFER] = CAPTION; 
// контроллера событий
volatile unsigned char eventController = 0;                 

// основные функциии и процедуры (используемые в main)
// обработка и вывод PWM сигнала
void pwm (void);

// инициализация портов
void port_ini (void);

// получение программной задержки сигнала (синус)
volatile uint16_t get_pwm_delay(void);

// система событий
// проверка нажатия на кнопки "UP" и "DOWN"
void get_up_down_event (void);                  

// таймеры
void timer1_ini (void);       // инициализация таймера T1 (16 бит)
void sync_timer1ms(uint16_t); // синхронный таймер

// ЖКИ
// инициализация ЖКИ
void lcd_init();                                  
// вывод строки
void display_str (unsigned char str[LCD_BUFFER]); 
// обновление значения задержки на экране
void display_delay (void);                        

// главная процедура
int main (void)
{
  // первичная инициализация портов, таймера и ЖКИ
  port_ini();
  timer1_ini();
  lcd_init();

  display_str(CAPTION);
  while (1) {
    pwm();
  }
}

/*
<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
                GET/INC/DEC/RESET pwmDelay
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
*/

/**
 * @brief возвращает текущее значение программной задержки сигнала
 * 
 * @return значение программной задержки (pwmDelay)
 */
volatile uint16_t get_pwm_delay()  
{
  return pwmDelay;
}

/**
 * @brief инкремент программной задержки сигнала (<MAX_pwm_DELAY)
 */
void inc_pwm_delay(void) 
{
  if(pwmDelay < MAX_PWM_DELAY) {
    pwmDelay++;
  }
}

/**
 * @brief декремент программной задержки сигнала (>0)
 */
void dec_pwm_delay(void) 
{
  if (pwmDelay > 1) {
    pwmDelay--;
  }
}

/**
 * @brief сброс программной задержки сигнала
 */
void reset_pwm_delay(void)
{
  pwmDelay = 0;
}

/*
<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
                GET/INC mCount1ms 
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
*/

/**
 * @brief возвращает текущее значение счётчика прерываний
 * 
 * @return текущее значение счётчика прерываний
 */
volatile uint16_t get_mcount1ms(void) 
{
  return mCount1ms;
}

/**
 * @brief инкремент счётчика прерываний (<MS)
 */
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

/**
 * @brief возвращает значение символа из буфера дисплея по индексу
 * 
 * @param index - позиция символа
 * @return символ из буфера на позиции index
 */
volatile unsigned char get_disp_data_char(unsigned char index) 
{
  return dispData[index];
}

/**
 * @brief устанавливает значение символа в буфер дисплея по индексу
 * 
 * @param symb - символ, который требуется занести в буфер
 * @param index - позиция символа
 */
void set_disp_data_char(unsigned char symb, unsigned char index) 
{
  dispData[index] = symb;
}

/*
<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
                GET/SET eventController
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
*/

/**
 * @brief возвращает значение контроллера событий
 * 
 * @return контроллер событий
 */
volatile unsigned char get_event(void) 
{
  return eventController;
}

/**
 * @brief задаёт события в контроллер событий
 * 
 * @param eventCode - текущие события
 */
void set_event (unsigned char eventCode) 
{
  eventController = eventCode;
}


/*
<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
                БЛОК РАБОТЫ С ПОРТАМИ
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
*/

/**
 * @brief вывод значения в порт B
 * 
 * @param data - значения для вывода
 */
void out_port_b (unsigned char data) 
{
  PORTB = data;
}

/**
 * @brief вывод значения в порт C
 * 
 * @param data - значение для вывода
 */
void out_port_c (unsigned char data) 
{
  PORTC = data;
}

/**
 * @brief вывод значения в порт D
 * 
 * @param data - значения для вывода
 */
void out_port_d (unsigned char data) 
{
    // маскируем вывод (чтобы не затронуть биты для ввода)
  data = data & PWM;
  // заносим значения в порт D
  PORTD = PORTD | data;
  // снова маскируем вывод с целью выделить нулевые значения
  data = ~data ^ PWM;
  // обнуляем (если есть) необходимые биты
  PORTD = PORTD & data;
}

/**
 * @brief инициализация портов микросхемы
 */
void port_ini (void)
{
  // создаём положительное напряжение на портах ввода
  PORTC   = 0xff;
  // обнуляем порты
  out_port_b(0x00);
  out_port_c(0x00);
  out_port_d(0x00);

  // порт B на вывод
  DDRB  = 0xFF;
  // ножки RS, RW, E порта C на вывод
  DDRC  = DDRC | ((1 << RS) | (1 << RW) | (1 << E));
  // ножки BTN_UP, BTN_DOWN порта C на ввод
  DDRC  = DDRC & ~((1 << BTN_UP) | (1 << BTN_DOWN));
  // порт D на вывод PWM
  DDRD  = DDRD | PWM;
}		

/*
<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
                БЛОК СИГНАЛА
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
*/

/**
 * @brief вывод и обработка сигнала синуса
 * 
 * @param arr - указатель на массив, содержащий данные для вывода
 * @param size - размер массива
 */
void pwm () 
{
  // значение для шиф (0 или 1)
  static unsigned char pwm_value = 0;

  // ГЕНЕРИРУЕМ ШИМ
  out_port_d(pwm_value);

  // инвертируем значение для шима
  pwm_value = ~pwm_value;

  // проверка на нажатие кнопок "UP" и "DOWN"
  get_up_down_event();

  // реализуем программную задержку для сигнала (растягиваем его)
  sync_timer1ms(get_pwm_delay());
}						  


/*
<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
                БЛОК СОБЫТИЯ
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
*/

/**
 * @brief проверка нажатия на кнопки "UP" и "DOWN"
 *  В зависимости от нажатой кнопки, увеличивает или уменьшает 
 *  значение задержки сигнала и выводит на ЖКИ
 * 
 *  Функция срабатывает только в момент нажатия кнопки, для чего
 *  используется сравнение прошлого состояния кнопки и текущего
 *  с целью выявить однократное нажатие 
 * 
 *  (CЕЙЧАС КНОПКА НАЖАТА?) && !(В ПРОШЛЫЙ РАЗ КНОПКА БЫЛА НАЖАТА?)
 *  т.е. пройдёт лишь то условие, при котором осуществилось изменение
 *  состояния кнопки с 0 на 1
 */
void get_up_down_event (void) 
{
  // старое событие (статическое)
  static unsigned char old_event;
  // новое событие
  unsigned char new_event;

  // получем текущие события
  new_event = get_event();

  // проверяем, была ли нажата кнопка "UP"
  if ((new_event & EVENT_BTN_UP) && (~old_event & EVENT_BTN_UP)) {
    // если да - увеличиваем задержку и выводим её на экран
    inc_pwm_delay();
    display_delay();
  }

  // проверяем, была ли нажата кнопка "DOWN"
  if ((new_event & EVENT_BTN_DOWN) && (~old_event & EVENT_BTN_DOWN)) {
    // если да - уменьшаем задержку и выводим её на экран
    dec_pwm_delay();
    display_delay();
  }

  // сохраняем текущее событие
  old_event = new_event;
}

/*
<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
                БЛОК ТАЙМЕРА
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
*/

/**
 * @brief инициализирует таймер Т1
 */
void timer1_ini (void)
{
  TCCR1B  = CONF_TIME1; 
  TIMSK   = TIMSK | (1<<OCIE1A);  // режим прерывания по вектору COMPA_vect
  OCR1AH  = HIGH_KOD_TIME1;
  OCR1AL  = LOW_KOD_TIME1;

  sei();                          // разрешаем прерывания
}

/**
 * @brief процедура прерывания
 *  Увеличивает счётчик прерываний и "слушает" ножки ввода (кнопки)
 */
ISR (TIMER1_COMPA_vect)
{
  // инкремент счётчика
  inc_mcount1ms();
  // "слушаем" ножки ввода (кнопик)
  set_event( (BTN_UP_CHCK) | (BTN_DOWN_CHCK) );
}

/**
 * @brief синхронный таймер на 1ms (<MS)
 *  Получает на вход задержку, приращивает 
 *  к текущему значению счётчика прерываний
 *  и ждёт, пока они не будут равны (по модулю MS)
 * 
 * @param inp_delay - значение задержки в мс
 */
void sync_timer1ms (uint16_t inp_delay)
{
  if (inp_delay != 0) {
    // осуществляем приращение задержки и счётчика тиков по модулю MS
    inp_delay = (inp_delay + get_mcount1ms()) % MS;
    // ждём, пока счётчик тиков и задержка сравняться
    while(inp_delay != get_mcount1ms());
  }
}


/*
<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
                БЛОК РАБОТЫ С ЖКИ
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
*/

/**
 * @brief вывод команды на вход ЖКИ
 * 
 * @param command - код команды
 */
void lcd_cmd (unsigned char command) 
{
  // выводим команду на порт B
  out_port_b(command);
  // включаем режим чтения команд
  PORTC = PORTC & ~((1 << RS) | (1 << RW));
  PORTC = PORTC | (1 << E);
  
  // ждём пока прочитаем
  sync_timer1ms(LCD_DELAY);

  // выключаем режим чтения команд
  PORTC = PORTC & ~(1 << E);

}

/**
 * @brief вывод данных на ввод ЖКИ
 * 
 * @param data - код символа
 */
void lcd_data(unsigned char data)
{
  // вывод символа на порт B
  out_port_b(data);
  // включаем режим чтения данных
  PORTC = PORTC & ~(1 << RW);
  PORTC = PORTC | ((1 << RS) | (1 << E));

  // ждём пока прочитает
  sync_timer1ms(LCD_DELAY);

  // выключаем режим чтения данных
  PORTC = PORTC & ~((1 << RS) | (1 << E));

}

/**
 * @brief возвращает курсор на начало
 */
void lcd_home(void)
{
  lcd_cmd(LCD_NEW_LINE);  // возврат курсора на начало
}

/**
 * @brief переход на следующую строку
 */
void lcd_new_line(void)
{
  lcd_cmd(LCD_NEW_LINE); // начать с новой стоки
}

/**
 * @brief инициализаия ЖКИ
 * 
 */
void lcd_init()  
{
  lcd_cmd(LCD_TWO_LINES);   // две линии, 5х7
  lcd_cmd(LCD_DISPLAY_ON);  // включаем дисплей
  lcd_cmd(LCD_CLEAR);       // очищаем
  lcd_cmd(LCD_RETURN);      // переводим курсор на позицию 1 линии 1
}

/**
 * @brief вывод данных из буфера на ЖКИ
 */
void lcd_show (void) 
{
  // текущая итерация
  unsigned char iter = 0;
  // текущий символ
  unsigned char curr_char;

  // переводим курсор на начало
  lcd_home();

  // цикл для каждого символа в буфере
  for (iter = 0; iter < LCD_BUFFER; iter++) {
    // получаем текущий символ из буфера
    curr_char = get_disp_data_char(iter);

    // если встречаем символ конца строки - переходим на новую строку
    if (curr_char == '\0') {
      // переходим на новую строку
      lcd_new_line();
    } else {
      // иначе выводим символ
      lcd_data(curr_char);
    }
  }
}

/**
 * @brief вывод строки на экран жки
 *  Сначала считывается новая строка, потом она заноситься в буфер
 *  после чего осуществляется процедура отображения буфера на ЖКИ
 * 
 * @param str - строка символов с длиной LCD_BUFFER
 */
void display_str (unsigned char str[LCD_BUFFER]) 
{
  // текущая итерация
  unsigned char iter;

  // цикл для каждого символа из буфера
  for(iter = 0; iter < LCD_BUFFER; iter++) {
    // обновляем значения буфера значением из ввода
    set_disp_data_char(str[iter], iter);
  }

  // отображаем буфер
  lcd_show();
}

/**
 * @brief обновление значения программной задержки сигнала
 *  В отличии от display_str не требует аргументов
 */
void display_delay(void)
{
  // текущее значение программной задержки сигнала
  unsigned char curr_pwm_delay;
  // текущая итерация
  unsigned char iter;

  // получаем значение программной задержки
  curr_pwm_delay = get_pwm_delay();

  // цикл для численного разряда буфера
  for (iter = LETTERS; iter < LCD_BUFFER; iter++) {
    // заносим старшие разряды числа задержки
    set_disp_data_char((curr_pwm_delay / 10) + '0', iter);
    // убираем старшие разряды
    curr_pwm_delay = (curr_pwm_delay % 10) * 10;        
  } 

  // запускаем процедуру отображения
  lcd_show();
}