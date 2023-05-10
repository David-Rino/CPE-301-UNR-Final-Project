//UNR CPE 301 Final Project
//AuthorRino D., Nolan V. & Lukas L.

#include <LiquidCrystal.h>
#include <Stepper.h>
#include <dht_nonblocking.h>
#include <DS3231.h>

//Definitions
#define RDA 0x80
#define TBE 0x20  

#define DHTType DHT_TYPE_11
#define DHTPin 7

// Registers For Pins 22 - 29, 30 - 37, 7 - 13
volatile unsigned char* port_a = (unsigned char*) 0x22;
volatile unsigned char* ddr_a = (unsigned char*) 0x21;
volatile unsigned char* pin_a = (unsigned char*) 0x20;

volatile unsigned char* port_c = (unsigned char*) 0x28;
volatile unsigned char* ddr_c = (unsigned char*) 0x27;
volatile unsigned char* pin_c = (unsigned char*) 0x26;

volatile unsigned char* port_b  = (unsigned char *) 0x25;
volatile unsigned char* ddr_b   = (unsigned char *) 0x24;
volatile unsigned char* pin_b   = (unsigned char *) 0x23;

//UART Pointers
volatile unsigned char *myUCSR0A  = (unsigned char *)0x00C0;
volatile unsigned char *myUCSR0B  = (unsigned char *)0x00C1;
volatile unsigned char *myUCSR0C  = (unsigned char *)0x00C2;
volatile unsigned int  *myUBRR0   = (unsigned int *) 0x00C4;
volatile unsigned char *myUDR0    = (unsigned char *)0x00C6;

//ADC Pointers
volatile unsigned char* my_ADMUX = (unsigned char*) 0x7C;
volatile unsigned char* my_ADCSRB = (unsigned char*) 0x7B;
volatile unsigned char* my_ADCSRA = (unsigned char*) 0x7A;
volatile unsigned int* my_ADC_DATA = (unsigned int*) 0x78;

//Pin Interrupt Pointers
volatile unsigned char *myPCICR   = (unsigned char *) 0x68;
volatile unsigned char *myPCIFR   = (unsigned char *) 0x3B;
volatile unsigned char *myPCMSK0  = (unsigned char *) 0x6B;

enum STATE {IDLE = 0, RUNNING = 1, DISABLED = 2, ERROR = 3};
STATE currentState = ERROR;
STATE previousState = currentState;

//Decleration of Peripherials

//RTC MODULE
DS3231 rtc;
DateTime dt;

//LCD Display
//Note: As Described in the Demo Video unkown bug occurs when PB4 and PB7 Are Set as input pins.
//Causes the LCD display to stop working but as shown in the video when these two are turned off it runs normally.
//Could be possibly related to system interrupts but couldn't find confirmation for sure. 
LiquidCrystal lcd(52,53,51,50,49,48);

//Stepper Motor Controller
Stepper motor = Stepper(2038, 24, 23, 22, 21);

//DHT11 Sensor
DHT_nonblocking dht_sensor(DHTPin, DHTType);
float temp, humidity;

int waterlvl = 0;

void setup() {
  lcd.begin(16, 2);
  adc_init();
  U0init(9600);
  
  motor.setSpeed(2);

  rtc.setDate(__DATE__);
  rtc.setHour(__TIME__);


  setPinOutput('a', 4);
  setPinOutput('a', 5);
  setPinOutput('a', 6);
  setPinOutput('a', 7);
  setPinOutput('c', 2);

  //Important Thing of note setPinInput 4, and 7 seems to have a weird issue with the fact that it prevents the LCD from displaying, this is explained in the video.
  setPinInput('b', 4);
  setPinInput('b', 7);
  setPinInput('c', 2);

  writePin('b', 4, 1);
  writePin('b', 7, 1);
  writePin('c', 5, 1);

  *myPCICR |= 0x01;
  *myPCMSK0 |= 0b10010000;

}

void loop() {
  // put your main code here, to run repeatedly:
  waterlvl = adc_read(0);

  if(waterlvl < 100) {
    currentState = ERROR;
  }

  if(currentState != ERROR && currentState != DISABLED) {
    updateDisplay();
    U0putchar('\n');
    measuretemp(&temp, &humidity);
  }

    switch(currentState) {
    case IDLE:
      resetLed();
      writePin('a', 5, 1);
      writePin('c', 2, 0);
      break;
    case RUNNING:
      resetLed();
      writePin('a', 6, 1);
      if(temp < 10) {
        currentState = ERROR;
        break;
      }
      writePin('c', 2, 1);
      break;
    case DISABLED:
      resetLed();
      writePin('a', 4, 1);
      writePin('c', 2, 0);
      break;
    case ERROR:
      resetLed();
      lcd.print("ERROR!");
      writePin('a', 7, 1);
      writePin('c', 2, 0);
      break;
  }

  if(*pin_c & (0x01 << 5)) {
    motor.step(5);
  }


}

bool measuretemp(float* temp, float* humidity) {
  if(dht_sensor.measure(temp, humidity)) {
    return true;
  }
  return false;
}

void updateDisplay() {
  char buffer[128];
  int length;

  lcd.setCursor(0, 0);
  sprintf(buffer, "Temperature: %dC", int(temp));
  lcd.print(buffer);
  length = strlen(buffer);
  for(int i = 0; i < length; i++) {
    U0putchar(buffer[i]);
  }
  U0putchar('\n');
  

  lcd.setCursor(0, 1);
  sprintf(buffer, "Humidity: %d%%", int(humidity));
  lcd.print(buffer);
  length = strlen(buffer);
  for(int i = 0; i < length; i++) {
    U0putchar(buffer[i]);
  }
  U0putchar('\n');
}

void display() {
  char buffer[128];
  int length = strlen(buffer);
  dt = rtc.getDate();
}

ISR(PCINT0_vect) {
  if(*pin_b & 0b10000000) {
    currentState = DISABLED;
  } else if(*pin_b & (0x01 << 4)) {
    switch(currentState) {
      case IDLE:
        currentState = RUNNING;
        break;
      case RUNNING:
        currentState = IDLE;
        break;
      case DISABLED:
        currentState = IDLE;
        break;
    }
  }

}

void resetLed() {
  writePin('a', 4, 0);
  writePin('a', 5, 0);
  writePin('a', 6, 0);
  writePin('a', 7, 0);
}

//UART Functions
void U0init(int U0baud)
{
 unsigned long FCPU = 16000000;
 unsigned int tbaud;
 tbaud = (FCPU / 16 / U0baud - 1);
 *myUCSR0A = 0x20;
 *myUCSR0B = 0x18;
 *myUCSR0C = 0x06;
 *myUBRR0  = tbaud;
}

unsigned char U0kbhit()
{
  return *myUCSR0A & RDA;
}
unsigned char U0getchar()
{
  return *myUDR0;
}
void U0putchar(unsigned char U0pdata)
{
  while((*myUCSR0A & TBE) == 0) {};
  *myUDR0 = U0pdata;
}

//ADC Functions

void adc_init()
{

  *my_ADCSRA |= 0b10000000; 
  *my_ADCSRA &= 0b11011111; 
  *my_ADCSRA &= 0b11110111;
  *my_ADCSRA &= 0b11111000; 

  *my_ADCSRB &= 0b11110111; 
  *my_ADCSRB &= 0b11111000; 

  *my_ADMUX  &= 0b01111111; 
  *my_ADMUX  |= 0b01000000; 
  *my_ADMUX  &= 0b11011111; 
  *my_ADMUX  &= 0b11100000; 
}

unsigned int adc_read(unsigned char adc_channel_num)
{
  *my_ADMUX  &= 0b11100000;
  *my_ADCSRB &= 0b11110111;

  if(adc_channel_num > 7)
  {
    adc_channel_num -= 8;
    *my_ADCSRB |= 0b00001000;
  }

  *my_ADMUX  += adc_channel_num;
  *my_ADCSRA |= 0x40;

  while((*my_ADCSRA & 0x40) != 0);

  return *my_ADC_DATA;
}

//Pin IO
void setPinOutput(char pin, unsigned char pin_num) {
  if(pin == 'a') {
    *ddr_a |= 0x01 << pin_num;
  } else if (pin == 'c') {
    *ddr_c |= 0x01 << pin_num;
  } else if (pin == 'b') {
    *ddr_b |= 0x01 << pin_num;
  }
}

void setPinInput(char pin, unsigned char pin_num) {
  if(pin == 'a') {
    *ddr_a &= 0x01 << pin_num;
  } else if (pin == 'c') {
    *ddr_c &= 0x01 << pin_num;
  } else if (pin == 'b') {
    *ddr_b &= 0x01 << pin_num;
  }
}

void writePin(char pin, unsigned char pin_num, unsigned char state) {
  if(pin == 'a') {
    if(state == 0) {
      *port_a &= ~(0x01 << pin_num);
    } else {
      *port_a |= 0x01 << pin_num;
    }
  } else if (pin == 'c') {
    if(state == 0) {
      *port_c &= ~(0x01 << pin_num);
    } else {
      *port_c |= 0x01 << pin_num;
    }
  } else if (pin == 'b') {
    if(state == 0) {
      *port_b &= ~(0x01 << pin_num);
    } else {
      *port_b |= 0x01 << pin_num;
    }
  }
}
