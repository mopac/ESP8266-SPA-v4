# ESP8266-SPA-v4
This project is designed to provide a bridge between a Bilboa hottub or spa controller and an MQTT broker. 
Periodically publishing all the hottub data (such as temperature, heat status, fault messages), 
and subscribing to hottub command messages and relaying them to the hottub controller.

The expectation is that some other tool will use this MQTT data to provide a user interface and control of the hottub.
I use OpenHAB and the 'things', 'items', 'rules' and 'sitemape' files are included to allow simple setup in OpenHAB.

The hardware used is Wemos D1 Mini; RS485 data converter; 15V power supply
