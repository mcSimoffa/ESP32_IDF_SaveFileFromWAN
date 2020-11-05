# WiFi HTTP grabber example
### How it work (brief)
After "power supply on" the ESP32 module connect to WiFi SSID, then it Download a File from URL and save it to SDcard.  
You should watch the UART port by monitor to see information messages.  
When message "Card unmounted" will appear you can "power off" module.  
### Hardware customize
I use the the WROVER kit 4.1   
https://docs.espressif.com/projects/esp-idf/en/stable/hw-reference/modules-and-boards.html#esp-wrover-kit-v4-1  
Warning: you should disconnect 4 jumpers JP2 (JTAG) because this pins are used in SD card interface. Urfortunatly it's impossible debug this project with any JTAG debugger.
### Configure the project
```
idf.py menuconfig
```
or
```
eclipse sdkconfig
```
* WAN configuration: Set WiFi SSID and WiFi Password and Maximum retry  
* APP configuration: Set URL from what you want to download, Filename what will be create on SD card and buffer size
### How it work (detail)
Because this project will grow, it based on multithreading. ffw_main.c create two FreeRTOS task:  
* network_task for network service (WiFi connection and HTTP client)
* store_task for working with SD card  
A threads are sincronized by simple notifications (xTaskNotify)  
I use like base a three standard example from SDK 4.1:
* \wifi\getting_started\station\station_example_main.c Nearly without changes.
* \protoclos\esp_http_client\esp_http_client_example.c I use only stream read method from here.
* \storage\sd_card\sd_card_example_main.c I change fprintf function to fwrite there.  
Because loaded file may have a big size it's can't be download at once. Therefore it download by part in buffer. When the buffer is filled, the network_task send notify to store_task about "I have a data array to give you" and wait a feedback notification from store_task. At this time store_task wait a fresh data array in blocking function Get_http_data(). As it receive a notification, it copy data by memcpy() and send notification to network_task about "I  had copy this data array, and you can download next data portion". Now the store_task save the data array to SD card and the same time network_task download next data part. This cycle repeated while HTTP client haven't give all data.
### Error handling:
If a HTTP timeout occured then store_task get notify about this and close unfinished file and delete it.
The same doing will be if HTTP client get wrong data from esp_http_client_read() function
