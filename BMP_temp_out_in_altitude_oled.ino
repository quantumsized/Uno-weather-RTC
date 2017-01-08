#include <SFE_BMP180.h>
#include <SPI.h>
#include <Wire.h>
#include <OneWire.h>
#include <DS3231.h>
#include "DHT.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// You will need to create an SFE_BMP180 object, here called "pressure":

SFE_BMP180 pressure;
DS3231  rtc(SDA, SCL);
DHT dht;

int SensorPin = 2;
OneWire ds(SensorPin);

#define OLED_RESET 4
Adafruit_SSD1306 display(OLED_RESET);

int dhtPin = 3;

#define ALTITUDE 153.10 // Altitude of Home in Yelm, WA. in meters
//define RTC address
#define DS3231_I2C_ADDRESS 0x68 // Set address for RTC so as there is no conflict

String tod = "AM";
char* timeZone[] = { "PST", "PDT"};
boolean showSeconds = true; // Set to true if you want to show seconds
boolean useDST = true;      // Set to true for use of DST. Not auto-set as to geolocation.
int DST = 0; // Used for setting Daylight Savings Time for RTC
boolean isMetric = false;

int miliseconds = 1;
int seconds = miliseconds * 1000;
int minutes = seconds * 60;
unsigned long previousMillis = 0; // Var for timer governing cycle tests
const long interval = 100; // Cycle time for running tests set in milliseconds

//convert normal decimal numbers to binary coded decimals
byte decToBcd(byte val) {
  return ( (val / 10 * 16) + (val % 10) );
}
//convert binary coded decimal to normal decimal numbers
byte bcdToDec(byte val) {
  return ( (val / 16 * 10) + (val % 16) );
}

const int chipSelect = 4;

#if (SSD1306_LCDHEIGHT != 64)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

void setup() {
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  dht.setup(dhtPin);
  delay(2000);

  // Clear the buffer.
  display.clearDisplay();
  // Initialize the sensor (it is important to get calibration values stored on the device).
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  if (pressure.begin())
    display.println("BMP180 init success");
  else
  {
    // Oops, something went wrong, this is usually a connection problem,
    // see the comments at the top of this sketch for the proper connections.

    display.println("BMP180 init fail\n\n");
    while (1); // Pause forever.
  }
  rtc.begin();
  display.print("Loading sensors.");
  display.display();
  delay(100);
  display.print(".");
  display.display();
  delay(100);
  display.print(".");
  display.display();
  delay(100);
  display.println(".");
  display.display();
  delay(1000);
}

//set the time and date to the RTC
void setRTCTime(byte second, byte minute, byte hour, byte dayOfWeek, byte dayOfMonth, byte month, byte year) {
  // sets time and date data to DS3231
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0); // set next input to start at the seconds register
  Wire.write(decToBcd(second)); // set seconds
  Wire.write(decToBcd(minute)); // set minutes
  Wire.write(decToBcd(hour)); // set hours
  Wire.write(decToBcd(dayOfWeek)); // set day of week (1=Sunday, 7=Saturday)
  Wire.write(decToBcd(dayOfMonth)); // set date (1 to 31)
  Wire.write(decToBcd(month)); // set month
  Wire.write(decToBcd(year)); // set year (0 to 99)
  Wire.endTransmission();
}

//read the time and date from the RTC
void readRTCTime(byte *second, byte *minute, byte *hour, byte *dayOfWeek,byte *dayOfMonth, byte *month, byte *year) {
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0); // set DS3231 register pointer to 00h
  Wire.endTransmission();
  Wire.requestFrom(DS3231_I2C_ADDRESS, 7);
  // request seven bytes of data from DS3231 starting from register 00h
  *second = bcdToDec(Wire.read() & 0x7f);
  *minute = bcdToDec(Wire.read());
  *hour = bcdToDec(Wire.read() & 0x3f);
  *dayOfWeek = bcdToDec(Wire.read());
  *dayOfMonth = bcdToDec(Wire.read());
  *month = bcdToDec(Wire.read());
  *year = bcdToDec(Wire.read());
}

void loop() {
  char status;
  double T, P, p0, a;
  byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
  float humidity = dht.getHumidity();
  //retrieve time
  readRTCTime(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month, &year);
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) { // use millis() timer rather than delay()
    previousMillis = currentMillis; // update var for checking current cycle in timer
    float To = getTemp();
    /**  Start DST setting block
         You will have to check when DST changes in your area and set this manually.  
         Otherwise you may find or create a library of general timezones that do and don't use Daylight Savings Time. **/
    if(useDST == true) {
      if (dayOfWeek == 7 && month == 3 && dayOfMonth >= 7 && dayOfMonth <= 14 && hour == 2 && DST == 0) {
        // DS3231 seconds, minutes, hours, day, date, month, year
        setRTCTime(second, minute, 3, dayOfWeek, dayOfMonth, month, year);
        DST = 1;
      } else if (dayOfWeek == 7 && month == 11 && dayOfMonth >= 1 && dayOfMonth <= 7 && hour == 2 && DST == 1) {
        // DS3231 seconds, minutes, hours, day, date, month, year
        setRTCTime(second, minute, 1, dayOfWeek, dayOfMonth, month, year);
        DST = 0;
      }
    }
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(0,0);
    status = pressure.startTemperature();
    if (status != 0) {
      // Wait for the measurement to complete:
      delay(status);
      printTime();
      display.setCursor(0,20);
      display.setTextSize(1);
      display.print("Inside RH:");
      display.print(humidity, 0);
      display.println("%");
      // Retrieve the completed temperature measurement:
      // Note that the measurement is stored in the variable T.
      // Function returns 1 if successful, 0 if failure.
  
      status = pressure.getTemperature(T);
      if (status != 0) {
        // Print out the measurement:
        display.setCursor(0,30);
        if(isMetric) {
          display.print("In:");
          display.print(T, 1);
          display.print((char)247);
          display.print("C");
          display.print(" Out:");
          display.print(To, 1);
          display.print((char)247);
          display.println("C");
        }else{
          display.print("In:");
          display.print((9.0 / 5.0)*T + 32.0, 1);
          display.print((char)247);
          display.print("F");
          display.print(" Out:");
          display.print((9.0 / 5.0)*To + 32.0, 1);
          display.print((char)247);
          display.println("F");
        }
        // Start a pressure measurement:
        // The parameter is the oversampling setting, from 0 to 3 (highest res, longest wait).
        // If request is successful, the number of ms to wait is returned.
        // If request is unsuccessful, 0 is returned.
  
        status = pressure.startPressure(3);
        if (status != 0) {
          // Wait for the measurement to complete:
          delay(status);
  
          // Retrieve the completed pressure measurement:
          // Note that the measurement is stored in the variable P.
          // Note also that the function requires the previous temperature measurement (T).
          // (If temperature is stable, you can do one temperature measurement for a number of pressure measurements.)
          // Function returns 1 if successful, 0 if failure.
  
          status = pressure.getPressure(P, T);
          if (status != 0) {
  
            // The pressure sensor returns abolute pressure, which varies with altitude.
            // To remove the effects of altitude, use the sealevel function and your current altitude.
            // This number is commonly used in weather reports.
            // Parameters: P = absolute pressure in mb, ALTITUDE = current altitude in m.
            // Result: p0 = sea-level compensated pressure in mb
            display.setCursor(0,40);
            p0 = pressure.sealevel(P, ALTITUDE); // we're at 153 meters (Yelm, WA)
            display.print("Pres: ");
            if(isMetric) {
              display.print(p0, 2);
              display.println(" mb");
            }else{
              display.print(p0 * 0.0295333727, 2);
              display.println(" inHg");
            }
            // On the other hand, if you want to determine your altitude from the pressure reading,
            // use the altitude function along with a baseline pressure (sea-level or other).
            // Parameters: P = absolute pressure in mb, p0 = baseline pressure in mb.
            // Result: a = altitude in m.
  
            a = pressure.altitude(P, p0);
            display.setCursor(0,50);
            display.print("Alt: ");
            if(isMetric) {
              display.print(a, 2);
              display.println(" m");
            }else{
              display.print(a * 3.28084, 2);
              display.println(" ft");
            }
            
          }
          else display.println("error retrieving pressure measurement\n");
        }
        else display.println("error starting pressure measurement\n");
      }
      else display.println("error retrieving temperature measurement\n");
    }
    else display.println("error starting temperature measurement\n");
    display.display();
  }
}

//reads the RTC time and prints it to the top of the LCD
void printTime() {
  byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
  
  //retrieve time
  readRTCTime(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month, &year);
  
  //print to serial
  // Hour:Minute:Second
  // to display 12 hour time ...
  if(hour-12<10 && hour-12>0) {display.print(" ");}
  if(hour>12){
    display.print(hour-12,DEC);
    tod = "PM";
  }else{
    display.print(hour, DEC);
    tod = "AM";
  }
  display.print(":");
  if (minute<10) { display.print("0"); }
  display.print(minute, DEC);
  if(showSeconds == true) {
    display.print(":");
    if (second<10) { display.print("0"); }
    display.print(second, DEC);
  }
  display.println(tod);
}

float getTemp() {


  byte data[12];
  byte addr[8];

  if ( !ds.search(addr)) {
    //no more sensors on chain, reset search
    ds.reset_search();
    return -1000;
  }

  if ( OneWire::crc8( addr, 7) != addr[7]) {
    display.println("CRC is not valid!");
    return -1000;
  }

  if ( addr[0] != 0x10 && addr[0] != 0x28) {
    display.print("Device is not recognized");
    return -1000;
  }

  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1);

  byte present = ds.reset();
  ds.select(addr);
  ds.write(0xBE);


  for (int i = 0; i < 9; i++) {
    data[i] = ds.read();
  }

  ds.reset_search();

  byte MSB = data[1];
  byte LSB = data[0];

  float TRead = ((MSB << 8) | LSB);
  float Temperature = TRead / 16;

  return Temperature;

}
