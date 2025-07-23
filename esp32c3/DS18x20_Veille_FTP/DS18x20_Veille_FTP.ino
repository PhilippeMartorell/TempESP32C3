#include "Arduino.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <ESP32_FTPClient.h>
#include <OneWire.h>
#include <stdio.h>
#include "esp_sntp.h"

// Paramètres de mise en veille
#define uS_TO_S_FACTOR 1000000ULL                                        /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP 10 /* Time ESP32 will go to sleep (in seconds) */  // http://www.pjrc.com/teensy/td_libs_OneWire.html
//String NbBoot[6] ; // Nb de boots en chaine de caractères de longueur fixe


// Configuration du Wifi
// Frênes
//#define WIFI_SSID "BlueBox_2.4Ghz"
//#define WIFI_PASS "bonjoure"
// Arcachon
#define WIFI_SSID "Philippe"
#define WIFI_PASS "aqwzsxedc"
// Timeout connexion WiFi en secondes
#define ConstTimeOut 20

OneWire ds(4);  // on pin 10 (a 4.7K resistor is necessary)
RTC_DATA_ATTR int bootCount = 0;

// On configure le seveur NTP
const char* ntpServer = "fr.pool.ntp.org";
// Variables pour stocker la date/heure
String dateString = "";
String timeString = "";
String fullDateTime = "";

// Serveur FTP
// Frênes
//char ftp_server[] = "192.168.1.54";
// Arcachon
char ftp_server[] = "192.168.1.2";
const int ftp_port = 21;
char ftp_user[] = "rebooterie";
char ftp_pass[] = "rebooterie";
const char* ftp_remote_path = "/home/rebooterie/Esp/";  // Dossier distant (doit se terminer par /)
/*
char ftp_server[] = "91.169.47.199";
const int ftp_port = 21;
char ftp_user[] = "philippe";
char ftp_pass[] = "aqwzsxedc";
const char *ftp_remote_path = "/home/philippe/Esp/";  // Dossier distant (doit se terminer par /)
*/
// you can pass a FTP timeout and debbug mode on the last 2 arguments
ESP32_FTPClient ftp(ftp_server, ftp_user, ftp_pass, 5000, 2);


void setup(void) {
  Serial.begin(115200);
  delay(10000);  //Take some time to open up the Serial Monitor
  //Increment boot number and print it every reboot
  ++bootCount;
  Serial.println("Boot number: " + String(bootCount));

  // Config de la mise en veille
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) + " Seconds");



  // On configure le seveur NTP
  configTime(0, 0, ntpServer);



  // température
  byte i;
  byte present = 0;
  byte type_s;
  byte data[9];
  byte addr[8];
  float celsius;

  if (!ds.search(addr)) {
    Serial.println("No more addresses.");
    Serial.println();
    ds.reset_search();
    delay(250);
    return;
  }

  Serial.print("ROM =");
  for (i = 0; i < 8; i++) {
    Serial.write(' ');
    Serial.print(addr[i], HEX);
  }

  if (OneWire::crc8(addr, 7) != addr[7]) {
    Serial.println("CRC is not valid!");
    return;
  }
  Serial.println();

  // the first ROM byte indicates which chip
  switch (addr[0]) {
    case 0x10:
      Serial.println("  Chip = DS18S20");  // or old DS1820
      type_s = 1;
      break;
    case 0x28:
      Serial.println("  Chip = DS18B20");
      type_s = 0;
      break;
    case 0x22:
      Serial.println("  Chip = DS1822");
      type_s = 0;
      break;
    default:
      Serial.println("Device is not a DS18x20 family device.");
      return;
  }

  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1);  // start conversion, with parasite power on at the end

  delay(1000);  // maybe 750ms is enough, maybe not
  // we might do a ds.depower() here, but the reset will take care of it.

  present = ds.reset();
  ds.select(addr);
  ds.write(0xBE);  // Read Scratchpad

  Serial.print("  Data = ");
  Serial.print(present, HEX);
  Serial.print(" ");
  for (i = 0; i < 9; i++) {  // we need 9 bytes
    data[i] = ds.read();
    Serial.print(data[i], HEX);
    Serial.print(" ");
  }
  Serial.print(" CRC=");
  Serial.print(OneWire::crc8(data, 8), HEX);
  Serial.println();

  // Convert the data to actual temperature
  // because the result is a 16 bit signed integer, it should
  // be stored to an "int16_t" type, which is always 16 bits
  // even when compiled on a 32 bit processor.
  int16_t raw = (data[1] << 8) | data[0];
  if (type_s) {
    raw = raw << 3;  // 9 bit resolution default
    if (data[7] == 0x10) {
      // "count remain" gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  } else {
    byte cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00) raw = raw & ~7;       // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20) raw = raw & ~3;  // 10 bit res, 187.5 ms
    else if (cfg == 0x40) raw = raw & ~1;  // 11 bit res, 375 ms
    //// default is 12 bit resolution, 750 ms conversion time
  }
  celsius = (float)raw / 16.0;
  Serial.print("  Temperature = ");
  Serial.print(celsius);
  Serial.println(" Celsius ");


  // Connexion WiFi

  WiFi.begin(WIFI_SSID, WIFI_PASS);

  // Timeout
  int timeout = ConstTimeOut ;
  while ( WiFi.status() != WL_CONNECTED ) {
    delay(1000);
    Serial.print(".");
    timeout--;
    if ( timeout == 0 )
      {
        MiseEnSommeil() ;
      }
    }
  
  Serial.println("");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Relevé de l'heure
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) 
  {
    Serial.println("Erreur lors de la récupération de l'heure");
    dateString = "Erreur";
    timeString = "Erreur";
    fullDateTime = "Erreur";
    MiseEnSommeil() ;
  }

  // Buffer pour formater les chaînes
  char buffer[64];

  // 1. Variable pour la date seulement (format: JJ/MM/AAAA)
  strftime(buffer, sizeof(buffer), "%Y_%m_%d_", &timeinfo);
  dateString = String(buffer);
  // 2. Variable pour l'heure seulement (format: HH:MM:SS)
  strftime(buffer, sizeof(buffer), "%H:%M:%S", &timeinfo);
  timeString = String(buffer);
  // 3. Variable pour date et heure complète (format: JJ/MM/AAAA HH:MM:SS)
  strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M:%S", &timeinfo);
  fullDateTime = String(buffer);

  //Connexion FTP
  ftp.OpenConnection();

  // Get directory content
  ftp.InitFile("Type A");
  String list[128];
  ftp.ChangeWorkDir(ftp_remote_path);

  // Create the file new and write a string into it
  ftp.InitFile("Type A");

  //Céation du nom de fichier
  // Nb de boots longueur fixe
  char NbBoot[7];
  sprintf(NbBoot, "%05d", bootCount);
  String NomFichier = NbBoot ;
  NomFichier = String(dateString + NbBoot + ".csv");
  Serial.print("Nom du Fichier=>");
  Serial.print(NomFichier);
  Serial.println("<=");
  const char* NomFichierConstante = NomFichier.c_str();
  //  ftp.NewFile("MesuresTemp.txt");
  ftp.NewFile(NomFichierConstante);
  Serial.println(String(celsius, DEC));
  //char dateTxt[19];



  char mesureTxt[6];
  snprintf(mesureTxt, sizeof(mesureTxt), "%.2f", celsius);
  //char ligne[50];
  String ligne = String(timeString + ";" + mesureTxt);
  Serial.print("ligne =>");
  Serial.print(ligne);
  Serial.println("<=");
  const char* ligneConstante = ligne.c_str();
  ftp.Write(ligneConstante);
  ftp.CloseFile();

  ftp.CloseConnection();


  delay(60000);  //60 secondes pour pouvoir reprogrammer la bête !

  // mise en sommeil
MiseEnSommeil() ;

  
}


void loop(void) {
}

void MiseEnSommeil(void) {
  Serial.println("Going to sleep now");
  Serial.flush();
  esp_deep_sleep_start();
  Serial.println("This will never be printed");
}
