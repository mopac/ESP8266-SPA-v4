# ESP8266-SPA-v4
This project is designed to provide a bridge between a Bilboa hottub or spa controller and an MQTT broker. 
Periodically publishing all the hottub data (such as temperature, heat status, fault messages), 
and subscribing to hottub command messages and relaying them to the hottub controller.

The expectation is that some other tool will use this MQTT data to provide a user interface and control of the hottub.
I use OpenHAB and the 'things', 'items', 'rules' and 'sitemape' files are included to allow simple setup in OpenHAB.

The hardware used is Wemos D1 Mini; RS485 data converter; 15V power supply. This hardware must be fitted inside the hottub and connected to the Balboa Controller network bus. The hardware must be able to connect to your house wifi, to allow it to communicate with the MQTT broker.

## Credits

This code is largely based on the work of cribskip (https://github.com/cribskip/esp8266_spa). I have improved the hardware, added a web interface, restructured the code slightly and a few other tweaks but the protocol handling is all his.

And the great work here (https://github.com/ccutrer/balboa_worldwide_app/wiki) to reverse engineer the bilboa bus protocol

# Hardware

The software is designed to run on a WEMOS D1 Mini (https://www.wemos.cc/en/latest/d1/d1_mini.html). It will also run on WEMOS D1 Mini Pro. The Pro has the advantage that it can be configured to use an external antenna. The external antenna means it has better range for connecting to wifi. Generally your hottub is in the garden not too close to your wifi access point so extended range is essential.
![WEMOS D1 Mini Pro image](/images/wemos-d1-mini-pro.jpg)

The Balboa controller uses a 15V DC power bus; whereas the WEMOS D1 Mini requires 5V. A 15V to 5V DC-DC converter is required to power the WEMOS D1 mini from the Balboa power bus. I use the WEMOS Power Shield (https://www.wemos.cc/en/latest/d1_mini_shield/dc_power.html). It is exactly the same size as the D1 Mini with exactly the same pinout; so is very simple to connect.

![Power Shield image](/images/power%20shield.jpg)

The Balboa control bus uses the RS485 physical layer to send and receive serial messages. RS485 uses a 2 wire differential, half duplex configuration. So transmit and receive happens on the same pair of pins. Conflict and arbitration is handled by the higher level protocols. Essentially the master on the bus assigns a cyclic time window to each slave, where transmitt messages are allowed.

The WEMOS D1 Mini serial interface uses 3.3V logic for Tx and Rx. A converter is required to convert the WEMOS Rx & Tx pins to RS485. Maxim make a suitable chip called MAX485 and conveniently many suppliers mount the MAX485 on small adaptor boards and bring the interface signals to convenient headers.
The only minor issue is voltage levels. MAX485 interface is 5V whereas D1 Mini is 3.3V. MAX485 interface has 4 signals (DI, DE, RE, RO) - 3 inputs and an output. The 3 inputs are not a problem as the 3.3V outputs from the D1 Mini can drive 5V inputs without any problem. But the 5V output (RO), would damage the D1 Mini 3.3V input if connected directly. The solution is a simple potential divider - 2 resistors to reduce the voltage.

![MAX485 image](/images/RS485.png)

# WEMOS D1 Mini Serial

WEMOS D1 Mini has a single hardware UART for sending and receiving serial messages. This UART is available via pins GPIO1 (Tx) and GPIO3 (Rx). But the problem is that GPIO1 & GPIO3 are also connected to the Serial to USB converter. This USB interface allows the D1 Mini to be connected to the USB port of your laptop for flash programming and Serial.print() messages during debug. It is possible to connect the UART to the USB converter and the RS485 converter at the same time but this is very dependent on the D1 mini you are using. Some D1 Mini clones use a different USB converter or don't have the series resistors. So generally, sharing the pins is unreliable.

A much more reliable solution is to use a feature of D1 Mini that swaps the Tx & Rx to pins GPIO13 & GPIO15. With this swap, there is no conflict between the USB converter and the RS485 converter. The RS485 converter connects to GPIO13 & GPIO15. The USB converter stays connected to GPIO1 & GPIO3.

But with this swap, there is no hardware to send serial message to your laptop. The solution is to use SoftwareSerial! SoftwareSerial is a software library that can implement a software UART on any pair of the D1 Mini GPIO. So we configure SoftwareSerial to use GPIO1 & GPIO3, where the USB converter is connected. Then we send all debug serial messages via SoftwareSerial. Because SoftwareSerial is 'bit-bashing' the serial messages, it is advisable to run at a slower baudrate that is possible with the hardware UART. I use SoftwareSerial at 57600 baud, which seems pretty reliable; also the odd error in a debug message is unlikely to be a problem.

It is possible to use SoftwareSerial to drive the RS485 converter directly without the swap feature. But the 115200 baud required for the RS485 connection is at the limit of SoftwareSerial's performance. I found too many bus error with the hottub. Swap is much more robust.
