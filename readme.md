<H1>Home automation with Homewizard Energy sockets API</H1>

You'll need to delve into it a bit, but the API is well-documented, and with some help from ChatGPT and either a D1 mini or an M5Stick or M5 Atom, you can get it up and running quite quickly using Arduino IDE.

In the energy socket (app), you first need to enable the API. After that, you can retrieve the IP address based on the device ID. 
To do it all properly, you should also assign a fixed IP in your router.
With your WiFi SSID, password, and IP, you can connect to the socket. You can read from it and also make adjustments.
In this repository you can find sketches for some remotes for socketcontrol, an automatic socket switcer and I a working on plans for a water sensor/switch and a socket datalogger.
All are free to use for your own projects. The API is described here: https://api-documentation.homewizard.com/docs/introduction/

<H2>Auto Socketswitch</H2>
Automatic socket switch and display based on M5 matrix and Arduino IDE. This switch turns off a specific socket depending on the total consumption of 4 Homewizard Energy sockets at a certain total value, and turns it back on when possible.<BR><BR>
<img src="https://github.com/gtmans/homewizard/blob/main/autosocketswitch/M5MatrixAutoSocketSwitch.png" width="300" align="left" /><BR><BR><BR><BR><BR><BR>


https://github.com/gtmans/homewizard/tree/main/autosocketswitch<BR>
https://github.com/gtmans/homewizard/blob/main/M5MatrixAutoSocketSwitch-git.ino
<BR><BR>

<H2>Socketswitch/remote</H2>


<H3>draadloze remote voor 1 socket</H3> 

<img src="https://github.com/gtmans/homewizard/blob/main/socketswitch/api-switch-single-small.png" width="300" />

<H3>draadloze remote voor 2 sockets</H3> 

<img src="https://github.com/gtmans/homewizard/blob/main/socketswitch/api-switch-dual-small.png" width="300" />

https://github.com/gtmans/homewizard/tree/main/socketswitch


<H3>work in progress</H3>
Socketlogger based on D1 mini or M5Stack logs every hour the powerconsumption of 1 or more sockets into a .csv file on SD
<BR><BR>
<img src="https://github.com/gtmans/homewizard/blob/main/socketlogger/Socketlogger-D1mini.jpg" width="300" />


