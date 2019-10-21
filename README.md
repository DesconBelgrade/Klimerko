
# “Klimerko” Air Quality Monitoring Station

<img align="right" width="300" height="200" src="extras/klimerko-tile.png">

"Klimerko" is the continuation of the [Winning Project from Descon 4.0 2018](https://descon.me/2018/winning-product/).  

See live data from all "Klimerko" devices in your area by going to [vazduhgradjanima.rs](http://vazduhgradjanima.rs)

This is a DIY air quality measuring device that costs about $30 to build (even less without the 3D Case) and measures [Particulate Matter](https://www.epa.gov/pm-pollution/particulate-matter-pm-basics) in the air as well as Temperature, Humidity and Pressure.  
Read on to find out how to build your own!


### Firmware Version History
| Version | Changelog |
|--|--|
| v1.1 | Added ability to configure device credentials using Serial Monitor (no need to re-flash the whole device just to change credentials), updated textual form "Air Quality" reporting to use PM 10 values (based on Republic of Serbia Regulations) instead of PM 2.5 used previously  |
| v1 | The initial firmware written for Descon 5.0 (2019) workshop |


# Parts
This diagram shows an overview of how Klimerko works
![Klimerko Diagram](extras/klimerko-diagram.png)

## Hardware Required

> In case you're not following this guide at a workshop: 
> You're still able to build this device if you don't have the 3D case. You'll just need to tinker and figure out how to place the device in a secure location away from rain/sun (but in an open space so it can detect pollution properly) 

You need to have the following components to follow this guide:

- [NodeMCU](https://www.aliexpress.com/item/33053690164.html) Board (with CP2102 chip)
- [Plantower PMS7003](https://www.aliexpress.com/item/32623909733.html) Air Quality Sensor
    - Bridge board
    - Connector cable
- [Bosch BME280](https://www.aliexpress.com/item/32961882719.html) Temperature/Humidity/Pressure Sensor
- USB Power adapter (5V, minimum 250mA)
- MicroUSB Cable (5m preferred)
- 3D printed case for the device, which comprises of:
    - Components base
    - Weather-resistant cover
    - Flat stand
    - Wall-mount holder
- **5x** Screws (for plastic, 3x5mm) (**7**x screws if you’re going to wall-mount the device)
- **4x** Wires (each at least 13cm long)


## Tools Required

You’re going to need the following tools at minimum to complete the project:

- Soldering Iron
- Solder
- Scissors or a Wire Stripper
- Screwdriver



# Hardware Assembly

Once you’ve got all the components and tools ready, it’s time to begin the assembly process.

## 3D Case Preparation

Depending on your use case, you can use either the flat stand or the wall-mounted holder.
If you wish to wall-mount the device, use 2 screws to mount the holder to a wall. You’ll be able to easily attach and detach the device from the wall.

For now, disassemble the case and put away all the parts except for the components base, where you’ll be placing all the components.

## PMS7003 Sensor Preparation

- First, make sure **not** to remove the blue plastic cover from the PMS7003 sensor as it helps it fit into the 3D case better.
- Place the PMS7003 sensor into the components base of the 3D case with the connection port of PMS7003 facing outwards in upper right side.  
- Plug in the connector cable into the bridge board.  
- Because we don’t need excess cabling, removal of the unnecessary wires from the connector cable is suggested. In order to know which wires to pull, make sure the connector cable is plugged into the bridge board so that you can see the markings on the board that correspond to the wires. To remove the wires, pull them with medium force until they’re detached from the connector. 
These are the wires to pull (marked red):
    - **NC** (first one)
    - **NC** (second one)
    - **RST**
    - **SET**
    The only wires left attached should be **TX**, **RX**, **GND** and **VCC**.

![Bridge Board Wire Removal](extras/bridge-board-wire-removal.png)
    
- Measure ~13cm of all 4 remaining wires from the connector cable and cut the rest, so you’re left with the connector at one end and cut wires on the other.
- Using scissors or a wire stripper, remove the insulation ~2mm from the end of each wire, so you’re left with clear copper at the ends.
- Plug the bridge board (with the connector cable in) into the PMS7003 Sensor.
- Route the wires through the circular hole in the components base of the 3D case so it reaches the other side of the base.


## BME280 Sensor Preparation
- Take the 4 wires you have prepared (not to be confused with the PMS7003 wires) and strip (remove insulation) ~2mm from each end of each wire so that you have 4 wires with all ends stripped.
- Solder those 4 wires to pins **SDA**, **SCL**, **GND**, **VCC/VIN** on the BME280
- Make sure the PMS7003 Sensor is seated properly (because BME280 goes above it), and only then proceed to the next step.
- Mount the BME280 sensor in place by screwing it to the hole in the upper right side of the 3D case (above the PMS7003).
- Route the wires through the circular hole in the components base of the 3D case so it reaches the other side of the base.

You have now completed the setup of the sensor side of the components base.
Next up, the NodeMCU side.


## NodeMCU Preparation

You should now have 8 wires coming through the circular hole leading to the NodeMCU side of the 3D case.


- Solder the wires to the NodeMCU board:


> Please double check to make sure you’re soldering the correct wires (coming from the sensors) to the NodeMCU board.

| Sensor  | Sensor Wire | NodeMCU Pin             |
| ------- | ----------- | ----------------------- |
| PMS7003 | VCC         | VIN                     |
| PMS7003 | GND         | GND (Any GND pin works) |
| PMS7003 | RX          | D6                      |
| PMS7003 | TX          | D5                      |
| BME280  | VCC/VIN     | 3v3 (Any 3v3 pin works) |
| BME280  | GND         | GND (Any GND pin works) |
| BME280  | SCL         | D1                      |
| BME280  | SDA         | D2                      |

> You have now finished all steps that require soldering.

- Now use 4 screws to place the NodeMCU in its spot in the components base of the 3D case.
- Double check all the connections and if everything is as it should be.
- Put the weather-resistant cover of the 3D case over the components base (it's better to do this once everything is done and device is connected, though)

Congratulations, you’ve assembled the device! Now onto the software side.


# Cloud Platform (Credentials)
- Head over to https://maker.allthingstalk.com/signup and create an account
    
![Cloud Signup](extras/cloud-signup.png)

- Sign in
- Click “Playground”
    
![Cloud Grounds](extras/cloud-grounds.png)

- In *Devices*, click *+ New Device*
    
![Cloud Devices](extras/cloud-devices.png)

- Choose “Descon Klimerko”
    
![Cloud Device Selection](extras/cloud-device-selection.png)

- Click “Settings” in the upper right corner
    
![Cloud Settings](extras/cloud-settings.png)

- Click “Authentication”
    
![Cloud Authentication](extras/cloud-authentication.png)

- Write down your **Device ID** and **Device Token**
    
![Cloud Credentials](extras/cloud-credentials.png)



# Software

## Uploading Firmware
- Download and install https://www.arduino.cc/en/Main/software (choose “Windows installer, for Windows XP and up” if you’re on Windows)
    If you already have Arduino IDE, make sure it’s at least version 1.8.10
- Open Arduino IDE
- Go to *File* > *Preferences*
- In the *Additional Boards Manager URLs*, enter `http://arduino.esp8266.com/stable/package_esp8266com_index.json` and click *OK*
- Go to *Tools* > *Board* > *Boards Manager*
- Search for and install “*esp8266*” by *ESP8266 Community*
- Once done, close Arduino IDE
- Plug in the USB cable into your Klimerko device and your computer
- Download https://github.com/DesconBelgrade/Klimerko/archive/master.zip
- Unzip the file, open it and go to “Klimerko_Firmware” folder
- Open “Klimerko_Firmware.ino” with Arudino IDE
- Now go to *Tools* > *Board* and choose “*NodeMCU 1.0 (ESP-12E Module)*”
- Go to *Tools* > *Upload Speed* and choose *115200*
- Go to *Tools* > *Port* and you should see a single port there. Select it.
- Go to “Sketch” > “Upload” and wait for the firmware to be uploaded to your Klimerko device

## Configuring Device Credentials
- Once you've uploaded the firmware, go to “Tools” > “Serial Monitor” and press the “RST” button on the NodeMCU
- When you see “Write 'config' to configure your credentials (expires in 10 seconds)”, enter “config” in the upper part of Serial Monitor and press ENTER
- Now enter “all” in the Serial Monitor input and press ENTER
- Now enter your WiFi Network Name, WiFi Password and the Device ID and Device Token for communication with AllThingsTalk (noted earlier)
- Your device will now restart. Wait for it to boot.
> Use the Serial Monitor for diagnostic output from Klimerko. If you don’t see any data, restart Klimerko (either press the RST button on NodeMCU or unplug and plug it back it) because it’s currently not reading/publishing data, so there’s nothing to be shown.
- Once you see “Your device is up and running!” in Serial Monitor, you’re good to go!
- Feel free to unplug the device from your computer and plug it into a wall USB Power adapter with your 5m USB cable. 

> Note: There’s a blue LED light on the device that automatically starts “breathing/fading” when a connection to either WiFi or AllThingsTalk is being established or is dropped. If the LED isn’t “breathing/fading” (if it’s off), the device should be connected successfully and uploading data.


# Cloud Platform (Final)

All of the data from your Klimerko is available on your AllThingsTalk Maker.  
Other than raw actual and historical air quality values, you’re able to see your air quality in a textual form at a glance, see the WiFi Signal Strength of your Klimerko and control how often your Klimerko publishes data.  
You’ll also get notifications if your Klimerko goes offline for any reason.

## Configure Device

Open [AllThingsTalk Maker](https://maker.allthingstalk.com), go to “Devices” and select your new “Descon Klimerko” device.  
You’ll see all assets of your device (stuff that it can report and things you can control).

> Make sure to keep the data (that you’re about to set below) updated if you move your device

- Click on the “Location” asset and select the location of your Klimerko on the map
- Click on the “Height” asset and set the height of your device (in **meters**) by adding a value right next to `"value":`  and clicking the ✔️ button when done (below is an example of 10 meters height):
```
    {
        "at": "2019-10-18T11:28:04.554Z",
        "value": 10
    }
```
This needs to be done since air pollution readings are different on different heights.


## Share Data

Go to your “Decon Klimerko” device, click “Settings” in the upper right corner, go to “Share data” and share your data with “**Vazduh gradjanima**”


## Visualize Data

Now go to “Pinboards” on the left side of AllThingsTalk Maker.  
You should see a new pinboard named “Klimerko”. Select it if it isn’t already selected.  
All data from your Klimerko is visualized here:

- Air Quality (in textual form)  
    Value shown will also have a corresponding background color.  
    This value is derived from PM10 values (as per regulations of Republic of Serbia):
    
| Textual Value | PM 10 Value |
| ------------- | ----------- |
| Excellent     | 0 - 25      |
| Good          | 26 - 35     |
| Acceptable    | 36 - 50     |
| Polluted      | 51 - 75     |
| Very Polluted | 75 >        |

- Actual PM1, PM2.5, PM10, Temperature, Humidity and Pressure data
- Historical PM1, PM2.5, PM10, Temperature, Humidity and Pressure data
- Location of your device on the map (that you’ve set)
- WiFi Signal Strength of your Klimerko device  
    Possible values (from good to bad): Excellent, Good, Decent, Bad, Horrible.  
    The values shown will also have a corresponding background color.
- Device height (that you’ve set)
- Device reporting interval (minutes)  
    You can use this slider to control how often your Klimerko reports its data to AllThingsTalk. The default and recommended value is 15 minutes.

You’re done! Enjoy your device and feel free to visit [vazduhgradjanima.rs](http://vazduhgradjanima.rs) and see your device along with all the other devices just like yours that are helping others be aware of the air pollution in your area!


