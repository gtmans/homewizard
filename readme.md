<H2>Socketswitch</H2><BR>
Automatic socket switch and display based on M5 matrix and Arduino IDE. This switch turns off a specific socket depending on the total consumption of 4 Homewizard Energy sockets at a certain total value, and turns it back on when possible.<BR><BR>
<img src="https://github.com/gtmans/homewizard/blob/main/autosocketswitch/M5MatrixAutoSocketSwitch.png" width="300" align="left" /><BR><BR><BR><BR><BR><BR>

https://github.com/gtmans/homewizard/tree/main/autosocketswitch
https://github.com/gtmans/homewizard/blob/main/M5MatrixAutoSocketSwitch-git.ino
<BR><BR>

<H2>draadloze remote voor 1 socket</H2> 
![single](https://github.com/gtmans/homewizard/blob/main/single-small.png)
![single](https://github.com/gtmans/homewizard/blob/main/single2-small.png)

<H2>draadloze remote voor 2 sockets</H2> 

![dual](https://github.com/gtmans/homewizard/blob/main/dual-small.png)

Je moet er even induiken maar de api is goed gedocumenteerd en met wat hulp van ChatGPT en een D1 mini of een M5Stick heb je het best snel aan de praat. 
Ik heb nog plannen voor een watermelder en een schakeling op basis van luchttemperatuur of luchtvochtigheid. 


In de socket moet je eerst de api aanzetten. Daarna kun je op basis van apparaat id het IP adres achterhalen. Om het helemaal goed te doen zou je het IP ook nog vast moeten zetten in je router. 
Met je wifi ssid en passwd en IP maak je contact met de socket. Je kunt hem uitlezen en ook aanpassen. 

![schema](https://github.com/gtmans/homewizard/blob/main/api-SWITCH-DUAL_bb.png)

Arduino IDE code voor D1 mini en M5Stick beschikbaar in deze directory
