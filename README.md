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

The Balboa control bus uses the RS485 physical layer to send and receive serial messages. RS485 uses a 2 wire differential, half duplex configuration. So transmit and receive happens on the same pair of pins. Conflict and arbitration is handled by the higher level protocols. Essentially the master on the bus assigns a cyclic time window to each slave, where transmitt messages are allowed.

The WEMOS D1 Mini serial interface uses 3.3V logic for Tx and Rx. A converter is required to convert the WEMOS Rx & Tx pins to RS485. Maxim make a suitable chip called MAX485 and conveniently many suppliers mount the MAX485 on small adaptor boards and bring the interface signals to convenient headers.
The only minor issue is voltage levels. MAX485 interface is 5V whereas D1 Mini is 3.3V. MAX485 interface has 4 signals (DI, DE, RE, RO) - 3 inputs and an output. The 3 inputs are not a problem as the 3.3V outputs from the D1 Mini can drive 5V inputs without any problem. But the 5V output (RO), would damage the D1 Mini 3.3V input if connected directly. The solution is a simple potential divider - 2 resistors to reduce the voltage.
![MAX485 image](/images/RS485.png)

