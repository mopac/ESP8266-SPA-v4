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

The software is designed to run on a WEMOS D1 Mini. It will also run on WEMOS D1 Mini Pro. The Pro has the advantage that it can be configured to use an external antenna. The external antenna means it has better range for connecting to wifi. Generally your hottub is in the garden not too close to your wifi access point so extended range is essential.

The Balboa controller uses a 15V DC power bus; whereas the WEMOS D1 Mini requires 5V. A 15V to 5V DC-DC converter is required to power the WEMOS D1 mini from the Balboa power bus. I use the WEMOS Power Shield. It is exactly the same size as the D1 Mini with exactly the same pinout; so is very simple to connect.

The Balboa control bus uses the RS485 physical layer to send and receive serial messages. RS485 uses a 2 wire differential, half duplex configuration
