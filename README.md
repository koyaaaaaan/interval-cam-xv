# interval-cam-xv
Interval Camera storing to SD and Web made out of ESP32<br />
<img src="readme/interval-cam-xv.jpg" width="360" />

#### Features
 - Interval Capture with front CAM
 - Upload image via Wifi
 - Store image in SD card
 - Automatic Flash with a brightness sensor (CDS)
 - Mobile setup App.

# Assebmle
## Components
 - ESP32-CAM
 - SD Card (optional)
 - Tact switch (ON/OFF)
 - Push button (Setting Mode)
 - CDS (12k - 58k)
 - Resistor (22o-mu)
 - Capacitor
 - 5.0-5,2V battery

## ESP32-CAM setup
Open "esp32-code" directory with ArduinoIDE to upload source code to ESP32-cam.<br />
<img src="readme/arduinoIDE.png" width="240" />

## Electric Circuit
Assemble the circuit like below. <br />
<img src="readme/circuit.png" width="600" />

#### ESP32 Pin connections

| Components   | ESP32 GPIO |  
| ------------ | ---------- | 
| CDS          | IO12       | 
| Tact Button  | IO2        | 
| Battery GND  | GND        | 
| Battery 5.0V | 5.0V       | 

#### Considering points
- The registor value is depends on your CDS. ESP32 Analog read returns 0(0V) - 4095(5.0V).
- This cam needs the voltage 5.0 above. Under 5.0 voltage supply can cause low energy error to restart ESP32.
- Don't recommend change the ESP32 pins. Because PIN0 / PIN16 can infect basic functions on ESP32.

## WebAPI Setup
Upload [post.php](https://github.com/koyaaaaaan/interval-cam-xv/tree/main/web) to your Web Server (need to setup php cgi runtime.)<br />


## Download App. 
<img src="readme/mobile-app.jpg" width="240" />

#### How to use
1. Keep push down tact switch to turn on the CAM.<br />
   Interval Cam flushs 3 times to get into Bluetooth setting mode.<br />
2. Pair Interval-Cam-XV with your phone.
3. Open App. to connect.
4. Change setting to Save it.
5. Reset Interval-CAM-XV.

#### Settings


# Trouble shootings 


