<B>Auto Socket Switch for Homewizard Energy sockets</B>

<img src="https://github.com/gtmans/homewizard/blob/main/autosocketswitch/M5MatrixAutoSocketSwitch.png" width="250" align="left" />
Automatic socket switch and display based on M5 matrix and Arduino IDE. This switch turns off a specific socket depending on the total consumption of 4 Homewizard Energy sockets at a certain total value, and turns it back on when possible.<BR><BR><BR><BR><BR><BR><BR>

<B>Story.</B><BR>
<img src="https://github.com/gtmans/homewizard/blob/main/autosocketswitch/M5MatrixAutoSocketSwitch2.png" width="250" align="left" />
Unfortunately, I have 4 high-consumption devices on one 16A fuse. Two of them I cannot automatically switch off because the device does not return to its original state after turning on the socket again (Air conditioning and bathroom radiator). That's why I made a mini display from this M5 Atom matrix that monitors the consumption of 4 specific devices and turns off one particular device to prevent the fuse from blowing. Then I can also turn the socket back on by clicking on the screen. On the display, the first 4 lines represent one particular socket, where 5 LEDs >1200 watts. The 5th line displays the total consumption in red LEDs.<BR><BR><BR>



<B>Requirements.</B>
- Atom matrix (costs about 15 euros)
- Arduino IDE
- 4 Homewizard Energy sockets
- M5MatrixAutoSocketSwitch-git.ino
