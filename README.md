# WiFi HTTP grabber example
### How it work (brief)
Next power on the ESP32 module connect to WiFi SSID, then it Download a File from URL and save it to SDcard
### Hardware customize
I use the the WROVER kit 4.1   
https://docs.espressif.com/projects/esp-idf/en/stable/hw-reference/modules-and-boards.html#esp-wrover-kit-v4-1
Warning: you should disconnect 4 jumpers JP2 (JTAG) because this pin will be used in SD card interface. And urfortunatly impossible debug this project with JTAG debugger.

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
So this project will grow it based on multithreading. ffw_main.c create two FreeRTOS task:  
* network_task for network service (WiFi connection and HTTP client)
* store_task for working with SD card  
A threads are sincronized by simple notifications (xTaskNotify)
Because loaded file may have a big size it's can't be download at once. Therefore it download by part in buffer. When the buffer is filled, the network_task send notify to store_task about "I have a data block to give you" and wait a feedback notification from store_task. At this time store_task wait a fresh data block in blocking function Get_http_data(). As it receive a notification, it copy data by memcpy() and send notification to network_task about "I  had copy this data block, and you can download next data portion". Now the store_task save the data block to SD card and the same time network_task download next data part. This cycle repeated while HTTP client haven't give all data.
Error handling:
If a HTTP timeout occured then store_task get notify about this and close unfinished file and delete it.
The same doing will be if HTTP client get wrong data from esp_http_client_read() function
