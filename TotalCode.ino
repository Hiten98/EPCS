// This #include statement was automatically added by the Particle IDE.
#include "SparkFun_Photon_Weather_Shield_Library/SparkFun_Photon_Weather_Shield_Library.h"

// Add math to get sine and cosine for wind vane
#include <math.h>
#include <Wire.h>
#include<sht15.h>
#include <t5403.h>

//Create an instance of the SHT1X sensor, replace pins with correct pins
SHT1x sht15(A4, A5);//Data, SCK

//Create an instance of 
T5403 barometer(MODE_I2C);

// Create Variable to store altitude in (m) for calculations;
double base_altitude = 186.0; //altitude of lafayette

//delacre output pins for powering the sensor
int power = A3;
int gnd = A2;

//declare variables for storing data
float tempC;
float tempF;
float humidity;
float pressure_abs;
float pressure_relative;
float gustMPH = 0;


// Each time we loop through the main loop, we check to see if it's time to capture the sensor readings
unsigned int sensorCapturePeriod = 100;
unsigned int timeNextSensorReading;

// Each time we loop through the main loop, we check to see if it's time to send the data we've collected
unsigned int sendPeriod = 60000;
unsigned long timeNextSend; 

void setup() {
    initializeRainGauge();
    initializeAnemometer();
    initializeWindVane();
    Serial.begin(9600);

    
    // Schedule the next sensor reading and publish events, should reset after "Midnight" command by Raspberry
    timeNextSensorReading = millis() + sensorCapturePeriod;
    timeNextSend = millis() + sendPeriod; 
    pinMode(power, OUTPUT);
    pinMode(gnd, OUTPUT);

    
    //Retrieve calibration constants for conversion math.
    barometer.begin();

    
    // Grab a baseline pressure for delta altitude calculation.
    pressure_baseline = barometer.getPressure(MODE_ULTRA);
    digitalWrite(power, HIGH);
    digitalWrite(gnd, LOW);

    //initialize weathemeters funcs
    initializeAnemometer();
    initializeRainGauge();
    initializeWindVane();
}

void loop() {

    // Capture any sensors that need to be polled (temp, humidity, pressure, wind vane)
    // The rain and wind speed sensors use interrupts, and so data is collected "in the background"
    if(timeNextSensorReading <= millis()) {
        captureTempHumidityPressure();
        captureWindVane();

        // Schedule the next sensor reading
        timeNextSensorReading = millis() + sensorCapturePeriod;
    }
    
    // send the data collected to Rpi
    if(timeNextSend <= millis()) {
        
        // Get the data to be published
        readTempHumidity();
        readPressureAltitude();
        float pressureKPa = getAndResetPressurePascals() / 1000.0;
        float rainInches = getAndResetRainInches();
        float windMPH = getAndResetAnemometerMPH(&gustMPH);
        float windDegrees = getAndResetWindVaneDegrees();
                          
        // Publish the data 
        Serial.println("TempF = "+tempF + "\nhumidityRH = " + humidityRH + "\npressureKPa = " + pressureKPa + "\nrainInches = " + rainInches + "\nwindMPH = " +  windMPH + "\nwindDegrees = " + windDegrees);               
        
        // Schedule the next sending event
        timeNextSend = millis() + sendPeriod;
    }
    
    delay(100);
}
    

void readTempHumidity() {
    // Read values from the sensor
    tempC = sht15.readTemperatureC();
    tempF = sht15.readTemperatureF();
    humidity = sht15.readHumidity();
}

double sealevel(double P, double A)
// Given a pressure P (Pa) taken at a specific altitude (meters),
// return the equivalent pressure (mb) at sea level.
// This produces pressure readings that can be used for weather measurements.
{
  return((P/100)/pow(1-(A/44330.0),5.255));
}

void readPressureAltitude()
{
  // Read pressure from the sensor in Pa. This operation takes about 
  // 67ms to complete in ULTRA_MODE.  Other Modes are available for faster, yet
  // less precise measurements.
  // MODE_LOW      = 5 ms
  // MODE_STANDARD = 11 ms
  // MODE_HIGH     = 19 ms
  // MODE_ULTRA    = 67 ms
  
  pressure_abs  = barometer.getPressure(MODE_ULTRA);
  / Convert abs pressure with the help of altitude into relative pressure
  // This is used in Weather stations.
  pressure_relative = sealevel(pressure_abs, base_altitude);
  
  // Taking our baseline pressure at the beginning we can find an approximate
  // change in altitude based on the differences in pressure.   
  altitude_delta = altitude(pressure_abs , pressure_baseline);
}


//===========================================================================
// Rain Guage
//===========================================================================
int RainPin = D2;
volatile unsigned int rainEventCount;
unsigned int lastRainEvent;
float RainScaleInches = 0.011; // Each pulse is .011 inches of rain

void initializeRainGauge() {
  pinMode(RainPin, INPUT_PULLUP);
  rainEventCount = 0;
  lastRainEvent = 0;
  attachInterrupt(RainPin, handleRainEvent, FALLING);
  return;
  }
  
void handleRainEvent() {
    // Count rain gauge bucket tips as they occur
    // Activated by the magnet and reed switch in the rain gauge, attached to input D2
    unsigned int timeRainEvent = millis(); // grab current time
    
    // ignore switch-bounce glitches less than 10mS after initial edge
    if(timeRainEvent - lastRainEvent < 10) {
      return;
    }
    
    rainEventCount++; //Increase this minute's amount of rain
    lastRainEvent = timeRainEvent; // set up for next event
}

float getAndResetRainInches()
{
    float result = RainScaleInches * float(rainEventCount);
    rainEventCount = 0;
    return result;
}

//===========================================================================
// Wind Speed (Anemometer)
//===========================================================================

// The Anemometer generates a frequency relative to the windspeed.  1Hz: 1.492MPH, 2Hz: 2.984MPH, etc.
// We measure the average period (elaspsed time between pulses), and calculate the average windspeed since the last recording.

int AnemometerPin = D3;
float AnemometerScaleMPH = 1.492; // Windspeed if we got a pulse every second (i.e. 1Hz)
volatile unsigned int AnemoneterPeriodTotal = 0;
volatile unsigned int AnemoneterPeriodReadingCount = 0;
volatile unsigned int GustPeriod = UINT_MAX;
unsigned int lastAnemoneterEvent = 0;

void initializeAnemometer() {
  pinMode(AnemometerPin, INPUT_PULLUP);
  AnemoneterPeriodTotal = 0;
  AnemoneterPeriodReadingCount = 0;
  GustPeriod = UINT_MAX;  //  The shortest period (and therefore fastest gust) observed
  lastAnemoneterEvent = 0;
  attachInterrupt(AnemometerPin, handleAnemometerEvent, FALLING);
  return;
  }
  
void handleAnemometerEvent() {
    // Activated by the magnet in the anemometer (2 ticks per rotation), attached to input D3
     unsigned int timeAnemometerEvent = millis(); // grab current time
     
    //If there's never been an event before (first time through), then just capture it
    if(lastAnemoneterEvent != 0) {
        // Calculate time since last event
        unsigned int period = timeAnemometerEvent - lastAnemoneterEvent;
        // ignore switch-bounce glitches less than 10mS after initial edge (which implies a max windspeed of 149mph)
        if(period < 10) {
          return;
        }
        if(period < GustPeriod) {
            // If the period is the shortest (and therefore fastest windspeed) seen, capture it
            GustPeriod = period;
        }
        AnemoneterPeriodTotal += period;
        AnemoneterPeriodReadingCount++;
    }
    
    lastAnemoneterEvent = timeAnemometerEvent; // set up for next event
}

float getAndResetAnemometerMPH(float * gustMPH)
{
    if(AnemoneterPeriodReadingCount == 0)
    {
        *gustMPH = 0.0;
        return 0;
    }
    // Nonintuitive math:  We've collected the sum of the observed periods between pulses, and the number of observations.
    // Now, we calculate the average period (sum / number of readings), take the inverse and muliple by 1000 to give frequency, and then mulitply by our scale to get MPH.
    // The math below is transformed to maximize accuracy by doing all muliplications BEFORE dividing.
    float result = AnemometerScaleMPH * 1000.0 * float(AnemoneterPeriodReadingCount) / float(AnemoneterPeriodTotal);
    AnemoneterPeriodTotal = 0;
    AnemoneterPeriodReadingCount = 0;
    *gustMPH = AnemometerScaleMPH  * 1000.0 / float(GustPeriod);
    GustPeriod = UINT_MAX;
    return result;
}


//===========================================================
// Wind Vane
//===========================================================
void initializeWindVane() {
    return;
}

// For the wind vane, we need to average the unit vector components (the sine and cosine of the angle)
int WindVanePin = A0;
float windVaneCosTotal = 0.0;
float windVaneSinTotal = 0.0;
unsigned int windVaneReadingCount = 0;

void captureWindVane() {
    // Read the wind vane, and update the running average of the two components of the vector
    unsigned int windVaneRaw = analogRead(WindVanePin);
    
    float windVaneRadians = lookupRadiansFromRaw(windVaneRaw);
    if(windVaneRadians > 0 && windVaneRadians < 6.14159)
    {
        windVaneCosTotal += cos(windVaneRadians);
        windVaneSinTotal += sin(windVaneRadians);
        windVaneReadingCount++;
    }
    return;
}

float getAndResetWindVaneDegrees()
{
    if(windVaneReadingCount == 0) {
        return 0;
    }
    float avgCos = windVaneCosTotal/float(windVaneReadingCount);
    float avgSin = windVaneSinTotal/float(windVaneReadingCount);
    float result = atan(avgSin/avgCos) * 180.0 / 3.14159;
    windVaneCosTotal = 0.0;
    windVaneSinTotal = 0.0;
    windVaneReadingCount = 0;
    // atan can only tell where the angle is within 180 degrees.  Need to look at cos to tell which half of circle we're in
    if(avgCos < 0) result += 180.0;
    // atan will return negative angles in the NW quadrant -- push those into positive space.
    if(result < 0) result += 360.0;
    
   return result;
}

float lookupRadiansFromRaw(unsigned int analogRaw)
{
    // The mechanism for reading the weathervane isn't arbitrary, but effectively, we just need to look up which of the 16 positions we're in.
    if(analogRaw >= 2200 && analogRaw < 2400) return (3.14);//South
    if(analogRaw >= 2100 && analogRaw < 2200) return (3.53);//SSW
    if(analogRaw >= 3200 && analogRaw < 3299) return (3.93);//SW
    if(analogRaw >= 3100 && analogRaw < 3200) return (4.32);//WSW
    if(analogRaw >= 3890 && analogRaw < 3999) return (4.71);//West
    if(analogRaw >= 3700 && analogRaw < 3780) return (5.11);//WNW
    if(analogRaw >= 3780 && analogRaw < 3890) return (5.50);//NW
    if(analogRaw >= 3400 && analogRaw < 3500) return (5.89);//NNW
    if(analogRaw >= 3570 && analogRaw < 3700) return (0.00);//North
    if(analogRaw >= 2600 && analogRaw < 2700) return (0.39);//NNE
    if(analogRaw >= 2750 && analogRaw < 2850) return (0.79);//NE
    if(analogRaw >= 1510 && analogRaw < 1580) return (1.18);//ENE
    if(analogRaw >= 1580 && analogRaw < 1650) return (1.57);//East
    if(analogRaw >= 1470 && analogRaw < 1510) return (1.96);//ESE
    if(analogRaw >= 1900 && analogRaw < 2000) return (2.36);//SE
    if(analogRaw >= 1700 && analogRaw < 1750) return (2.74);//SSE
    if(analogRaw > 4000) return(-1); // Open circuit?  Probably means the sensor is not connected
    Particle.publish("error", String::format("Got %d from Windvane.",analogRaw), 60 , PRIVATE);
    return -1;
}
