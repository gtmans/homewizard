Homewizard Socket thermostat

<img src="https://github.com/gtmans/homewizard/blob/main/thermostat/Sochet-thermostat-smaller.png" align=left width="300" />

I have connected an electric heater to a HomeWizard Socket because the built-in (mechanical) thermostat does not work very well. With a few simple and inexpensive components (D1 Mini, SHT30 sensor, rotary switch, OLED SSD1306 display), I can easily switch the socket with the heater on and off based on the set temperature. Additionally, I can easily adjust the desired temperature and set how long it takes before it switches on or off again to prevent it from oscillating. The D1 Mini connects to the socket via WiFi, for which the API must be enabled in the HomeWizard app. The switch lock must also be turned off there.

I used a "Tripler Lolin Board" with a D1 Mini, an SHT30 shield V1.2.0 (standard pins D1 and D2), and a rotary switch (D5, D6, D7). To connect the OLED display and rotary switch, I made a custom board to easily combine the components, but you can also solder everything onto a single perforated board or use a breadboard. It might be useful to place the temperature sensor at a distance and use longer wires. This can also be done using the mini I2C connector on the D1 and the SHT30.

<img src="https://github.com/gtmans/homewizard/blob/main/thermostat/Thermo-Fritz.png" />
<img src="https://github.com/gtmans/homewizard/blob/main/thermostat/Thermo-breadboard.png" />

Description : Wireless Thermostat using Homewizard api to control a specific socket used by some heating appliance.<BR>
parts    : D1-mini, SHT30 sensor, rotary switch, oled SSD1306 display<BR>
features : can show actual temp and humidity, has adjustable SocketIP, wanted temperature, rotary step, minimal cummuting time, max. operating time <BR>
Sketch: https://github.com/gtmans/homewizard/blob/main/thermostat/SocketThermov4.ino <BR>

<img src="https://github.com/gtmans/homewizard/blob/main/thermostat/Thermo-breakout.png" />
 * Warning:
 * be aware that this device can turn a socket on without you being there 
 * use with care. Do not use unattended. Lock socket IP-address in your router (bind to MAC address) so it cannot change 
