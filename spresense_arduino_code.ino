/*
Before uploading, read through the "Setting up the Spresense board" part.
*/

#include <SDHCI.h>
#include <Audio.h>
#include <GNSS.h>

#define STRING_BUFFER_SIZE  128
#define RESTART_CYCLE       (60 * 5)

//Change this to "Serial" to allow debugging messages to appear in the Serial Monitor for testing
//Remember to change it back to "Serial2" once it is time to test with the DonkeyCar
#define mySerial Serial2

//Size of the geo fence (in meters)
const float maxDistance = 70;

//Enter the latitudes and corresponding longitudes
float beaconLatitudes[] = {xxxxxxx, xxxxxxxx};
float beaconLongitudes[] = {xxxxxxxx, xxxxxxxx};
float currentLat, currentLong;

bool donePlaying;
bool ErrEnd = false;
bool pointInGeofence;
bool audioInitializedOnce = false;

int index = 0;
int currentFile;
int numOfBeacons;
int previousFile = 0;
int donkeyCarStopped = 0;
int currentBeaconPosition;

String fileName;

SDClass theSD;
static SpGnss Gnss;
AudioClass *theAudio;

File myFile;

enum ParamSat {
  eSatGps,            /**< GPS                     World wide coverage  */
  eSatGlonass,        /**< GLONASS                 World wide coverage  */
  eSatGpsSbas,        /**< GPS+SBAS                North America        */
  eSatGpsGlonass,     /**< GPS+Glonass             World wide coverage  */
  eSatGpsQz1c,        /**< GPS+QZSS_L1CA           East Asia & Oceania  */
  eSatGpsGlonassQz1c, /**< GPS+Glonass+QZSS_L1CA   East Asia & Oceania  */
  eSatGpsQz1cQz1S,    /**< GPS+QZSS_L1CA+QZSS_L1S  Japan                */
};

//Set this parameter depending on your current region
static enum ParamSat satType =  eSatGps;

void setup() {
  //put your setup code here, to run once:
  int error_flag = 0;

  //Set mySerial and Serial baudrate
  mySerial.begin(115200);
  Serial.begin(115200);

  //Compares if co-ordinates are valid and finds the number of beacons
  int elementsInArray1 = (sizeof(beaconLatitudes) / 4);
  int elementsInArray2 = (sizeof(beaconLongitudes) / 4);
  if (elementsInArray1 == elementsInArray2) {
    numOfBeacons = elementsInArray1;
    mySerial.print("Number of Beacons: ");
    mySerial.println(numOfBeacons);
  }
  else {
    while (1) {
      mySerial.println("Error! Invalid beacon co-ordinates!");
      delay(1000);
    }
  }

  //Initializing audio
  mySerial.println("Initializing Audio Library!");
  theAudio = AudioClass::getInstance();
  theAudio->begin(audio_attention_cb);
  theAudio->setRenderingClockMode(AS_CLKMODE_NORMAL);
  theAudio->setPlayerMode(AS_SETPLAYER_OUTPUTDEVICE_SPHP, AS_SP_DRV_MODE_LINEOUT);

  ledOn(PIN_LED0);
  ledOn(PIN_LED1);
  ledOn(PIN_LED2);
  ledOn(PIN_LED3);

  int result;
  result = Gnss.begin();
  if (result != 0)
  {
    mySerial.println("Gnss begin error!!");
    error_flag = 1;
  }
  else
  {
    switch (satType)
    {
      case eSatGps:
        Gnss.select(GPS);
        break;

      case eSatGpsSbas:
        Gnss.select(GPS);
        Gnss.select(SBAS);
        break;

      case eSatGlonass:
        Gnss.select(GLONASS);
        break;

      case eSatGpsGlonass:
        Gnss.select(GPS);
        Gnss.select(GLONASS);
        break;

      case eSatGpsQz1c:
        Gnss.select(GPS);
        Gnss.select(QZ_L1CA);
        break;

      case eSatGpsQz1cQz1S:
        Gnss.select(GPS);
        Gnss.select(QZ_L1CA);
        Gnss.select(QZ_L1S);
        break;

      case eSatGpsGlonassQz1c:
      default:
        Gnss.select(GPS);
        Gnss.select(GLONASS);
        Gnss.select(QZ_L1CA);
        break;
    }
    
    /* Start positioning */
    result = Gnss.start(COLD_START);
    if (result != 0)
    {
      mySerial.println("Gnss start error!!");
      error_flag = 1;
    }
    else
    {
      mySerial.println("Gnss setup OK");
    }
  }

  ledOff(PIN_LED0);
  ledOff(PIN_LED1);
  ledOff(PIN_LED2);
  ledOff(PIN_LED3);
}

void loop() {
  static int LoopCount = 0;
  static int LastPrintMin = 0;

  if (Gnss.waitUpdate(-1))
  {
    SpNavData NavData;
    Gnss.getNavData(&NavData);
    if (NavData.time.minute != LastPrintMin)
    {
      print_condition(&NavData);
      LastPrintMin = NavData.time.minute;
    }
    print_pos(&NavData);
  }
  else
  {
    mySerial.println("data not update");
  }

  LoopCount++;
  if (LoopCount >= RESTART_CYCLE)
  {
    int error_flag = 0;

    if (Gnss.stop() != 0)
    {
      mySerial.println("Gnss stop error!!");
      error_flag = 1;
    }
    else if (Gnss.end() != 0)
    {
      mySerial.println("Gnss end error!!");
      error_flag = 1;
    }
    else
    {
      mySerial.println("Gnss stop OK.");
    }

    if (Gnss.begin() != 0)
    {
      mySerial.println("Gnss begin error!!");
      error_flag = 1;
    }
    else if (Gnss.start(HOT_START) != 0)
    {
      mySerial.println("Gnss start error!!");
      error_flag = 1;
    }
    else
    {
      mySerial.println("Gnss restart OK.");
    }
    LoopCount = 0;
  }
}

static void audio_attention_cb(const ErrorAttentionParam *atprm)
{
  mySerial.println("Attention!");
  if (atprm->error_code >= AS_ATTENTION_CODE_WARNING)
  {
    ErrEnd = true;
  }
}

//Print position information
static void print_pos(SpNavData *pNavData)
{
  char StringBuffer[STRING_BUFFER_SIZE];

  snprintf(StringBuffer, STRING_BUFFER_SIZE, "%04d/%02d/%02d ", pNavData->time.year, pNavData->time.month, pNavData->time.day);
  mySerial.print(StringBuffer);

  snprintf(StringBuffer, STRING_BUFFER_SIZE, "%02d:%02d:%02d.%06d, ", pNavData->time.hour, pNavData->time.minute, pNavData->time.sec, pNavData->time.usec);
  mySerial.print(StringBuffer);

  snprintf(StringBuffer, STRING_BUFFER_SIZE, "numSat:%2d, ", pNavData->numSatellites);
  mySerial.print(StringBuffer);

  if (pNavData->posFixMode == FixInvalid)
  {
    mySerial.print("No-Fix, ");
  }
  else
  {
    mySerial.print("Fix, ");
  }
  if (pNavData->posDataExist == 0)
  {
    mySerial.print("No Position");
    ledOff(PIN_LED0);
  }
  else
  {
    mySerial.print("Lat=");
    mySerial.print(pNavData->latitude, 6);
    mySerial.print(", Lon=");
    mySerial.print(pNavData->longitude, 6);
    mySerial.println("");
    ledOn(PIN_LED0);

    currentLat = pNavData->latitude;
    currentLong = pNavData->longitude;

    //Checks if current position is in geofence; if yes it plays the appropriate file for that beacon
    pointInGeofence = isPointInGeofence(beaconLatitudes[index], beaconLongitudes[index], currentLat, currentLong);
    mySerial.print("Point in Geofence: ");
    mySerial.println(pointInGeofence);
    if (!pointInGeofence) {
      currentBeaconPosition = 0;
    } else {
      currentBeaconPosition = index + 1;
      currentFile = currentBeaconPosition;
      //Play one file only once
      if (previousFile != currentFile) {
        mySerial.println("Starting Audio Player");
        donePlaying = false;
        while (!donePlaying) {
          playFile();
          ledOn(PIN_LED3);
        }
        index++;
        ledOff(PIN_LED3);
      }
      previousFile = currentFile;
    }

    if (index == numOfBeacons) {
      index = 0;
    }

    mySerial.print("Currently at Beacon ");
    mySerial.println(currentBeaconPosition);
  }
  mySerial.println("");
}

//Print satellite condition.
static void print_condition(SpNavData * pNavData)
{
  char StringBuffer[STRING_BUFFER_SIZE];
  unsigned long cnt;

  snprintf(StringBuffer, STRING_BUFFER_SIZE, "numSatellites:%2d\n", pNavData->numSatellites);
  mySerial.print(StringBuffer);

  for (cnt = 0; cnt < pNavData->numSatellites; cnt++)
  {
    const char *pType = "---";
    SpSatelliteType sattype = pNavData->getSatelliteType(cnt);

    switch (sattype)
    {
      case GPS:
        pType = "GPS";
        break;

      case GLONASS:
        pType = "GLN";
        break;

      case QZ_L1CA:
        pType = "QCA";
        break;

      case SBAS:
        pType = "SBA";
        break;

      case QZ_L1S:
        pType = "Q1S";
        break;

      default:
        pType = "UKN";
        break;
    }

    unsigned long Id  = pNavData->getSatelliteId(cnt);
    unsigned long Elv = pNavData->getSatelliteElevation(cnt);
    unsigned long Azm = pNavData->getSatelliteAzimuth(cnt);
    float sigLevel = pNavData->getSatelliteSignalLevel(cnt);

    snprintf(StringBuffer, STRING_BUFFER_SIZE, "[%2d] Type:%s, Id:%2d, Elv:%2d, Azm:%3d, CN0:", cnt, pType, Id, Elv, Azm );
    mySerial.print(StringBuffer);
    mySerial.println(sigLevel, 6);
  }
}

//Check if point is in geofence. Returns 'true' if yes and 'false' if not
bool isPointInGeofence(float flat1, float flon1, float flat2, float flon2) {
  bool pointIn = false;
  float dist_calc = 0;
  float dist_calc2 = 0;
  float diflat = 0;
  float diflon = 0;

  diflat  = radians(flat2 - flat1);
  flat1 = radians(flat1);
  flat2 = radians(flat2);
  diflon = radians((flon2) - (flon1));
  dist_calc = (sin(diflat / 2.0) * sin(diflat / 2.0));
  dist_calc2 = cos(flat1);
  dist_calc2 *= cos(flat2);
  dist_calc2 *= sin(diflon / 2.0);
  dist_calc2 *= sin(diflon / 2.0);
  dist_calc += dist_calc2;
  dist_calc = (2 * atan2(sqrt(dist_calc), sqrt(1.0 - dist_calc)));
  dist_calc *= 6371000.0;
  if (dist_calc <= maxDistance) {
    pointIn = true;
  }
  return pointIn;
}

void playFile() {
  if (!audioInitializedOnce) {
    //Turn off GNSS before audio is being played (otherwise the Spresense board crashes)
    int result = Gnss.stop();
    if (result == 0) {
      mySerial.println("");
      mySerial.println("Successfully stopped GNSS");
    } else mySerial.println("Error stopping GNSS");

    //Prints '1' in the serial port so that DonkeyCar stops while audio is being played
    donkeyCarStopped = 1;
    Serial.println(donkeyCarStopped);

    err_t err = theAudio->initPlayer(AudioClass::Player0, AS_CODECTYPE_MP3, "/mnt/sd0/BIN", AS_SAMPLINGRATE_AUTO, AS_CHANNEL_STEREO);

    if (err != AUDIOLIB_ECODE_OK)
    {
      mySerial.println("Player0 initialize error\n");
      exit(1);
    }

    String mp3 = ".mp3";
    fileName = currentFile + mp3;
    myFile = theSD.open(fileName);

    if (!myFile)
    {
      mySerial.println("File open error");
      exit(1);
    }
    mySerial.print("Open: ");
    mySerial.println(myFile);

    err = theAudio->writeFrames(AudioClass::Player0, myFile);

    if ((err != AUDIOLIB_ECODE_OK) && (err != AUDIOLIB_ECODE_FILEEND))
    {
      mySerial.print("File Read Error: ");
      mySerial.print(err);
      myFile.close();
      exit(1);
    }

    mySerial.println("Play!");

    theAudio->setVolume(-80);
    theAudio->startPlayer(AudioClass::Player0);

    audioInitializedOnce = true;
    donePlaying = false;
  }

  mySerial.println("loop!!");

  int err = theAudio->writeFrames(AudioClass::Player0, myFile);

  if (err == AUDIOLIB_ECODE_FILEEND)
  {
    mySerial.println("Main player File End!");
  }

  if (err)
  {
    mySerial.print("Main player error code: ");
    mySerial.println(err);
    mySerial.println(" ");
    goto stop_player;
  }

  if (ErrEnd)
  {
    mySerial.println("Error End\n");
    goto stop_player;
  }

  usleep(40000);
  return;

stop_player:
  sleep(1);
  theAudio->stopPlayer(AudioClass::Player0);
  myFile.close();
  audioInitializedOnce = false;
  donePlaying = true;
  //Turn on GNSS once audio file is played
  int result = Gnss.start();
  if (result == 0) {
    mySerial.println("");
    mySerial.println("Successfully started GNSS");
  } else mySerial.println("Error starting GNSS");

  //Prints '0' to allow the DonkeyCar to continue once it has played the file
  donkeyCarStopped = 0;
  Serial.println(donkeyCarStopped);
}
