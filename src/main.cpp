/*
 * TimeRTC.pde
 * example code illustrating Time library with Real Time Clock.
 * 
 */

#include <Arduino.h>
#include <NMEAGPS.h>
#include <TimeLib.h>

#define TIME_HEADER  "T"   // Header tag for serial time sync message
#define TIME_REQUEST  "$"   // Header tag for serial time sync request message
#define gpsPort Serial3  // Alternatively, use Serial1 on the Leonardo, Mega or Due
#define GPS_BAUD 9600    // GPS module baud rate. GP3906 defaults to 9600.
NMEAGPS GPS;
gps_fix fix;

void digitalClockDisplay();
time_t getTeensy3Time();
unsigned long processSyncMessage();
void printDigits(int digits);
time_t request_time_sync();
bool time_set = false;
void printGPSInfo();
void printDate();
void printTime();

const int utc_offset = -4; // offset from UTC in hours
      bool          waitingForFix = false;
const unsigned long GPS_TIMEOUT   = 120000; // 2 minutes
      unsigned long gpsStart      = 0;
      unsigned long timeStart     = 0;

const int           GPSpower      = 10;

// interval for NTP set
const int NTP_INTERVAL = 1000 * 30;
uint32_t ntp_timer = 0;

// interval for GPS set
const int GPS_INTERVAL = 1000 * 30;
uint32_t gps_timer = 1000 * 15; //offset GPS set by 15s.

void setup()  {
  // set the Time library to use Teensy 3.0's RTC to keep time
  setSyncProvider(getTeensy3Time);

  Serial.begin(115200);
  Serial2.begin(115200);
  pinMode(GPSpower, OUTPUT);
  gpsPort.begin(GPS_BAUD);
  if (timeStatus()!= timeSet) {
    Serial.println("Unable to sync with the RTC");
  } else {
    Serial.println("RTC has set the system time");
  }
}

void loop() {
  bool turnGPSoff = false;

  if (millis() - ntp_timer > NTP_INTERVAL) {
    // ask for timestamp from ESP
    request_time_sync();
    Serial.println("Requesting NTP time");
    ntp_timer = millis();
  }

  // Is a time available from ESP?
  if (Serial2.available()) {
    time_t t = processSyncMessage();
    if (t != 0) {
      Teensy3Clock.set(t); // set the RTC
      setTime(t); // set TimeLib clock
      Serial.println("Time synced via NTP");
      time_set = true;
    }
  }

  // Is it time to turn on GPS and get fix/time?
  if (millis() - gps_timer > GPS_INTERVAL) {
      digitalWrite(GPSpower, HIGH);
      gpsStart      = millis();
      waitingForFix = true;
      gps_timer = millis();
  }

  // Is a GPS fix available?
  if (GPS.available( gpsPort )) {
    fix = GPS.read();

    if (waitingForFix) {
      //printGPSInfo();
      if(fix.valid.date && fix.valid.time) {
        //Serial.println(fix.dateTime);
        Teensy3Clock.set(fix.dateTime);
        setTime(fix.dateTime);
        adjustTime(utc_offset * SECS_PER_HOUR);
        Serial.println("Time synced via GPS");
      }
      Serial.println( millis() - gpsStart ); // DEBUG

      //if (fix.valid.satellites && (fix.satellites > 6)) {
      if(fix.valid.date && fix.valid.time) {
        waitingForFix = false;
        turnGPSoff    = true;
      }
    }
  }

  // Have we waited too long for a GPS fix?
  if (waitingForFix && (millis() - gpsStart > GPS_TIMEOUT)) {
    waitingForFix = false;
    turnGPSoff    = true;
  }

  if (turnGPSoff) {
    digitalWrite(GPSpower, LOW);
    Serial.println( F("GPS Toggled") );
  }


  // print time every second
  if (millis() - timeStart > 1000) {
    digitalClockDisplay();
    timeStart = millis();
  }
}

time_t getTeensy3Time()
{
  return Teensy3Clock.get();
}

/*  code to process time sync messages from the serial port   */
#define TIME_HEADER  "T"   // Header tag for serial time sync message
#define TIME_REQUEST  "$"   // Header tag for serial time sync request message

unsigned long processSyncMessage() {
  // TODO: handle errors
  // returned 2:36:40 7 2 2106 (4294971400) on first sync
  unsigned long pctime = 0L;
  const unsigned long DEFAULT_TIME = 1357041600; // Jan 1 2013 

  if(Serial2.find(TIME_HEADER)) {
     pctime = Serial2.parseInt();
     return pctime;
     if( pctime < DEFAULT_TIME) { // check the value is a valid time (greater than Jan 1 2013)
       pctime = 0L; // return 0 to indicate that the time is not valid
     }
  }
  return pctime;
}

// Send time sync request to ESP
time_t request_time_sync() {
  Serial2.write(TIME_REQUEST);  
  return 0; // the time will be sent later in response to serial mesg
}

void digitalClockDisplay() {
  // digital clock display of the time
  // make this a function that returns a formatted time string
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(day());
  Serial.print(" ");
  Serial.print(month());
  Serial.print(" ");
  Serial.print(year()); 
  Serial.println(); 
}

void printDigits(int digits){
  // utility function for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if(digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

void printGPSInfo()
{
  Serial.print( F("\nLat: ") );
  if (fix.valid.location)
    Serial.print( fix.latitude(), 6);
  Serial.print( F("\nLong: ") );
  if (fix.valid.location)
    Serial.print( fix.longitude(), 6);
  Serial.print( F("\nAlt: ") );
  if (fix.valid.altitude)
    Serial.print( fix.altitude() * 3.2808 );
  Serial.print( F("\nCourse: ") );
  if (fix.valid.heading)
    Serial.print(fix.heading());
  Serial.print( F("\nSpeed: ") );
  if (fix.valid.speed)
    Serial.print(fix.speed_mph());
  Serial.print( F("\nDate: ") );
  if (fix.valid.date)
    printDate();
  Serial.print( F("\nTime: ") );
  if (fix.valid.time)
    printTime();
  Serial.print( F("\nSats: ") );
  if (fix.valid.satellites)
    Serial.print(fix.satellites);
  Serial.println('\n');
}

void printDate()
{
  Serial.print(fix.dateTime.date);
  Serial.print('/');
  Serial.print(fix.dateTime.month);
  Serial.print('/');
  Serial.print(fix.dateTime.year); // or full_year()
}

void printTime()
{
  Serial.print(fix.dateTime.hours);
  Serial.print(':');
  if (fix.dateTime.minutes < 10) Serial.print('0');
  Serial.print(fix.dateTime.minutes);
  Serial.print(':');
  if (fix.dateTime.seconds < 10) Serial.print('0');
  Serial.print(fix.dateTime.seconds);
}