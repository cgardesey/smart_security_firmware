# Arduino Project: Intrusion Detection

This is an Arduino project that demonstrates intrusion detection with Passive Infrared Sensors (PIR's) and sound off alarms (buzzers). The project uses the Arduino programming language and can be customised for similar projects

## Table of Contents
- [Features](#features)
- [Requirements](#requirements)
- [Hardware Setup](#hardware-setup)
- [Software Setup](#software-setup)
- [Usage](#usage)
- [Contributing](#contributing)
- [License](#license)


## Features

-  **Auto power**: Set security system to auto activate at a set time and auto deactivate at a set time.
-  **Notifications**: Receive alerts on android app upon detection of intrusion.
-  **LCD display**: Interact with hardware with a keypad and LCD screen.
-  **Keypad**: Interact with hardware with a keypad and LCD screen.
-  **Time display:** See displayed time on LCD screen

## Requirements
To run this project, you will need the following components:
- Two Arduino board (e.g., Arduino Uno or Arduino Nano)
  The two boards will communicate serially. One board will serve as the master and the other will serve as the slave
- RTC
- 20x4 LED
- 4x4 Keypad
- Sim800 or sim900
- PIR's
- 120db buzzers
- Switch (Toggle, push button or slide switch)
- Terminal blocks (optional)
- 12V 5A power adapter (with stable output)
- Flexible wire calbe
- Packaging (Junction box, 3D moulded plastic box, wooden box etc)
- Resistors (10K, 1K 560R)
- Transistors (BC547, BD139)
- Capacitors (100nF, 10uF, 100uF 
- Jumper wires
- Breadboard (optional, for prototyping)
- Male headers
- Female headers

## Hardware Setup
Setup the the hardware as shown below:

![image](https://github.com/cgardesey/smart_security_firmware/assets/10109354/69208ca9-477b-47e1-883b-1400273117c8)



## Software Setup
1. Install the Arduino IDE (Integrated Development Environment) from the [official Arduino website](https://www.arduino.cc/en/software).
2. Connect your Arduino board to your computer using a USB cable.
3. Open the Arduino IDE and select the appropriate board and port from the **Tools** menu.
4. Create a new sketch in the Arduino IDE.
5. Copy and paste the code from the `Smart_Security.ino` file into your sketch.
6. Save the sketch with a meaningful name.

## Usage
1. Upload the **slave.ino** sketch to the Arduino board **labled Duino 1** by clicking the **Upload** button in the Arduino IDE.
2. The LED connected to the Arduino board should start blinking at a fixed interval.
3. Experiment with the code to and customize to suit your needs.
4. Repeat steps 1, 2, 3 using the sketch named **master.ino** for the Arduino board labled  **Duino 2**.

## Contributing
Contributions to this project are welcome. If you would like to contribute, please follow these steps:
1. Fork the repository.
2. Create a new branch for your feature or bug fix.
3. Make your changes and test them thoroughly.
4. Submit a pull request with a clear description of your changes.

If you're looking to integrate with an android sms based project, make sure to check out the the repository corresponding to the [android project](https://github.com/cgardesey/SmartSecurity) for detailed instructions.

## License
This project is licensed under the [MIT License](LICENSE). Feel free to modify and distribute this project as needed.
