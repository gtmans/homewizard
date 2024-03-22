<H2>Socketswitch</H2>

You'll need to delve into it a bit, but the API is well-documented, and with some help from ChatGPT and either a D1 mini or an M5Stick or M5 Atom, you can get it up and running quite quickly using Arduino IDE.

In the energy socket (app), you first need to enable the API. After that, you can retrieve the IP address based on the device ID. To do it all properly, you should also assign a fixed IP in your router. With your WiFi SSID, password, and IP, you can connect to the socket. You can read from it and also make adjustments. In this repository you can find sketches for some remotes for socketcontrol, an automatic socket switcer and I a working on plans for a water sensor/switch and a socket datalogger. All are free to use for your own projects. The API is described here: https://api-documentation.homewizard.com/docs/introduction/

<H3>wireless remote for 1 socket</H3> 

<img src="https://github.com/gtmans/homewizard/blob/main/socketswitch/api-switch-single-small.png" width="300" />
<img src="https://github.com/gtmans/homewizard/blob/main/socketswitch/api-switch-single2-small.png" width="300" />

<H3>wireless remote for 2 sockets</H3> 

<img src="https://github.com/gtmans/homewizard/blob/main/socketswitch/api-switch-dual-small.png" width="300" />
<img src="https://github.com/gtmans/homewizard/blob/main/socketswitch/api-switch-dual_breakboard.png" width="500" />

<H3>wireless remote for multi sockets</H3> 

<img src="https://github.com/gtmans/homewizard/blob/main/socketswitch/M5Stick-ApiSwitchMulti.png" width="300" />

Arduino IDE code for D1 mini and M5Stick available in this directory

