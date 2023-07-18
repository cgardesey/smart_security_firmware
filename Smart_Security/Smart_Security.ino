#include <SoftwareSerial.h>
#include <Password.h>
#include <Keypad.h>
#include <RTClib.h>
#include <Wire.h>
#include <LiquidCrystal.h>
#include <EEPROM.h>

SoftwareSerial SIM900(7, 8); // RX, TX

LiquidCrystal lcd(13, 12, 11, 10, 9, 6);
RTC_DS1307 RTC;

const uint8_t eepromSize PROGMEM = 48;
static DateTime now;

const char keys[4][4] = {
	{ '1', '2', '3', 'A' },
	{ '4', '5', '6', 'B' },
	{ '7', '8', '9', 'C' },
	{ '*', '0', '#', 'D' }
};
byte rowPins[4] = { 2, 3, 4, 5 }, colPins[4] = { 14, 15, 16, 17 };
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, 4, 4);
Password password = Password("1111");

uint8_t tot_overflow1, h = 9, m = 0, s = 0, _tm, sel = 0, scroll_mainMenu = 0, count = 0, powerTime = 0;

static volatile char keypressed;
static uint8_t States[6];

char pass[4], confirmPass[4], incomingByte = 0, inchar; // Will hold the incoming character from the GSM shield;
bool passwordSuccessfullyChanged = false, goHome = false, autoStartSet, autoShutdownSet, systemEnabled;
bool flag = false;//acceptInputEnabled, powerTimeLeave, menuScreenCleared, passForLoopBreak
String ansString[eepromSize / 12], receivedSMS, senderNumber, balance, tempString, tm = "am";

void setup()
{
	Serial.begin(9600); // for serial monitor

	SIM900.begin(19200); // for GSM shield

	lcd.begin(20, 4);

	lcd.setCursor(3, 1);
	lcd.print(F("LOGGING ON TO"));
	lcd.setCursor(3, 2);
	lcd.print(F("NETWORK"));
	for (uint8_t i = 0; i < 6; i++)
	{
		/*delay(500 / 7);*/
		delay(18500 / 7);
		lcd.print('.');
	}
	/*delay(500 / 7);*/
	delay(18500 / 7);

	SIM900.print(F("AT+CMGF=1\r"));// set SMS mode to text
	delay(100);
	SIM900.print(F("AT+CNMI=2,2,0,0,0\r")); // blurt out contents of new SMS upon receipt to the GSM shield's serial out
	delay(100);
	if (word(EEPROM[506], EEPROM[507]) == 65535)
	{
		writePasswordToEeprom(1111, 506, 507);
	}
	passwordSet();

	//counter1 init
	noInterrupts();           // disable all interrupts
	TCCR1A = 0;
	TCCR1B = 0;

	TCNT1 = 0;                // initialize counter
	TCCR1B |= (1 << CS12);    // 256 prescaler 
	TIMSK1 |= (1 << TOIE1);   // enable timer overflow interrupt
	interrupts();             // enable all interrupts
	tot_overflow1 = 0;
	for (uint8_t i = 0; i < 4; i++)
	{
		States[i] = 0;
	}
	States[4] = 1;
	States[5] = 0;
	senderNumber = "system power on";
	SIM900.setTimeout(7000);
	sendMultipleSMS();

	Wire.begin();
	RTC.begin();
	delSms();
	autoStartSet = autoShutdownSet = false;
	lcd.clear();

	if (EEPROM[503] == 255)
	{
		EEPROM.write(503, 0);

	}
	if (EEPROM[505] != 1)
	{
		Serial.print('D');
	}
}

void loop()
{
	//balance
	lcd.setCursor(0, 0);
	lcd.print('B');
	lcd.print(balance);

	//latch
	lcd.setCursor(0, 3);
	lcd.print('L');
	lcd.print(EEPROM.read(503));

	/* auto power*/
	if (EEPROM[505] == 1)
	{
		lcd.setCursor(19, 0);
		lcd.print('A');
		if (systemEnalbleTime() > systemDisableTime())
		{
			//shutdown time
			if (!(timeNow() >= systemEnalbleTime() && timeNow() <= 2359 || timeNow() >= 0 && timeNow() < systemDisableTime()) && !autoShutdownSet)
			{
				autoShutdown();
			}
			//turn on time
			if (!(timeNow() >= systemDisableTime() && timeNow() < systemEnalbleTime()) && !autoStartSet)
			{
				autoTurnOn();
			}
		}
		else
		{
			//turn on time      
			if (!(timeNow() >= systemDisableTime() && timeNow() <= 2359 || timeNow() >= 0 && timeNow() < systemEnalbleTime()) && !autoStartSet)
			{
				autoTurnOn();
			}
			//shutdown time
			if (!(timeNow() >= systemEnalbleTime() && timeNow() < systemDisableTime()) && !autoShutdownSet)
			{
				autoShutdown();
			}
		}
	}
	else
	{
		lcd.setCursor(19, 0);
		lcd.print('M');
	}

	//intrusion notifications
	if (EEPROM[504] == 'i')
	{
		lcd.setCursor(19, 3);
		lcd.print(' ');
	}
	else
	{
		lcd.setCursor(19, 3);
		lcd.print('N');
	}

	//serial receive
	if (Serial.available() > 0)
	{
		incomingByte = Serial.read();
		switch (incomingByte)
		{
		case 'Z':
			if (EEPROM[504] != 'i')
			{
				for (uint8_t i = 0; i < 6; i++)
				{
					States[i] = 1;
				}
				senderNumber = "intrusion!";
				sendMultipleSMS();
			}
			break;
		case 'X':
			sensorsInit();
			States[0] = States[1] = States[2] = States[3] = 0;
			States[4] = 1;
			States[5] = 0;
			senderNumber = "manual system enable";
			sendMultipleSMS();
			break;
		case 'J':
			sensorsInit();
			break;
		case 'x':
			States[0] = States[1] = States[2] = States[3] = 0;
			States[4] = 0;
			States[5] = 0;
			senderNumber = "manual system disable";
			sendMultipleSMS();
			break;
		case 'Y':
			States[0] = States[1] = States[2] = States[3] = 1;
			States[4] = 1;
			States[5] = 0;
			senderNumber = "alarms on";
			sendMultipleSMS();
			break;
		case 'y':
			States[0] = States[1] = States[2] = States[3] = 0;
			States[4] = 1;
			States[5] = 0;
			senderNumber = "alarms off";
			sendMultipleSMS();
			break;
		case 'P':
			systemEnabled = true;
			break;
		case 'p':
			systemEnabled = false;
			break;
		default:
			break;
		}
	}

	now = RTC.now();
	//show timekeypressed = keypad.getKey();
	lcd.setCursor(5, 2);
	lcd.print(getHour(now.hour()));
	lcd.print(':');
	lcd.print(printDigits(now.minute()));
	lcd.print(':');
	lcd.print(printDigits(now.second()));
	lcd.print(' ');
	lcd.print(getTimeMode(now.hour()));

	keypressed = keypad.getKey();
	if (keypressed == 'A')
	{
		resetClear();
		password.reset();
		goHome = false;
		flag = false;
		while (true)
		{
			if (goHome)
			{
				break;
			}
			lcd.setCursor(2, 1);
			lcd.print(F("Enter Password:"));
			uint8_t i_p;
			for (i_p = 0; i_p < 4; i_p++)
			{
				keypressed = keypad.getKey();
				if (goBack())
				{
					flag = true;
					break;
				}

				if (tot_overflow1 > 15)
				{
					lcd.clear();
					flag = true;
					break;
				}

				if (isDigit(keypressed))
				{
					resetCounter1();
					password.append(keypressed);
					lcd.setCursor((7 + i_p), 2);
					lcd.print('#');
				}
				else
				{
					i_p--;
				}
			}

			if (flag)
			{
				flag = false;
				break;
			}

			if (password.evaluate())
			{
				goHome = true;
				password.reset();
				scroll_mainMenu = 0;
				lcd.clear();
				flag = false;
				while (1)
				{
					//menu interrupt                                                                                                         
					const String menuItemArray[8] PROGMEM = { "1. SET TIME","2. TEST ALARMS", "3. ENABLING MODE", "4. CHANGE PASSWORD", " 5. DATA CALL ", "6. NUMBER ACTION", "7. INTRUSION NTFCT", "8. ALARMS LATCH TIME" };
					if (scroll_mainMenu == 0)
					{
						MenuScreenClear();

						for (uint8_t i = 0; i < 4; i++)
						{
							lcd.setCursor((20 - menuItemArray[i].length()) / 2, i);
							lcd.print(menuItemArray[i]);
						}
					}
					else if (scroll_mainMenu == 1)
					{
						MenuScreenClear();
						for (uint8_t i = 4; i < 8; i++)
						{
							lcd.setCursor((20 - menuItemArray[i].length()) / 2, i - 4);
							lcd.print(menuItemArray[i]);
						}
					}
					keypressed = keypad.getKey();

					if (keypressed == 'B' || keypressed == 'C')
					{
						flag = false;
						resetCounter1();
						scroll_mainMenu ^= 1;
					}
					if (goBack())
					{
						break;
					}
					if (tot_overflow1 > 15)
					{
						lcd.clear();
						break;
					}
					menuItem(keypressed);
				}
			}
			else
			{
				if (goBack())
				{
					break;
				}
				if (tot_overflow1 > 15)
				{
					lcd.clear();
					break;
				}
				lcd.clear();
				lcd.setCursor(3, 1);
				lcd.print(F("Wrong Password!"));
				delayClear();
				password.reset();
			}

			if (goBack())
			{
				break;
			}
			if (tot_overflow1 > 15)
			{
				lcd.clear();
				break;
			}
		}
	}
	//manual power
	if (keypressed == '*')
	{
		Serial.print('F');
	}
	//manual alarm set
	if (keypressed == '#')
	{
		Serial.print('G');
	}
	//receive sms
	receiveSMS();
	if (receivedSMS != "" && (isMemmoryContact(senderNumber) || senderNumber == "auto-system disable" || senderNumber == "auto-system enable"))
	{
		receivedSMS.trim();
		if (receivedSMS == "a1")
		{
			Serial.print('B');
			States[0] = States[1] = States[2] = States[3] = 1;
			States[4] = 1;
			States[5] = 0;
			sendMultipleSMS();
		}
		else if (receivedSMS == "a0")
		{
			Serial.print('C');
			States[0] = States[1] = States[2] = States[3] = 0;
			States[4] = 1;
			States[5] = 0;
			sendMultipleSMS();
		}
		else if (receivedSMS == "p1")
		{
			Serial.print('D');
			States[0] = States[1] = States[2] = States[3] = 0;
			States[4] = 1;
			States[5] = 0;
			sendMultipleSMS();
		}
		else if (receivedSMS == "p0")
		{
			Serial.print('E');
			States[0] = States[1] = States[2] = States[3] = 0;
			States[4] = 0;
			States[5] = 0;
			sendMultipleSMS();
		}
		else if (receivedSMS == "b")
		{
			sendMultipleSMS();
		}
		else
		{
			dataCall('*' + receivedSMS + '#');
			lcd.clear();
			USSDcodeRunning();
			delay(1500);
			sendMultipleSMS();
		}
		receivedSMS = "";
	}
}

String printDigits(byte digits)
{
	if (digits < 10)
	{
		return ('0' + String(digits));
	}
	else
	{
		return String(digits);
	}
}

String getHour(uint8_t hour)
{
	if (hour > 12)
	{
		hour -= 12;
	}
	else if (hour == 0)
	{
		return String(hour + 12);
	}
	return printDigits(hour);
}

String getTimeMode(uint8_t hour)
{

	if (hour > 11)
	{
		return("pm");
	}
	else
	{
		return("am");
	}
}

void resetCounter1()
{
	TCNT1 = 0;
	tot_overflow1 = 0;
}

ISR(TIMER1_OVF_vect)
{
	tot_overflow1++;
	/*keypressed = keypad.getKey();*/
	if (tot_overflow1 == 250)
	{
		resetCounter1();
	}
}

void menuItem(char menuItem)
{
	if (menuItem == '1')
	{
		resetClear();
		h = getHour(now.hour()).toInt();
		m = printDigits(now.minute()).toInt();
		s = printDigits(now.second()).toInt();
		tm = getTimeMode(now.hour());
		sel = 0;
		while (1)
		{
			if (goBack())
			{
				break;
			}
			if (tot_overflow1 > 15)
			{
				lcd.clear();
				break;
			}
			keypressed = keypad.getKey();
			if (keypressed == 'B')
			{
				resetCounter1();
				if (sel == 0)
				{
					sel = 6;
				}
				else
				{
					sel--;
				}
			}
			if (keypressed == 'C')
			{
				resetCounter1();
				if (sel == 6)
				{
					sel = 0;
				}
				else
				{
					sel++;
				}
			}
			adjustHour();

			//show time
			lcd.setCursor(4, 1);
			showTimeHelper();
			lcd.print(':');
			lcd.print(printDigits(s));
			lcd.print(' ');
			lcd.print(tm);

			uint8_t timePlacements[7] = { 4, 5, 7, 8, 10, 11, 13 };
			switch (sel)
			{
			case 0:
				blinks(String(h / 10), timePlacements[sel], 1);
				break;
			case 1:
				blinks(String(h % 10), timePlacements[sel], 1);
				break;
			case 2:
				blinks(String(m / 10), timePlacements[sel], 1);
				break;
			case 3:
				blinks(String(m % 10), timePlacements[sel], 1);
				break;
			case 4:
				blinks(String(s / 10), timePlacements[sel], 1);
				break;
			case 5:
				blinks(String(s % 10), timePlacements[sel], 1);
				break;
			case 6:
				blinks(tm, timePlacements[sel], 1);
				break;
			}
			if (isDigit(keypressed))
			{
				resetCounter1();
				char key = String(keypressed).toInt();

				switch (sel)
				{
				case 0:
					if (key < 2)
					{
						h = setTens(h, key);
					}
					break;
				case 1:
					h = setOnes(h, key);
					break;
				case 2:
					if (key < 6)
					{
						m = setTens(m, key);
					}
					break;
				case 3:
					m = setOnes(m, key);
					break;
				case 4:
					if (key < 6)
					{
						s = setTens(s, key);
					}
					break;
				case 5:
					s = setOnes(s, key);
					break;
				case 6:
					setTimeMode(key);
					break;
				default:
					break;
				}
			}
			if (keypressed == 'A')
			{
				adjustHourFormat();
				RTC.adjust(DateTime(now.year(), now.month(), now.day(), h, m, s));
				now = RTC.now();
				break;
			}
		}
	}
	else if (menuItem == '2')
	{
		Serial.print('A');
	}
	else if (menuItem == '3')
	{
		resetClear();
		while (true)
		{
			lcd.setCursor(2, 1);
			lcd.print(F("1. MANUAL MODE"));
			lcd.setCursor(1, 2);
			lcd.print(F("2. AUTOMATIC MODE"));

			if (goBack())
			{
				break;
			}
			if (tot_overflow1 > 15)
			{
				lcd.clear();
				break;
			}
			keypressed = keypad.getKey();

			switch (keypressed)
			{
			case '1':
				lcd.clear();
				lcd.setCursor(3, 1);
				lcd.print(F("Manual mode"));
				activated();
				EEPROM.update(505, 0);
				delayClear();
				break;
			case '2':
				lcd.clear();
				automaticMode();
				break;
			default:
				break;
			}
		}
	}
	else if (menuItem == '4')
	{
		resetClear();
		password.reset();
		flag = false;
		while (true)
		{
			if (passwordSuccessfullyChanged)
			{
				passwordSuccessfullyChanged = false;
				break;
			}
			lcd.setCursor(0, 1);
			lcd.print(F("Enter Old Password:"));
			for (uint8_t i = 0; i < 4; i++)
			{
				keypressed = keypad.getKey();
				if (goBack())
				{
					flag = true;
					break;
				}
				if (tot_overflow1 > 15)
				{
					lcd.clear();
					flag = true;
					break;
				}
				if (isDigit(keypressed))
				{
					resetCounter1();
					password.append(keypressed);
					lcd.setCursor((7 + i), 2);
					lcd.print('#');
				}
				else
				{
					i--;
				}
			}
			if (flag)
			{
				flag = false;
				break;
			}
			if (password.evaluate())
			{
				lcd.clear();
				password.reset();
				while (true)
				{
					lcd.setCursor(0, 1);
					lcd.print(F("Enter New Password:"));
					for (uint8_t i = 0; i < 4; i++)
					{
						keypressed = keypad.getKey();
						if (goBack())
						{
							flag = true;
							break;
						}
						if (tot_overflow1 > 15)
						{
							lcd.clear();
							flag = true;
							break;
						}
						if (isDigit(keypressed))
						{
							resetCounter1();
							pass[i] = keypressed;
							lcd.setCursor((7 + i), 2);
							lcd.print('#');
						}
						else
						{
							i--;
						}
					}
					if (flag)
					{
						flag = false;
						break;
					}

					lcd.clear();
					lcd.setCursor(0, 1);
					lcd.print(F("Confirm New Password"));
					for (uint8_t i = 0; i < 4; i++)
					{
						keypressed = keypad.getKey();
						if (goBack())
						{
							flag = true;
							break;
						}
						if (tot_overflow1 > 15)
						{
							lcd.clear();
							flag = true;
							break;
						}
						if (isDigit(keypressed))
						{
							resetCounter1();
							confirmPass[i] = keypressed;
							lcd.setCursor((7 + i), 2);
							lcd.print('#');
						}
						else
						{
							i--;
						}
					}
					if (flag)
					{
						flag = false;
						break;
					}

					if (String(pass).substring(0, 4) == String(confirmPass).substring(0, 4))
					{
						lcd.clear();
						lcd.setCursor(5, 1);
						lcd.print(F("Password"));
						lcd.setCursor(0, 2);
						lcd.print(F("successfully changed"));
						writePasswordToEeprom(String(confirmPass).substring(0, 4).toInt(), 506, 507);
						passwordSet();
						delayClear();
						passwordSuccessfullyChanged = true;
						break;
					}
					else
					{
						lcd.clear();
						lcd.setCursor(1, 1);
						lcd.print(F("Passwords do not"));
						lcd.setCursor(6, 2);
						lcd.print(F("match"));
						delayClear();
						password.reset();
					}
				}

			}
			else
			{
				lcd.clear();
				lcd.setCursor(3, 1);
				lcd.print(F("Wrong Password!"));
				delayClear();
				password.reset();
			}
		}
	}
	else if (menuItem == '5')
	{
		tempStringInit();
		flag = true;
		h = 0, m = 0, s = 0;

		while (true)
		{
			if (tempString == "")
			{
				lcd.setCursor(4, 1);
				lcd.print("Enter code:");
			}
			if (!flag && tempString != "")
			{
				if (m < 3)
				{
					for (uint8_t i = s; i < tempString.length(); i++)
					{
						if ((tempString.charAt(i) == '\n' && m == 3) || m == 4)
						{
							s = i;
							break;
						}
						if (tempString.charAt(i) == '\n')
						{
							if (h != 0)
							{
								m++;
								h = 0;
							}
							continue;
						}
						if (tempString.charAt(i) == ' ' && h == 0)
						{
							continue;
						}

						lcd.setCursor(h, m);
						lcd.print(tempString.charAt(i));
						if (h == 19)
						{
							m++;
							h = 0;
						}
						else
						{
							h++;
						}
						if (i == tempString.length() - 1)
						{
							s = 0;
							m = 4;
							break;
						}
					}
				}
			}
			if (goBack())
			{
				break;
			}
			if (tot_overflow1 > 15)
			{
				lcd.clear();
				break;
			}
			keypressed = keypad.getKey();
			if ((isDigit(keypressed) || keypressed == '*' || keypressed == '#'))
			{
				if (!flag)
				{
					tempString = "";
				}
				lcd.clear();
				addCharHelper();
				flag = true;
			}
			if (keypressed == 'C' && flag == true)
			{
				removeCharHelper();
			}
			if (keypressed == 'B' && flag == false)
			{
				resetClear();
				h = 0;
				m = 0;
			}
			if (keypressed == 'A' && tempString.length() > 0 && flag == true)
			{
				resetClear();
				dataCall(tempString);
				lcd.clear();
				USSDcodeRunning();
				if (SIM900.find("+CUSD"))
				{
					tempString = "";
					while (inchar != '"')
					{
						delay(10);
						inchar = SIM900.read();
						if (inchar != '"')
						{
							tempString += inchar;
						}
					}
				}
				else
				{
					requestFailed();
					return;
				}
				/*tempString = "Your a/c is GHC0.11,SMS a/c is 0.25GHC,Bonus a/c is 0.00GHC,J4u a/c is 0.00GHC,IDD Bundle is 0.00GHC,Xtra-time a/c is 0.00GHC,internet bundle is 0.00MB.BOR a";*/
				flag = false;
				h = 0;
				m = 0;
				s = 0;

			}
			if (flag)
			{
				lcd.setCursor(0, 0);
				lcd.print(tempString);
			}
		}
	}
	else if (menuItem == '6')
	{
		resetClear();
		while (true)
		{
			lcd.setCursor(2, 0);
			lcd.print(F("1. ADD NUMBER"));
			lcd.setCursor(2, 1);
			lcd.print(F("2. LIST NUMBERS"));
			lcd.setCursor(2, 2);
			lcd.print(F("3. DELETE NUMBER"));

			keypressed = keypad.getKey();
			if (goBack())
			{
				break;
			}
			if (tot_overflow1 > 15)
			{
				lcd.clear();
				break;
			}

			menuItem_numberAction(keypressed);
		}
	}
	else if (menuItem == '7')
	{
		resetClear();
		lcd.setCursor(0, 0);
		lcd.print(F("1. INTRU. NTFCT ON"));
		lcd.setCursor(0, 1);
		lcd.print(F("2. INTRU. NTFCT OFF"));
		while (true)
		{
			if (goBack())
			{
				break;
			}
			if (tot_overflow1 > 15)
			{
				lcd.clear();
				break;
			}
			if (keypressed == '1')
			{
				intrusionNTFCThelper('I');
				lcd.print(F("enabled"));
				delayClear();
				break;
			}
			if (keypressed == '2')
			{
				intrusionNTFCThelper('i');
				lcd.print(F("disabled"));
				delayClear();
				break;
			}
			keypressed = keypad.getKey();
		}
	}
	else if (menuItem == '8')
	{
		resetClear();
		lcd.setCursor(2, 0);
		lcd.print(F("1.  0 seconds"));
		lcd.setCursor(2, 1);
		lcd.print(F("2.  5 seconds"));
		lcd.setCursor(2, 2);
		lcd.print(F("3. 10 seconds"));
		lcd.setCursor(2, 3);
		lcd.print(F("4. 20 seconds"));
		while (true)
		{
			if (goBack())
			{
				break;
			}
			if (tot_overflow1 > 15)
			{
				lcd.clear();
				break;
			}

			if (keypressed == '1')
			{
				latchTimeUpdate(0, keypressed);
				break;
			}
			if (keypressed == '2')
			{
				latchTimeUpdate(5, keypressed);
				break;
			}
			if (keypressed == '3')
			{
				latchTimeUpdate(10, keypressed);
				break;
			}
			if (keypressed == '4')
			{
				latchTimeUpdate(20, keypressed);
				break;
			}
			keypressed = keypad.getKey();
		}
	}
}

void menuItem_numberAction(char menuItem)
{
	switch (menuItem)
	{
	case '1':
		if (systemMemmoryFull())
		{
			systemMemmoryFullScreen();
			return;
		}
		tempStringInit();
		contactScreen();
		while (true)
		{
			if (goBack())
			{
				break;
			}
			if (tot_overflow1 > 15)
			{
				lcd.clear();
				break;
			}
			keypressed = keypad.getKey();
			if (isDigit(keypressed) && tempString.length() < 12)
			{
				addCharHelper();
			}
			if (keypressed == 'C')
			{
				removeCharHelper();
				contactScreen();
			}
			if (keypressed == 'A' && tempString.length() > 0)
			{
				resetCounter1();
				if (systemMemmoryFull())
				{
					systemMemmoryFullScreen();
					return;
				}
				if (numberExist('+' + tempString))
				{
					lcd.clear();
					lcd.setCursor(2, 1);
					lcd.print(F("Number already"));
					lcd.setCursor(5, 2);
					lcd.print(F("exists!"));
					delayClear();
					contactScreen();
				}
				else
				{
					for (uint8_t i = 0; i < eepromSize; i += 12)
					{
						if (EEPROM[i] == 255)
						{
							for (uint8_t j = 0; j < tempString.length(); j++)
							{
								EEPROM[i + j] = tempString.charAt(j);
							}
							lcd.clear();
							lcd.setCursor(6, 1);
							lcd.print(F("Number"));
							lcd.setCursor(1, 2);
							lcd.print(F("successfully added"));
							delayClear();
							while (true)
							{
								keypressed = keypad.getKey();
								listAlarmTimeScreen();
								if (goBack())
								{
									tempString = "";
									contactScreen();
									keypressed = NO_KEY;
									break;
								}
								if (tot_overflow1 > 15)
								{
									lcd.clear();
									break;
								}
							}
							break;
						}
					}
				}
			}
			lcd.setCursor(3, 2);
			lcd.print(tempString);
		}
		break;

	case '2':
		resetClear();
		while (true)
		{
			if (!eepromEmpty())
			{
				listAlarmTimeScreen();
				if (tot_overflow1 > 15)
				{
					lcd.clear();
					return;
				}
				if (goBack())
				{
					resetClear();
					return;
				}
			}
			else
			{
				lcd.clear();
				alarmTimeListEmptyScreen();
				break;
			}
			keypressed = keypad.getKey();
			if (goBack())
			{
				break;
			}
			if (tot_overflow1 > 15)
			{
				lcd.clear();
				break;
			}
		}
		break;

	case '3':
		resetClear();
		sel = 0;
		if (!eepromEmpty())
		{
			while (1)
			{
				listAlarmTimeScreen();
				blinks(ansString[sel], 4, sel % 4);
				keypressed = keypad.getKey();
				if (keypressed == 'A')
				{
					resetClear();
					for (uint8_t i = 0; i < 12; i++)
					{
						EEPROM[getSelection() + i] = 255;
					}

					if (eepromEmpty())
					{
						numberListIs();
						lcd.setCursor(4, 2);
						lcd.print(F("now empty!"));
						delayClear();
						break;
					}

					while (true)
					{
						listAlarmTimeScreen();
						keypressed = keypad.getKey();
						if (goBack())
						{
							sel = 0;
							keypressed = NO_KEY;
							break;
						}
						if (tot_overflow1 > 15)
						{
							lcd.clear();
							break;
						}
					}
				}

				//moveUpDown
				if (keypressed == 'B')
				{
					resetCounter1();
					if (sel == 0)
					{
						sel = upperLimit();
					}
					else
					{
						sel--;
					}
				}
				if (keypressed == 'C')
				{
					resetCounter1();

					if (sel == upperLimit())
					{
						sel = 0;
					}
					else
					{
						sel++;
					}

				}
				if (goBack())
				{
					break;
				}
				if (tot_overflow1 > 15)
				{
					lcd.clear();
					break;
				}
			}
		}
		else
		{
			lcd.clear();
			alarmTimeListEmptyScreen();
			break;
		}
		break;
	default:
		break;
	}
}

void blinks(String text, uint8_t x, uint8_t y)
{
	if (count % 2)
	{
		lcd.setCursor(x, y);
		lcd.print(text);
	}
	else
	{
		lcd.setCursor(x, y);
		lcd.print(getWhiteSpaces(text));
	}
	count++;
	delay(250);
}

String getWhiteSpaces(String input)
{
	String result;
	for (uint8_t i = 0; i < input.length(); i++)
	{
		result += ' ';
	}
	return result;
}

uint8_t setTens(uint8_t timeParam, uint8_t key)
{
	return  (10 * key + timeParam % 10);
}

uint8_t setOnes(uint8_t timeParam, uint8_t key)
{
	uint8_t temp = timeParam / 10;
	return  (10 * temp + key);
}

void automaticMode()
{

	if (systemEnalbleTime() == 65535)
	{
		writePasswordToEeprom(2200, 510, 511);
	}
	automaticModeHelper(systemEnalbleTime());
	powerTime = 0;
	while (true)
	{
		keypressed = keypad.getKey();
		lcd.setCursor(0, 1);
		lcd.print(F("System enable time:"));
		selectScroll_PO();
		if (goBack())
		{
			return;
		}
		if (flag)
		{
			break;
		}
	}

	if (systemDisableTime() == 65535)
	{
		writePasswordToEeprom(500, 508, 509);
	}
	automaticModeHelper(systemDisableTime());
	powerTime = 1;
	while (true)
	{
		keypressed = keypad.getKey();
		lcd.setCursor(0, 1);
		lcd.print(F("System disable time:"));
		selectScroll_PO();
		if (goBack())
		{
			return;
		}
		if (flag)
		{
			break;
		}
	}

	lcd.setCursor(2, 1);
	lcd.print(F("Automatic mode"));
	activated();
	EEPROM.update(505, 1);
	delayClear();
}

String getTime(int param)
{
	return(getHour(param / 100) + ':' + printDigits(param % 100) + ' ' + getTimeMode(param / 100));
}

boolean eepromEmpty()
{
	for (uint8_t i = 0; i < eepromSize; i += 12)
	{
		if (EEPROM[i] != 255)
		{
			return false;
		}
	}
	return true;
}

void alarmTimeListEmptyScreen()
{
	numberListIs();
	lcd.setCursor(6, 2);
	lcd.print(F("empty!"));
	delayClear();
}

void eepromConvert()
{
	//reset ans string
	for (uint8_t i = 0; i < eepromSize / 12; i++)
	{
		ansString[i] = "+";
	}
	//convert
	for (uint8_t i = 0, j = 0; i < eepromSize; i += 12, j += 12)
	{
		if (EEPROM[i] == 255)
		{
			j -= 12;
			continue;
		}
		for (uint8_t k = 0; k < 12; k++)
		{
			if (EEPROM[i + k] == 255)
			{
				continue;
			}
			ansString[j / 12] += (char)(EEPROM[i + k]);
		}
	}
}

void listAlarmTimeScreen()
{
	eepromConvert();
	for (uint8_t i = 0, j = 0; i < 4; i++, j++)
	{
		if (ansString[i] == "+")
		{
			j--;
			continue;
		}
		lcd.setCursor(4, j % 4);
		lcd.print(ansString[i]);
	}
}

void passwordSet()
{
	tempString = String(word(EEPROM[506], EEPROM[507]));
	tempString.toCharArray(pass, tempString.length() + 1); //convert string to char array
	password.set(pass);
}

void writePasswordToEeprom(int param, int highByteAddr, int lowByteAddr)
{
	EEPROM.write(highByteAddr, highByte(param));
	EEPROM.write(lowByteAddr, lowByte(param));
}

uint8_t getSelection()
{
	for (uint8_t i = 0; i < eepromSize; i += 12)
	{
		tempString = "+";
		for (uint8_t j = 0; j < 12; j++)
		{
			if (EEPROM[i + j] == 255)
			{
				continue;
			}
			tempString += (char)(EEPROM[i + j]);
		}
		if (ansString[sel] == tempString)
		{
			return i;
		}
	}
}

boolean systemMemmoryFull()
{
	for (uint8_t i = 0; i < eepromSize; i += 12)
	{
		if (EEPROM[i] == 255)
		{
			return false;
		}
	}
	return true;
}

boolean numberExist(String number)
{
	eepromConvert();
	for (uint8_t i = 0; i < 4; i++)
	{
		if (ansString[i] == number)
		{
			return true;
		}
	}
	return false;
}

void sendMultipleSMS()
{
	float b = getCreditBalance().toFloat();
	eepromConvert();
	lcd.clear();
	lcd.setCursor(3, 1);
	lcd.print(F("SENDING"));
	lcd.setCursor(3, 2);
	lcd.print(F("NOTIFICATION..."));
	for (uint8_t i = 0; i < 4; i++)
	{
		if (ansString[i] == "+")
		{
			continue;
		}
		sendSMS(b, ansString[i]);
	}
	lcd.clear();
}

void sendSMS(float B, String U)
{
	U.trim();
	SIM900.print(F("AT+CMGF=1\r")); // AT command to send SMS message
	delay(100);
	SIM900.println("AT + CMGS = \"" + U + "\"");  // recipient's mobile number,
	delay(100);

	//print sms
	//SIM900.println("{"S":{"A":[0, 1, 1, 1],"P" : 0,"I" : 1,"B" : 1.2,"U" : "2554441","V" : 12}}"); // message to send
	SIM900.print("{\"S\":{\"A\":[");
	SIM900.print(States[0]);
	SIM900.print(',');
	SIM900.print(States[1]);
	SIM900.print(',');
	SIM900.print(States[2]);
	SIM900.print(',');
	SIM900.print(States[3]);
	SIM900.print("],\"P\":");
	SIM900.print(States[4]);
	SIM900.print(",\"I\":");
	SIM900.print(States[5]);
	SIM900.print(",\"B\":");
	SIM900.print(B);
	SIM900.print(",\"U\":\"");
	SIM900.print(senderNumber);
	SIM900.print("\",\"V\":");
	SIM900.print(EEPROM[505] % 255);
	SIM900.println("}}");

	delay(100);
	SIM900.println((char)26); // End AT command with a ^Z, ASCII code 26
	delay(100);
	SIM900.println();
	delay(5000); // give module time to send SMS  
}

void receiveSMS()
{
	if (SIM900.available() > 0)
	{
		inchar = SIM900.read();
		delay(10);
		if (inchar == '+')
		{
			inchar = SIM900.read();
			delay(10);
			if (inchar == 'C')
			{
				inchar = SIM900.read();
				delay(10);
				if (inchar == 'M')
				{
					inchar = SIM900.read();
					delay(10);
					if (inchar == 'T')
					{
						inchar = SIM900.read();
						delay(10);
						if (inchar == ':')
						{
							inchar = SIM900.read();
							delay(10);
							if (inchar == ' ')
							{
								inchar = SIM900.read();
								delay(10);
								if (inchar == '"')
								{
									inchar = SIM900.read();
									delay(10);
									if (inchar == '+')
									{
										inchar = SIM900.read();
										delay(10);
										senderNumber = "+";
										while (inchar != '"')
										{
											senderNumber += inchar;
											inchar = SIM900.read();
											delay(10);
										}
										while (inchar != '*')
										{
											inchar = SIM900.read();
											delay(10);
										}
										receivedSMS = "";
										while (inchar != '#')
										{
											inchar = SIM900.read();
											if (inchar != '#')
											{
												receivedSMS += inchar;
											}
											delay(10);
										}
										delSms();
										return;
									}
								}
							}
						}
					}
				}
			}
		}
	}
}

void delSms()
{
	SIM900.print(F("AT+CMGD=ALL\r\n")); // delete all message from the inbox
	delay(100);
	//SIM900.println(F("AT+CMGD=1,4")); // delete all SMS
}


String getCreditBalance()
{
	dataCall("*124#");
	lcd.clear();
	lcd.setCursor(5, 1);
	lcd.print(F("CHECKING"));
	lcd.setCursor(5, 2);
	lcd.print(F("BALANCE..."));
	if (SIM900.find("GHC"))
	{
		balance = "";
		while (inchar != ',')
		{
			delay(10);
			inchar = SIM900.read();
			if (inchar != ',')
			{
				balance += inchar;
			}
		}
		return(balance);
	}
	else
	{
		requestFailed();
		return  balance;
	}
}

void dataCall(String code)
{
	/*SIM900.println("AT+CUSD = 1, \"" + code + "\"\r");*/
	SIM900.print("AT+CUSD = 1, \"");
	SIM900.print(code);
	SIM900.println("\"\r");
	delay(100);
}

int timeNow()
{
	now = RTC.now();
	return (100 * now.hour() + now.minute());
}

boolean goBack()
{
	if (keypressed == 'D')
	{
		resetClear();
		return true;
	}
	return false;
}

void selectScroll_PO()
{
	lcd.setCursor(5, 2);
	showTimeHelper();
	lcd.print(' ');
	lcd.print(tm);
	const uint8_t timePlacements_PO[5] = { 5, 6, 8, 9, 11 };
	adjustHour();
	switch (sel)
	{
	case 0:
		blinks(String(h / 10), timePlacements_PO[sel], 2);
		break;
	case 1:
		blinks(String(h % 10), timePlacements_PO[sel], 2);
		break;
	case 2:
		blinks(String(m / 10), timePlacements_PO[sel], 2);
		break;
	case 3:
		blinks(String(m % 10), timePlacements_PO[sel], 2);
		break;
	case 4:
		blinks(tm, timePlacements_PO[sel], 2);
		break;
	}
	keypressed = keypad.getKey();
	if (isDigit(keypressed))
	{
		resetCounter1();
		char key = String(keypressed).toInt();

		switch (sel)
		{
		case 0:
			if (key < 2)
			{
				h = setTens(h, key);
			}
			break;
		case 1:
			h = setOnes(h, key);
			break;
		case 2:
			if (key < 6)
			{
				m = setTens(m, key);
			}
			break;
		case 3:
			m = setOnes(m, key);
			break;
		case 4:
			setTimeMode(key);
			break;
		default:
			break;
		}
	}
	else if (keypressed == 'B')
	{
		resetCounter1();
		if (sel == 0)
		{
			sel = 4;
		}
		else
		{
			sel--;
		}
	}
	else if (keypressed == 'C')
	{
		resetCounter1();

		if (sel == 4)
		{
			sel = 0;
		}
		else
		{
			sel++;
		}
	}
	else if (keypressed == 'A')
	{
		adjustHourFormat();

		int y = 100 * h + m;
		switch (powerTime)
		{
		case 0:
			EEPROM.update(510, highByte(y));
			EEPROM.update(511, lowByte(y));
			lcd.clear();
			lcd.setCursor(1, 1);
			lcd.print(F("System enable time"));
			break;
		case 1:
			EEPROM.update(508, highByte(y));
			EEPROM.update(509, lowByte(y));
			lcd.clear();
			lcd.setCursor(0, 1);
			lcd.print(F("System disable time"));
			break;
		default:
			break;
		}
		lcd.setCursor(1, 2);
		successfullySet();
		delayClear();
		flag = true;
		return;
	}
}

boolean isMemmoryContact(String contact)
{
	eepromConvert();
	for (uint8_t i = 0; i < 4; i++)
	{
		ansString[i];
		if (ansString[i] == contact)
		{
			return true;
		}
	}
	return false;
}

void sensorsInit()
{
	lcd.clear();
	lcd.setCursor(2, 1);
	lcd.print(F("STARTING SENSORS"));
	lcd.setCursor(2, 2);
	for (uint8_t i = 0; i < 16; i++)
	{
		delay(3529);
		/*delay(31);*/
		lcd.print('*');
	}
	delay(3536);
	/*delay(31);*/
	lcd.clear();
}

uint8_t upperLimit()
{
	eepromConvert();

	for (uint8_t i = 0; i < eepromSize / 12; i++)
	{
		if (ansString[i] == "+")
		{
			return i - 1;
		}
	}
	return 3;
}

void contactScreen()
{
	lcd.setCursor(1, 1);
	lcd.print(F("Enter contact no.:"));
}

void resetClear()
{
	resetCounter1();
	lcd.clear();
}

void autoShutdown()
{
	if (systemEnabled)
	{
		receivedSMS = "p0";
		senderNumber = "auto-system disable";
	}

	autoShutdownSet = true;
	autoStartSet = false;
}

void autoTurnOn()
{
	if (!systemEnabled)
	{
		receivedSMS = "p1";
		senderNumber = "auto-system enable";
	}
	autoStartSet = true;
	autoShutdownSet = false;
}

void automaticModeHelper(int x)
{
	h = (getTime(x).substring(0, 2)).toInt();
	m = (getTime(x).substring(3, 5)).toInt();
	tm = getTime(x).substring(6);
	flag = false;
	sel = 0;
}

void delayClear()
{
	delay(1500);
	lcd.clear();
}

uint16_t systemEnalbleTime()
{
	return(word(EEPROM[510], EEPROM[511]));
}

uint16_t systemDisableTime()
{
	return(word(EEPROM[508], EEPROM[509]));
}

void adjustHour()
{
	if (h > 12)
	{
		h = 12;
	}
	if (h == 0)
	{
		h = 1;
	}
}

void adjustHourFormat()
{
	if (tm == "pm" && h != 12)
	{
		h += 12;
	}
	if (tm == "am" && h == 12)
	{
		h = 0;
	}
}

void setTimeMode(int key)
{
	tm = (key == 1) ? "am" : "pm";
}

void systemMemmoryFullScreen()
{
	lcd.clear();
	lcd.setCursor(2, 1);
	lcd.print(F("System memmory"));
	lcd.setCursor(7, 2);
	lcd.print(F("full!"));
	delayClear();
}

void numberListIs()
{
	lcd.setCursor(2, 1);
	lcd.print(F("Number list is"));
}

void addCharHelper()
{
	resetCounter1();
	tempString += keypressed;
}

void removeCharHelper()
{
	resetClear();
	tempString.remove(tempString.length() - 1);
}

void tempStringInit()
{
	resetClear();
	tempString = "";
}

void latchTimeUpdate(uint8_t latch, char i)
{
	EEPROM.update(503, latch);
	Serial.print(i);
	lcd.clear();
	lcd.setCursor(1, 0);
	lcd.print(F("Alarm latch time"));
	lcd.setCursor(1, 1);
	successfullySet();
	lcd.setCursor(1, 2);
	lcd.print(F("to "));
	lcd.print(latch);
	lcd.print(F(" seconds"));
	delayClear();
}

void intrusionNTFCThelper(char i)
{
	EEPROM.update(504, i);
	Serial.print(i);
	lcd.clear();
	lcd.setCursor(3, 0);
	lcd.print(F("Intrusion"));
	lcd.setCursor(3, 1);
	lcd.print(F("notifications"));
	lcd.setCursor(3, 2);
	lcd.print(F("successfully"));
	lcd.setCursor(3, 3);
}

void MenuScreenClear()
{
	if (!flag)
	{
		lcd.clear();
		flag = true;
	}
}

void activated()
{
	lcd.setCursor(4, 2);
	lcd.print(F("activated"));
}

void showTimeHelper()
{
	lcd.print(printDigits(h));
	lcd.print(':');
	lcd.print(printDigits(m));
}

void successfullySet()
{
	lcd.print(F("successfully set"));
}

void requestFailed()
{
	lcd.clear();
	lcd.setCursor(6, 1);
	lcd.print(F("REQUEST"));
	lcd.setCursor(6, 2);
	lcd.print(F("FAILED!"));
	delay(1500);
}

void USSDcodeRunning()
{
	lcd.setCursor(4, 1);
	lcd.print(F("USSD CODE"));
	lcd.setCursor(4, 2);
	lcd.print(F("RUNNING..."));
}
