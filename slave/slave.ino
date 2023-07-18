#include <EEPROM.h>

char incomingByte;      // a variable to read incoming serial data into
bool alarmsSet;
static volatile uint32_t tot_overflow1 = 0, tot_overflow2 = 0, tot_overflow2Copy = 0;

void setup()
{
  Serial.begin(9600); // for serial monitor

   //POWER PIN
  pinMode(3, OUTPUT);
  digitalWrite(3, HIGH);

  //PIR PIN
  pinMode(9, INPUT);
  digitalWrite(9, HIGH);

  //BUZZER PINS
  DDRC |= 0b00001111;

  //BUZZER TEST
  PORTC |= 0b00001111;
  delay(500);
  PORTC &= 0b11110000;
  digitalWrite(3, LOW);
  /* turn on shield*/
  SIM900power();
  delay(10000);
  alarmsSet = false;
  if (EEPROM[503] == 255)
  {
    EEPROM.write(503, 0);
  }
  Counter1Init();
  Counter1Stop();
}

void loop()
{
  if (alarmsSet == false)
  {
    intrusionDetected();
  }

  serialReceive();
}

void intrusionDetected()
{
  if (digitalRead(9) == LOW)
  {
    Counter2Init();
    while (tot_overflow2Copy < 733)
    {
      tot_overflow2Copy = tot_overflow2;
    }
    if (digitalRead(9) == LOW)
    {
      PORTC |= 0b00001111;
      if (TCCR1A == 0 && TCCR1B == 0)
      {
        Serial.print('Z');
        if (EEPROM[503] == 0)
        {
          Counter1Init();
        }
      }
      delay(EEPROM[503] * 1000);
    }
  }
  else
  {
    PORTC &= 0b11110000;
    Counter2Init();
    Counter2Stop();
    tot_overflow2Copy = 0;
  }
}

void serialReceive()
{
  if (Serial.available() > 0)
  {
    incomingByte = Serial.read();
    switch (incomingByte)
    {
    case 'A'://TEST
      PORTC |= 0b00001111;
      delay(2000);
      PORTC &= 0b11110000;
      break;
    case 'B'://ALARMS ON
      PORTC |= 0b00001111;
      alarmsSet = true;
      break;
    case 'C'://ALARMS OFF
      PORTC &= 0b11110000;
      alarmsSet = false;
      break;
    case 'D'://POWER ON
      PORTC &= 0b11110000;
      alarmsSet = false;      
      if (digitalRead(3) == LOW)
      {
        PORTD |= 1 << PD3;
        Serial.print('P');
        Serial.print('J');
        delay(60000);
        /*delay(500);*/
      }
      break;
    case 'E'://POWER OFF
      if (digitalRead(3) == HIGH)
      {
        PORTD &= ~(1 << PD3);
        PORTC &= 0b11110000;
        Serial.print('P');
        alarmsSet = false;
      }     
      break;
    case 'F'://POWER TOGGLE
      PORTD ^= 1 << PD3;
      if (digitalRead(3) == HIGH)
      {
        Serial.print('P');
        Serial.print('X');
        delay(60000);
        /*delay(500);*/
      }
      else
      {
        Serial.print('p');
        Serial.print('x');
        PORTC &= 0b11110000;
        alarmsSet = false;
      }
      break;
    case 'G'://ALARM TOGGLE
      PORTC ^= 1 << PC3 | 1 << PC2 | 1 << PC1 | 1 << PC0;
      if (digitalRead(14) == HIGH)
      {
        alarmsSet = true;
        Serial.print('Y');
      }
      else
      {
        alarmsSet = false;
        Serial.print('y');
      }
      break;
    case '1':
      EEPROM.update(503, 0);
      break;
    case '2':
      EEPROM.update(503, 5);
      break;
    case '3':
      EEPROM.update(503, 10);
      break;
    case '4':
      EEPROM.update(503, 20);
      break;
    default:
      break;
    }
  }
}

void SIM900power()
// software equivalent of pressing the GSM shield "power" button
{
  PORTC |= 1 << PC4;
  delay(1000);
  PORTC &= ~(1 << PC4);
  delay(7000);
}

void Counter1Init()
{
  noInterrupts();           // disable all interrupts
  TCCR1A = 0;
  TCCR1B = 0;

  TCNT1 = 0;               // initialize timer
  TCCR1B |= (1 << CS12);    // 256 prescaler 
  TIMSK1 |= (1 << TOIE1);   // enable timer overflow interrupt
  interrupts();             // enable all interrupts
  tot_overflow1 = 0;
}

void resetCounter1()
{
  TCNT1 = 0;
  tot_overflow1 = 0;
}

ISR(TIMER1_OVF_vect)
{
  tot_overflow1++;
  if (tot_overflow1 == 120)
  {
    Counter1Stop();
  }
}

void Counter1Stop()
{
  TCCR1A = 0;
  TCCR1B = 0;
  tot_overflow1 = 0;
}




void Counter2Init()
{
  noInterrupts();           // disable all interrupts
  TCCR2A = 0;
  TCCR2B = 0;

  TCNT2 = 0;            // preload timer 65536-16MHz/256/2Hz
  TCCR2B |= (1 << CS22);    // 256 prescaler 

  TIMSK2 |= (1 << TOIE2);   // enable timer overflow interrupt
  interrupts();             // enable all interrupts
  tot_overflow2 = 0;
}

void resetCounter2()
{
  TCNT2 = 0;
  tot_overflow2 = 0;
}

ISR(TIMER2_OVF_vect)
{
  tot_overflow2++;
  if (tot_overflow2 == 2000000)
  {
    Counter2Stop();
  }
}

void Counter2Stop()
{
  TCCR2A = 0;
  TCCR2B = 0;
  tot_overflow2 = 0;
}



