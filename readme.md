# Smart air conditioner controller
An internet-cennected controller able to keep the room temperature at a desired level.

### Hardware
![hardware](https://i.imgur.com/L7kMx0h.jpg)

- The device is mounted on the wall replacing the old mechanical keys.
- Node-MCU board is the main controller connected to the internet with wifi.
- Other sensors such as the temperature sensor, the humidity sensor, and the manual control keys are connected to node.
- node communicates with an MQTT server sending current sensor readings and receiving the commands from the app.

### Software
![software](https://i.imgur.com/kN87t8r.jpg)

The application reports the room temperature and humidity and allows the user to set a desiered temperature for automatic control. It is also possible to control the switches directly from the app.