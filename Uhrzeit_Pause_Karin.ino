
/*
 * Projekt: Wordclock
 * Version: 1.01
 * Datum: 27.10.2021
 * Autor: SPSundTechnik
 * 
 * 
 * Changelog:
 * - Testdurchlauf der LED's während die WLAN-Verbindung hergestellt wird
 * - NTP-Server wird im Setup abgefragt
 * - Uhrzeit wird durch den Millis()-Timer berechnet
 * - NTP-Server wird nur noch stündlich abgefragt
 * - Ausgabe der Wörter mit neuem Array, welches die Zustandsänderung speichert
 * - Wörter werden hoch- bzw. herunter"gedimmt"
 * - LED's können in einem definierten Zeitfenster ausgeschaltet werden (Nachtmodus)
 * - korrekte Sommer-/Winterzeitumstellung bei Neustart der Uhr
 * - Uhr startet auch ohne WLAN-Verbindung
 * - Wort-Adressen werden bei Änderungam Layout korrekt zurückgesetzt
 * - Schnittstelle zum User-Interface
 * - UDP-Daten für NTP/UI können zusammen empfangen werden
 * - Stromaufnahme im Debug-Monitor
 * - Uhr wird ohne WLAN-Verbindung zum Access Point
 * 
*/


//Bibliotheken
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>


//Anzahl LED's und Ausgang vom ESP festlegen
#define PIN         12
#define NUMBERS     136

//*****************************************NEUE FUNKTION KARIN Geburtstag***************************************************
bool aktiv = false;
bool verzoegert = false;
int verzoegerung;


//******************************************************** PARAMETER ********************************************************


//********************************************************** WLAN ***********************************************************

//Zugangsdaten zum eigenen WLAN
//SSID
char Ssid_Sta[] = "WITTENSTEIN_HOTSPOT";
//Passwort
char Password_Sta[] = "WiFi4free";

//Zugangsdaten WordClock Access Point
//SSID
char Ssid_Ap[] = "WordClock";
//Passwort
char Password_Ap[] = "WordClock";

//***************************************************************************************************************************


//******************************************* FARBWERTE UND HELLIGKEIT DER LED's ********************************************

//Beispiel: RGB-Farbschema Rot = 50; Grün = 50; Blau = 50 => ergibt einen weißen Farbton.
//Tipp: Der blaue Anteil muss bei den WS2812B-LED's etwas reduziert werden, da dieser dominanter ausfällt.
//Helligkeit der LED's kann mit Werten zwischen 0 - 255 eingestellt werden.
//Beispiel 1: Rot = 255;  Grün = 255; Blau = 255 => weißer Farbton mit maximaler Helligkeit aller LED's
//Beispiel 2: Rot = 0;    Grün = 0;   Blau = 255 => blauer Farbton mir maximaler Helligkeit der blauen LED
//Beispiel 3: Rot = 127;  Grün = 127; Blau = 127 => weißer Farbton mit 50% der Helligkeit aller LED's

//Farbwert ROT
const unsigned int rgbvaluered = 83;
//Farbwert GRÜN
const unsigned int rgbvaluegreen = 83;
//Farbwert BLAU
const unsigned int rgbvalueblue = 58;

//***************************************************************************************************************************


//******************************************************** NACHTMODUS *******************************************************

//Display kann in einem definiertem Zeitfenster (24h-Format) ausgeschalten werden.
//Enable Display On/Off => Freigabe der Funktion
//Display Off => Uhrzeit wann das Display wieder ausgeschaltet werden soll
//Display On => Uhrzeit wann das Display wieder eingeschaltet werden soll

//Freigabe der Funktion
unsigned int enableDisplayOnOff = 0;
//Uhrzeit Display ausschalten
unsigned int displayOff = 0;
//Uhrzeit Display einschalten
unsigned int displayOn = 0;

//***************************************************************************************************************************


//***************************************************** LOKALE UHRZEIT ******************************************************

//Wird die Uhr ohne eine WLAN-Verbindung gestartet, kann mit den folgenden Parametern eine Uhrzeit eingestellt werden.
//Die Uhrzeit wird als UTC-Zeit eingestellt. Sommerzeit (UTC+2) oder Winterzeit (UTC+1) können eingestellt werden.

//Stundenzahl in UTC (24h-Format)
const unsigned int localutcaktstunde = 12;
//Minutenzahl in UTC
const unsigned int localutcaktminute = 30;
//Sekundenzahl in UTC
const unsigned int localutcaktsekunde = 0;
//Einstellung Sommer-/Winterzeit
const unsigned int localutcx = 1;

//***************************************************************************************************************************


//******************************************************** DIAGNOSE *********************************************************

//Serielle Schnittstelle für die Diagnose öffnen
const bool debug = LOW;

//***************************************************************************************************************************




//******************************************************* KONSTANTEN ********************************************************


//Lokale Portnummer für das UDP-Paket
const unsigned int localPort = 2390;


//Adresse des NTP-Serverpools
const char* NtpServerName = "de.pool.ntp.org";


//Größe des NTP-Datenpaket
const unsigned int NtpPaketSize = 48;
byte NtpPaketBuffer[NtpPaketSize];


//Größe des UserInterface-Datenpaket
const unsigned int UserInterfacePaketSize = 12;
byte UserInterfacePaketBuffer[UserInterfacePaketSize];


//ID-Nummern
const int UdpIdHandshakeRequest = 100;
const int UdpIdHandshakeAnswer = 101;
const int UdpIdRgb = 102;
const int UdpIdTime = 103;
const int UdpIdDate = 104;
const int UdpIdDisplay = 105;


//Zeitintervalle in Millisekunden
const unsigned long secondperday = 86400;
const unsigned long secondperhour = 3600;
const unsigned long secondperminute = 60;
const unsigned long minuteperhour = 60;
const unsigned long millisecondpersecond = 1000;
const unsigned long IntervallUdp = 20;
const unsigned long intervallDebug = 6000;
const unsigned long intervallFade = 4;


//Anzahl der Tage im Monat
unsigned int tageImMonat[13] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};


//Adresse der ersten und letzten LED
const int ersteLED = 0;
const int letzteLED = 113;


//Faktor zur Berechnung der Stromaufnahme
const float currentPerRgbValue = 0.079;


//Adressen der Stundenzahlen
const int STUNDENADR[12][2] = { {49, 53},   //Zwölf
                                {60, 63},   //Eins
                                {62, 65},   //Zwei
                                {67, 70},   //Drei
                                {77, 80},   //Vier
                                {73, 76},   //Fünf
                                {104, 108}, //Sechs
                                {55, 60},   //Sieben
                                {89, 92},   //Acht
                                {81, 84},   //Neun
                                {93, 96},   //Zehn
                                {85, 87} }; //Elf


//Adressen der Minutenzahlen
const int MINUTENADR[12][2] = { {7, 10},     //Fünf
                               {18, 21},    //Zehn
                               {26, 32},    //Viertel
                               {11, 17},    //Zwanzig
                               {44, 47},    //Halb
                               {38, 41},    //Nach
                               {35, 37},    //Vor
                               {99, 101},   //Uhr

//Adresse MinutenLEDs
                              {110, 110},     //min1
                              {110, 111},     //min2
                              {110, 112},     //min3
                              {110, 113}  };  //min4
                              

//***************************************************************************************************************************




//******************************************************** VARIABLEN ********************************************************

//Zähler for-Schleifen
int i = 0;
int z = 0;
int x = 0;
int y = 0;

unsigned long newsecsSince1900 = 0;
unsigned long oldsecsSince1900 = 0;

unsigned int oldstunde = 99;
unsigned int oldmezstunde = 0;
unsigned int oldminute = 0;

unsigned int utcaktstunde = 0;
unsigned int utcaktminute = 0;
unsigned int utcaktsekunde = 0;

unsigned int mezaktstunde = 0;
unsigned int mezaktstunde24 = 0;
unsigned int mezaktminute = 0;
unsigned int mezaktsekunde = 0;

unsigned long newtageSeit1900 = 0;
unsigned long oldtageSeit1900 = 0;
unsigned int newschaltTage = 0;

unsigned int wochenTag = 0;

unsigned int datumTag = 0;
unsigned int datumMonat = 0;
unsigned int datumJahr = 0;

unsigned int tagNummer = 0;

unsigned int utcx = 0;

unsigned int oldschaltTage = 0;
bool schaltJahr = LOW;
bool sonderFall366 = LOW;

unsigned long lastmillisDebug = 0;
unsigned long lastmillisSecond = 0;
unsigned long lastmillisdifferenz = 0;
unsigned long lastmillisFade = 0;
unsigned long LastMillisUdp = 0;

unsigned int UdpId = 0;

unsigned int UdpRgbRed = 0;
unsigned int UdpRgbGreen = 0;
unsigned int UdpRgbBlue = 0;

unsigned int UdpHour = 0;
unsigned int UdpMinute = 0;
unsigned int UdpSecond = 0;

unsigned int UdpDay = 0;
unsigned int UdpMonth = 0;
unsigned int UdpYear = 0;

unsigned int UdpEnableDisplay = 0;
unsigned int UdpDisplayOn = 0;
unsigned int UdpDisplayOff = 0;

unsigned int red = 0;
unsigned int green = 0;
unsigned int blue = 0;

float newredminus = 0.0;
float newgreenminus = 0.0;
float newblueminus = 0.0;

float newredplus = 0.0;
float newgreenplus = 0.0;
float newblueplus = 0.0;

float fadevaluered = 1.0;
float fadevaluegreen = 1.0;
float fadevalueblue = 1.0;

unsigned int currentPerLed = 0;

unsigned int startadresse = 0;
unsigned int endadresse = 0;

unsigned int minutenwert = 0;

unsigned int pointer = 0;

unsigned int newletter[114];
unsigned int showletter[114];
unsigned int changeletter[114];

bool newdisplay = LOW;
bool setDisplayOff = LOW;

bool NoWifi = LOW;

//***************************************************************************************************************************




//Objekte anlegen
//IP-Adressen
IPAddress TimeServerIp;
IPAddress UserInterfaceIp;

//Wifi UDP
WiFiUDP udp;

//Neopixel
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMBERS, PIN, NEO_GRB + NEO_KHZ800);




//SETUP
void setup() {


  //Serielle Schnittstelle für Debuging öffen
  if(debug == HIGH) {
    Serial.begin(115200);
  }
  

  //Leds initialisieren
  pixels.begin();
  

  //Array initialisieren
  for(i = ersteLED; i <= letzteLED; i++) {
    newletter[i] = 0;   
  }

  //Array initialisieren
  for(i = ersteLED; i <= letzteLED; i++) {
    showletter[i] = 0;   
  }

  //Array initialisieren
  for(i = ersteLED; i <= letzteLED; i++) {
    changeletter[i] = 2;   
  }


  //Start: Farbwerte zum Dimmen der LED's berechnen

  fadevaluered = FadeValueRed(rgbvaluered,rgbvaluegreen,rgbvalueblue);
  fadevaluegreen = FadeValueGreen(rgbvaluered,rgbvaluegreen,rgbvalueblue);
  fadevalueblue = FadeValueBlue(rgbvaluered,rgbvaluegreen,rgbvalueblue);
  
  //Farbwerte um x Nachkommastellen verschieben
  for(i = 0; i <= 10; i++) {

    if(fadevaluered < 1.0 && fadevaluegreen < 1.0 && fadevalueblue < 1.0){
      break;
    }

    fadevaluered = fadevaluered / 10.0;
    fadevaluegreen = fadevaluegreen / 10.0;
    fadevalueblue = fadevalueblue / 10.0;

  }

  //Ende: Farbwerte zum Dimmen der LED's berechnen
  

  //Start: Aufbau der WLAN-Verbindung
  
  //Wifi Verbindung aufbauen
  WiFi.mode(WIFI_STA);
  WiFi.begin(Ssid_Sta, Password_Sta);

  //Wartezeit damit die Verbindung aufgebaut wird
  delay(3000);

  //Kontrolle ob die Verbindung aufgebaut wurde
  do{

    //Keine WLAN-Verbindung, Uhr mit lokaler Zeit starten
    if(x >= 2){
      NoWifi = HIGH;
      break;      
    }
    
    
    //Start: Test alle LEDs bei Neustart der Uhr

    //Farbwerte übernehmen
    red = rgbvaluered;
    green = rgbvaluegreen;
    blue = rgbvalueblue;

    //Testdurchlauf => alle LEDs nacheinander einschalten
    for(i = ersteLED; i <= 135; i++) {
      
      if(i != 110 && i != 111 && i != 112 && i != 113){
        pixels.setPixelColor(i, pixels.Color(random(0,128),random(0,128),random(0,128)));
        pixels.show();
      }
      
      delay(50);
    }
    for(int i = 110; i < 114; i++){
      pixels.setPixelColor(i, pixels.Color(random(0,128),random(0,128),random(0,128)));
      pixels.show();
      delay(50);
    }
    

    //Testdurchlauf => alle LEDs umgekehrt ausschalten
    for(int i = 113; i >= 110; i--){
      pixels.setPixelColor(i, pixels.Color(0,0,0));
      pixels.show();
      delay(50);
    }
    for(i = 135; i >= ersteLED; i--) {

      //Farbwerte übernehmen
      red = 0;
      green = 0;
      blue = 0;

      //Wörter "ES" und "IST" schreiben
      if(i == 0 || i == 1 || i == 3 || i == 4 || i == 5){

        //Farbwerte übernehmen
        red = rgbvaluered;
        green = rgbvaluegreen;
        blue = rgbvalueblue;

        //"ES"
        newletter[0] = 1;
        newletter[1] = 1;

        //"IST"
        newletter[3] = 1;
        newletter[4] = 1;
        newletter[5] = 1;        
      }
      if(i != 110 && i != 111 && i != 112 && i != 113){
        pixels.setPixelColor(i, pixels.Color(red,green,blue));
        pixels.show();
      }
      
      delay(50);
    }

    //Ende: Test alle LEDs bei Neustart der Uhr
    

    //Anzahl der Durchläufe zählen
    x++;
    
  }while(WiFi.status() != WL_CONNECTED);

  //Ende: Aufbau der WLAN-Verbindung


  //Keine WLAN-Verbindung gefunden
  //Eigenen Access Point starten
  if(NoWifi == HIGH){

    //Wifi Verbindung aufbauen
    WiFi.softAP(Ssid_Ap, Password_Ap);

    //Lokale Uhrzeit als Startzeit setzen
    utcaktstunde = localutcaktstunde;
    utcaktminute = localutcaktminute;
    utcaktsekunde = localutcaktsekunde;
    utcx = localutcx;

    //UDP-Protokoll
    udp.begin(localPort);
    
  }

  //WLAN-Verbindung aufgebaut
  else{
    
    //UDP-Protokoll
    udp.begin(localPort);

    //Start: NTP-Anfrage und NTP-Antwort

    //Zeitwerte vom NTP-Server abrufen
    do{
      SendNtpRequest();
      UdpDataReceive();
    }while(newsecsSince1900 == 0);
  
    //Ende: NTP-Anfrage und NTP-Antwort
    
    
    //Systemzeit speichern für den nächsten Vergleich
    lastmillisSecond = millis();


    //Berechnung der Jahreszahl
    datumJahr = datumJahr_calc(newsecsSince1900);


    //Berechnung der Anzahl der Schalttage seit dem 1. Januar 1900
    for(i = 1900; i < datumJahr; i++){
    
      //Aufruf der Funktion zur Erkennung ob ein Schaltjahr vorliegt
      schaltJahr = schaltJahrJN(i);
        
      if(schaltJahr == HIGH){
        newschaltTage++;
      }
    }

    newschaltTage = newschaltTage - 1;
    
  }

  //Arrays kopieren
  for(i = ersteLED; i <= letzteLED; i++) {
     showletter[i] = newletter[i];  
  }
  
  //Farbwerte übernehmen
  newredminus = rgbvaluered;
  newgreenminus = rgbvaluegreen;
  newblueminus = rgbvalueblue;
  UdpRgbRed = rgbvaluered;
  UdpRgbGreen = rgbvaluered;
  UdpRgbBlue = rgbvalueblue;

  
  //Lokale Einstellungen übernehmen
  UdpEnableDisplay = enableDisplayOnOff;
  UdpDisplayOn = displayOn;
  UdpDisplayOff = displayOff;

 Serial.begin(9600);
   
}


//LOOP
void loop() {
  

  //Start: UDP-Datenpakete empfangen
  
  //UDP-Daten in einem festen Zeitintervall abfragen
  if(millis() - LastMillisUdp > IntervallUdp && newdisplay == LOW){
    
    //Systemzeit speichern für den nächsten Vergleich
    LastMillisUdp = millis();

    UdpDataReceive();
  }

  //Ende: UDP-Datenpakete empfangen

  

  //Start: NTP-Anfrage
  
  //Zeitwerte bei jedem Stundenwechsel aktualisieren
  if (mezaktstunde != oldmezstunde && newdisplay == LOW && NoWifi == LOW) {

    //Zeit für den nächsten Vergleich speichern
    oldmezstunde = mezaktstunde;

    //Zeitwerte vom NTP-Server anfragen
    SendNtpRequest();
      
  }

  //Ende: NTP-Anfrage
  


  //Start: Datum und Uhrzeit ermitteln
  
  //Kontrolle ob eine Änderung bei dem Zeitstempel stattgefunden hat
  if(newsecsSince1900 > oldsecsSince1900) {

    //Änderung abspeichern für den nächsten Vergleich
    oldsecsSince1900 = newsecsSince1900;
    
    //Tage seit dem 1. Januar 1900; 00:00:00 Uhr
    newtageSeit1900 = newsecsSince1900 / secondperday;
    

    //Start: Datum ermitteln

    //Kontrolle ob eine Änderung bei den Tagen seit dem 1. Januar 1900 stattgefunden hat
    if(newtageSeit1900 > oldtageSeit1900) {

      //Änderung abspeichern für den nächsten Vergleich
      oldtageSeit1900 = newtageSeit1900;


      //Aufruf der Funktion zur Berechnung des Wochentages
      wochenTag = wochenTag_calc(newtageSeit1900);


      //Berechnug der Jahreszahl
      datumJahr = ((newtageSeit1900 - newschaltTage) / 365.002f) + 1900;
      

      //Aufruf der Funktion zur Erkennung ob ein Schaltjahr vorliegt
      schaltJahr = schaltJahrJN(datumJahr);
      

      //Funktion fürs Schaltjahr
      if(schaltJahr == HIGH) {

        tageImMonat[2] = 29;


        //Anzahl der Schalttage seit dem 1. Januar 1900 !ERST! am Ende des jeweiligen Schaltjahres hochzählen
        //Sonderfall: Zur Berechnung des 366. Tages wird der "alte" Wert der Schalttage benötigt, die Berechnung
        //            der Jahreszahl am 366. Tag muss aber mit dem neuen Wert durchgeführt werden

        //Berechnung der Anzahl der Schalttage seit dem 1. Januar 1900
        if(datumMonat == 12 && datumTag == 30) {

          oldschaltTage = newschaltTage;

          newschaltTage = newschaltTage + 1;
        
          sonderFall366 = HIGH;
        }
      }

      else {
        tageImMonat[2] = 28;
      }


      //Berechnung der wieviele Tag im Jahr 
      //Sonderfall: Die Berechnung des 366. Tages im Schaltjahr muss mit dem "alten" Wert der Schalttage (seit 1900) berechnet werden
      if(sonderFall366 == HIGH) {

        tagNummer = (newtageSeit1900 - ((datumJahr - 1900) * 365) - oldschaltTage);

        sonderFall366 = LOW;
      }

      //Ansonsten muss mit dem aktuellen Wert der Schalttage (seit 1900) gerechnet werden
      else {      
        tagNummer = (newtageSeit1900 - ((datumJahr - 1900) * 365) - newschaltTage);
      }

      //Aufruf der Funktion zur Berechnung des Monates
      datumMonat = welcherMonat(tagNummer, schaltJahr);

      datumTag = 0;

      //Berechnung der bereits vergangenen Tagen im Jahr
      for(i = 0; i < datumMonat; i++) {      
        datumTag = datumTag + tageImMonat[i];
      }

      //Tag-Nummer minus bereits vergangener Tage im Jahr = aktueller Tag
      datumTag = tagNummer - datumTag;
      
    }

    //Ende: Datum ermitteln

    

    //Start: Sommer-/ Winterzeit einstellen
    
    //am letzten Sonntag im März (03) wird von UTC+1 auf UTC+2 umgestellt; 02:00 -> 03:00
    //am letzten Sonntag im Oktober (10) wird von UTC+2 auf UTC+1 umgestellt; 03:00 -> 02:00

    //Aufruf jeden Sonntag im März
    if(datumMonat == 3 && wochenTag == 7) {

      //Letzter Sonntag im März
      if((tageImMonat[3] - datumTag) < 7) {
        
        //Umstellung auf Sommerzeit UTC+2
        utcx = 2;
      }
    }

    //Aufruf jeden Sonntag im Oktober
    if(datumMonat == 10 && wochenTag == 7) {

      //Letzter Sonntag im Oktober
      if((tageImMonat[10] - datumTag) < 7) {
        
        //Umstellung auf Winterzeit UTC+1
        utcx = 1;
      }
    }

    //Sommer-/Winterzeit bei Neustart bestimmen
    if(utcx == 0){

      //Neustart im Monat März
      if(datumMonat == 3){

        //Neustart in der letzten Woche im März
        if((tageImMonat[3] - datumTag) < 7){

          //Beispiel Jahr 2020: März 31 Tage; Neustart 26. März 2020 (Donnerstag = Wochentag = 4); 5 Tage Rest; Letzter Sonntag 29. März 2020
          //Berechnung: 31 - 26 = 5; 5 + 4 = 9;
          //Ergebnis: Letzter Tag im März ist ein Dienstag. Es folgt noch ein weiterer Sonntag im Oktober => Winterzeit einstellen
          
          //Beispiel Jahr 2021: März 31 Tage; Neustart 30. März 2021 (Dienstag = Wochentag = 2); 1 Tage Rest; Letzter Sonntag 28. März 2021
          //Berechnung: 31 - 30 = 1; 1 + 2 = 3;
          //Ergebnis: Letzter Tag im März ist ein Mittwoch. Umstellung auf Sommerzeit bereits erfolgt => Sommerzeit einstellen

          //Es folgt innerhalb der letzten Woche im März noch ein weiterer Sonntag => Winterzeit einstellen
          if(((tageImMonat[3] - datumTag) + wochenTag) >= 7){
            utcx = 1;
          }

          //Letzter Sonntag im März bereits vergangen => Sommerzeit einstellen
          else{
            utcx = 2;
          }
        }

        //Neustart in den ersten drei Wochen im März => Winterzeit einstellen
        else{
          utcx = 1;
        }
      }

      //Neustart im Monat Oktober
      if(datumMonat == 10){

        //Neustart in der letzten Woche im Oktober
        if((tageImMonat[10] - datumTag) < 7){

          //Beispiel Jahr 2020: Oktober 31 Tage; Neustart 26. Oktober 2020 (Montag = Wochentag = 1); 5 Tage Rest; Letzter Sonntag 25. Oktober 2020
          //Berechnung: 31 - 26 = 5; 5 + 1 = 6;
          //Ergebnis: Letzter Tag im Oktober ist ein Samstag. Umstellung auf Winterzeit bereits erfolgt => Winterzeit einstellen
          
          //Beispiel Jahr 2021: Oktober 31 Tage; Neustart 26. Oktober 2021 (Dienstag = Wochentag = 2); 5 Tage Rest; Letzter Sonntag 31. Oktober 2021
          //Berechnung: 31 - 26 = 5; 5 + 2 = 7;
          //Ergebnis: Letzter Tag im Oktober ist ein Sonntag. Es folgt noch ein weiterer Sonntag im Oktober => Sommerzeit einstellen

          //Es folgt innerhalb der letzten Woche im Oktober noch ein weiterer Sonntag => Sommerzeit einstellen
          if(((tageImMonat[10] - datumTag) + wochenTag) >= 7){
            utcx = 2;
          }

          //Letzter Sonntag im Oktober bereits vergangen => Winterzeit einstellen
          else{
            utcx = 1;
          }
        }

        //Neustart in den ersten drei Wochen im Oktober => Sommerzeit einstellen
        else{
          utcx = 2;
        }
      }

      //Falls Neustart der Uhr innerhalb der Sommerzeit (Ausnahme März/Oktober)
      if(datumMonat > 3 && datumMonat < 10) {      
        utcx = 2;
      }

      //Falls Neustart der Uhr innerhalb der Winterzeit (Ausnahme März/Oktober)
      if(datumMonat < 3 || datumMonat > 10) {    
        utcx = 1;
      }        
    }

    //Ende: Sommer-/ Winterzeit einstellen
    


    //Start: Aktuelle Uhrzeit hh:mm:ss aus dem NTP-Paket ermitteln
    
    //Beispiel: Berechnung der aktuellen Stunde 
    //Sekunden seit dem 1. Januar 1900 (3812811794) / Sekunden pro Tag (86400) = 44129 volle Tage seit dem 1. Januar 1900
    //Tage seit dem 1. Januar (44129) * Sekunden pro Tag (86400) = 3812745600 Sekunden der vollen Tage seit dem 1. Januar 1900
    //Sekunden seit dem 1. Januar 1900 (3812811794) - Sekunden der vollen Tage (3812745600) = 66194 Sekunden am aktuellen Tag vergangen
    //Sekunden des aktuellen Tages (66194) / Sekunden pro Stunde (3600) = 18 Stunden am aktuellen Tag vergangen => Aktuelle Uhrzeit 18:mm:ss

    //Berechnung der aktuellen Stunde
    utcaktstunde = ((newsecsSince1900  % secondperday) / secondperhour);

    //Berechnung der aktuellen Minute
    utcaktminute = (newsecsSince1900  % secondperhour) / secondperminute;

    //Berechnung der aktuellen Sekunde
    utcaktsekunde = (newsecsSince1900 % secondperminute);

    //Ende: Aktuelle Uhrzeit hh:mm:ss aus dem NTP-Paket ermitteln
       
  }

  //Ende: Datum und Uhrzeit ermitteln



  //Start: Uhrzeit über den millis()-Timer berechnen

  //Alle 1000 ms die Sekunden hochzählen
  if (millis() - lastmillisSecond > millisecondpersecond){

    //Zeitdifferenz
    lastmillisdifferenz = lastmillisdifferenz + ((millis() - lastmillisSecond) - millisecondpersecond);

    if(lastmillisdifferenz > millisecondpersecond){
      lastmillisdifferenz = 0;
      utcaktsekunde++;
    }
    
    //Systemzeit speichern für den nächsten Vergleich
    lastmillisSecond = millis();

    utcaktsekunde++;

  }

  //Alle 60 s den Minutenzähler hochzählen
  if(utcaktsekunde >= secondperminute){

    //Sekunden zurücksetzen
    utcaktsekunde = 0;

    //Minuten hochzählen
    utcaktminute++;
    
  }

  //Alle 60 min den Stundenzähler hochzählen
  if(utcaktminute >= minuteperhour){

    //Minuten zurücksetzen
    utcaktminute = 0;

    //Stunden hochzählen
    utcaktstunde++;

    //Wechsel von 24 Uhr zu 1 Uhr
    if(utcaktstunde == 25){
      utcaktstunde = 1;
    }

  }

  //Ende: Uhrzeit über den millis()-Timer berechnen



  //Start: Von UTC nach MEZ

  //Sekunden und Minuten unverändert von UTC übernehmen
  mezaktminute = utcaktminute;
  mezaktsekunde = utcaktsekunde;
  
  //Umrechnung von UTC nach MEZ mittels Zeitverschiebung
  mezaktstunde = utcaktstunde + utcx;

  //Aktuelle Stundenzahl in MEZ als 24h Format speichern
  mezaktstunde24 = mezaktstunde; 

  //Nach "Halb" Stundenzahl erhöhen => Beispiel: 16:25:00 => Fünf vor halb Fünf
  if(/*mezaktminute >= 15 && mezaktminute <= 19 ||*/ mezaktminute >= 25 && mezaktminute <= 59 ) {    
    mezaktstunde = mezaktstunde + 1;
  }
  
  //Beispiel: UTC             = 22:15:00
  //          MEZ (Winter +1) = 23:15:00 => 23:15:00 => 11:15:00 => viertel nach Elf
  //          MEZ (Sommer +2) = 24:15:00 => 00:15:00 => viertel nach Zwölf
  
  //Beispiel: UTC             = 23:15:00
  //          MEZ (Winter +1) = 24:15:00 => 00:15:00 => viertel nach Zwölf
  //          MEZ (Sommer +2) = 25:15:00 => 01:15:00 => viertel nach Eins

  //Problemstellung 1: Stundenzahlen über 23 abfangen
  if(mezaktstunde >= 24){
    mezaktstunde = mezaktstunde - 24;
  }
  

  //Beispiel: UTC             = 10:47:00
  //          MEZ (Winter +1) = 11:15:00 => 11:15:00 => viertel nach Elf
  //          MEZ (Sommer +2) = 12:15:00 => 00:15:00 => viertel nach Zwölf
  
  //Beispiel: UTC             = 11:47:00
  //          MEZ (Winter +1) = 12:15:00 => 00:15:00 => viertel nach Zwölf
  //          MEZ (Sommer +2) = 13:15:00 => 01:15:00 => viertel nach Eins

  //Problemstellung 2: Stundenzahlen auf den Bereich "Eins" bis "Zwölf" begrenzen
  if(mezaktstunde >= 12){
    mezaktstunde = mezaktstunde - 12;
  }

  //Ende: Von UTC nach MEZ

  

  //Start: Stundenzahl ausgeben
  
  if(mezaktstunde != oldstunde) {

    //Stundenzahl für den nächsten Vergleich speichern
    oldstunde = mezaktstunde;

    //Bereich für Stundenzahlen auf Null setzen
    for(i = 0; i <= 11; i++) {

      startadresse = STUNDENADR[i][0];
      endadresse = STUNDENADR[i][1];

      for(y = startadresse; y <= endadresse; y++) {
        newletter[y] = 0;
      }
    }

    //Start- und Endadresse für Ausgabe bestimmen
    startadresse = STUNDENADR[mezaktstunde][0];
    endadresse = STUNDENADR[mezaktstunde][1];

    //Neues Wort schreiben
    for(i = startadresse; i <= endadresse; i++) {      
          newletter[i] = 1;
    }
  }

  //Ende: Stundenzahl ausgeben


  
  //Start: Minutenzahl ausgeben
  
  if(mezaktminute != oldminute) {

    //Pointer zurücksetzen
    pointer = 99;
    
    //Minutenzahl für den nächsten Vergleich speichern
    oldminute = mezaktminute;

    //Bereich für Minutenzahlen auf Null setzen
    for(i = 0; i <= 11; i++) {

      startadresse = MINUTENADR[i][0];
      endadresse = MINUTENADR[i][1];

      for(y = startadresse; y <= endadresse; y++) {
        newletter[y] = 0;
      }
    }

    //Fünf
    if(mezaktminute >= 5 && mezaktminute <= 9 || mezaktminute >= 55 && mezaktminute <= 59) {
      pointer = 0;
    }

    //Zehn
    if(mezaktminute >= 10 && mezaktminute <= 14 || mezaktminute >= 50 && mezaktminute <= 54) {
      pointer = 1;
    }

    //Viertel
    if(mezaktminute >= 15 && mezaktminute <= 19 || mezaktminute >= 45 && mezaktminute <= 49) {
      pointer = 2;
    }

    //Zwanzig
    if(mezaktminute >= 20 && mezaktminute <= 24 || mezaktminute >= 40 && mezaktminute <= 44) {
      pointer = 3;
    }

    //Halb
    if(mezaktminute >= 30 && mezaktminute <= 34) {
      pointer = 4;
    }
    
    //Ausgabe der aktuellen Minutenzahl
    if(pointer >= 0 && pointer <= 4) {
      startadresse = MINUTENADR[pointer][0];
      endadresse = MINUTENADR[pointer][1];
      
      for(i = startadresse; i <= endadresse; i++) {      
        newletter[i] = 1;
      }
    }

    //Minutenwert berechnen
      minutenwert = mezaktminute % 5;

      if(minutenwert == 1) {
    pointer = 8;
    startadresse = MINUTENADR[pointer][0];
        endadresse = MINUTENADR[pointer][1];
        
        for(i = startadresse; i <= endadresse; i++) {      
            newletter[i] = 1;
      }
   }

    if(minutenwert == 2) {
      pointer = 9;
      startadresse = MINUTENADR[pointer][0];
          endadresse = MINUTENADR[pointer][1];
          
          for(i = startadresse; i <= endadresse; i++) {      
              newletter[i] = 1;
        }
     }

    if(minutenwert == 3) {
    pointer = 10;
    startadresse = MINUTENADR[pointer][0];
        endadresse = MINUTENADR[pointer][1];
        
        for(i = startadresse; i <= endadresse; i++) {      
            newletter[i] = 1;
      }
   }

   if(minutenwert == 4) {
    pointer = 11;
    startadresse = MINUTENADR[pointer][0];
        endadresse = MINUTENADR[pointer][1];
        
        for(i = startadresse; i <= endadresse; i++) {      
            newletter[i] = 1;
      }
   }
    

    //Fünf vor/nach Halb
    if(mezaktminute >= 25 && mezaktminute <= 29 || mezaktminute >= 35 && mezaktminute <= 39) {

      //Fünf
      pointer = 0;
      startadresse = MINUTENADR[pointer][0];
      endadresse = MINUTENADR[pointer][1];
      
      for(i = startadresse; i <= endadresse; i++) {      
          newletter[i] = 1;
      }

      //Halb
      pointer = 4;
      startadresse = MINUTENADR[pointer][0];
      endadresse = MINUTENADR[pointer][1];
      
      for(i = startadresse; i <= endadresse; i++) {      
          newletter[i] = 1;
      }
    }


    //Wort "Nach"
    if(mezaktminute >= 5 && mezaktminute <= 24 || mezaktminute >= 35 && mezaktminute <= 39) {
      pointer = 5;
    }

    //Wort "Vor"
    if(mezaktminute >= 25 && mezaktminute <= 29 || mezaktminute >= 40 && mezaktminute <= 59) {
      pointer = 6;
    }


    //Ausgabe der aktuellen Minutenzahl
    if(pointer == 5 || pointer == 6) {
      startadresse = MINUTENADR[pointer][0];
      endadresse = MINUTENADR[pointer][1];
      
      for(i = startadresse; i <= endadresse; i++) {      
        newletter[i] = 1;
      }
    }


    //Wort "Uhr"
    if(mezaktminute == 0 || mezaktminute == 1 || mezaktminute == 2 || mezaktminute == 3 || mezaktminute == 4) {  
      pointer = 7;
      startadresse = MINUTENADR[pointer][0];
      endadresse = MINUTENADR[pointer][1];
      
      for(i = startadresse; i <= endadresse; i++) {      
          newletter[i] = 1;
      }

      //Sonderfall: Ein / Ein(s)
      if(mezaktstunde == 1) {
        endadresse = STUNDENADR[1][0];      
        newletter[endadresse] = 0;
      }
    }

    //Sonderfall: Ein / Ein(s)
    if(mezaktminute >= 5 && mezaktminute <= 24) {

      if(mezaktstunde == 1) {
        endadresse = STUNDENADR[1][0];      
        newletter[endadresse] = 1;
      }
    }    
  }

  //Ende: Minutenzahl ausgeben
  


  //Start: Altes Bild mit neuem Bild vergleichen

  for(i = ersteLED; i <= letzteLED; i++){
    
    //showletter = 1; newletter = 0 => Wort ausschalten
    if(showletter[i] > newletter[i]){
      newdisplay = HIGH;
      changeletter[i] = 0;
    }
    
    //showletter = 0; newletter = 1 => Wort einschalten
    if(showletter[i] < newletter[i]){
      newdisplay = HIGH;
      changeletter[i] = 1;
    }
    
    if(showletter[i] == newletter[i]){
      changeletter[i] = 2;
    }
  }

  //Ende: Altes Bild mit neuem Bild vergleichen
  


  //Start: LED-Display in einem definierten Zeitfenster ausschalten
  
  if(UdpEnableDisplay == 1){

    //Beispiel: Display On: 5 Uhr; Display Off: 2 Uhr
    if(UdpDisplayOff < UdpDisplayOn){
      if(mezaktstunde24 >= UdpDisplayOff && mezaktstunde24 < UdpDisplayOn){ 
        
        for(i = ersteLED; i <= letzteLED; i++){

          //Angezeigte Wörter im ersten Aufruf ausschalten
          if(setDisplayOff == LOW){
            if(showletter[i] == 1 || newletter[i] == 1){
              changeletter[i] = 0;
              newdisplay = HIGH;
            }
          }

          //Display im Zeitfenster ausgeschaltet lassen
          else{
            changeletter[i] = 2;
            showletter[i] = 0;
            newdisplay = LOW;
          }        
        }
      }
      
      else{
        setDisplayOff = LOW;           
      }
    }
    
    //Beispiel: Display On: 6 Uhr; Display Off: 23 Uhr
    else{
      if(mezaktstunde24 >= UdpDisplayOff || mezaktstunde24 < UdpDisplayOn){
        
        for(i = ersteLED; i <= letzteLED; i++){

          //Angezeigte Wörter im ersten Aufruf ausschalten
          if(setDisplayOff == LOW){
            if(showletter[i] == 1 || newletter[i] == 1){
              changeletter[i] = 0;
              newdisplay = HIGH;
            }
          }
          
          //Display im Zeitfenster ausgeschaltet lassen
          else{
            changeletter[i] = 2;
            showletter[i] = 0;
            newdisplay = LOW;
          }        
        }
      }
      
      else{
        setDisplayOff = LOW;           
      }      
    }  
  }

  else{
    setDisplayOff = LOW;           
  }

  //Ende: LED-Display in einem definierten Zeitfenster ausschalten



  //Start: Ausgabe des Bildes

  if(newdisplay == HIGH) {

    //Farbwerte in dem eingestellten Intervall dimmen
    if (millis() - lastmillisFade > intervallFade) {

      //Systemzeit speichern für den nächsten Vergleich
      lastmillisFade = millis();

      //Farbwerte reduzieren "herunterdimmen"     
      if(newredminus > 0.0){
        newredminus = newredminus - fadevaluered;
        if(newredminus <= 0.0){
          newredminus = 0.0;
          newgreenminus = 0.0;
          newblueminus = 0.0;
        }
      }

      //Farbwerte reduzieren "herunterdimmen"   
      if(newgreenminus > 0.0){
        newgreenminus = newgreenminus - fadevaluegreen;
        if(newgreenminus <= 0.0){
          newredminus = 0.0;
          newgreenminus = 0.0;
          newblueminus = 0.0;
        }
      }

      //Farbwerte reduzieren "herunterdimmen"   
      if(newblueminus > 0.0){
        newblueminus = newblueminus - fadevalueblue;
        if(newblueminus <= 0.0){
          newredminus = 0.0;
          newgreenminus = 0.0;
          newblueminus = 0.0;
        }
      }

      //Farbwerte erhöhen "hochdimmen"     
      if(newredplus < UdpRgbRed){
        newredplus = newredplus + fadevaluered;
        if(newredplus >= UdpRgbRed){
          newredplus = UdpRgbRed;
          newgreenplus = UdpRgbGreen; 
          newblueplus = UdpRgbBlue;                
        }
      }

      //Farbwerte erhöhen "hochdimmen"
      if(newgreenplus < UdpRgbGreen){
        newgreenplus = newgreenplus + fadevaluegreen;
        if(newgreenplus >= UdpRgbGreen){
          newredplus = UdpRgbRed;
          newgreenplus = UdpRgbGreen; 
          newblueplus = UdpRgbBlue;                   
        }
      }

      //Farbwerte erhöhen "hochdimmen"
      if(newblueplus < UdpRgbBlue){
        newblueplus = newblueplus + fadevalueblue;
        if(newblueplus >= UdpRgbBlue){ 
          newredplus = UdpRgbRed;
          newgreenplus = UdpRgbGreen; 
          newblueplus = UdpRgbBlue;                 
        }
      }

      //Neue Farbwerte an die LED's übermitteln
      for(i = ersteLED; i <= letzteLED; i++){

        red = newredminus;
        green = newgreenminus;
        blue = newblueminus;

        if(changeletter[i] == 0){
          pixels.setPixelColor(i, pixels.Color(red,green,blue)); 
        }

        red = newredplus;
        green = newgreenplus;
        blue = newblueplus;
        
        if(changeletter[i] == 1){
          pixels.setPixelColor(i, pixels.Color(red,green,blue));
        }
        
      }

      //LED's aktualisieren
      pixels.show();

    }

    
    //Bild wurde komplett aktualisiert
    if(newredminus <= 0.0 && newgreenminus <= 0.0 && newblueminus <= 0.0 && newredplus >= UdpRgbRed && newgreenplus >= UdpRgbGreen && newblueplus >= UdpRgbBlue){
      
      //Arrays kopieren für den nächsten Vergleich
      for(i = ersteLED; i <= letzteLED; i++) {
        showletter[i] = newletter[i];  
      }
      

      //Start: Aktuelle Stromaufnahme ermitteln
      
      currentPerLed = 0;
      
      //Aktuelle Stromfaufnahme mittels RGB-Wert und Anzahl der aktiven LED's ermitteln
      //Die Stromaufnahme wird dabei nicht gemessen, sondern mit einem Faktor und der Anzahl der aktiven LED's berechnet
      for(i = ersteLED; i <= letzteLED; i++){
        
        if(showletter[i] == 1){
          currentPerLed = currentPerLed + round((float)(currentPerRgbValue * (red + green + blue)));
        }
        
      }
      
      //Ende: Aktuelle Stromaufnahme ermitteln
      

      //Aktualisierung des Bildes beenden
      newdisplay = LOW;
      setDisplayOff = HIGH;
      

      //Farbwerte für die nächste Aktualisierung zurücksetzen
      
      newredminus = UdpRgbRed;
      newgreenminus = UdpRgbGreen;
      newblueminus = UdpRgbBlue;
      newredplus = 0.0;
      newgreenplus = 0.0;
      newblueplus = 0.0;
    }
      
  }

  //Ende: Ausgabe des Bildes


  
  //Start: Ausgabe fürs Debuging
 
  if(debug == HIGH) {

    if (millis() - lastmillisDebug > intervallDebug) {
      
      //Systemzeit speichern für den nächsten Vergleich
      lastmillisDebug = millis();

      Serial.print("lokale IP-Adresse: ");
      Serial.println(WiFi.localIP());

      Serial.print("NTP-Paket: ");
      Serial.println(newsecsSince1900);

      Serial.print("Zeitdifferenz: ");
      Serial.print(lastmillisdifferenz);
      Serial.println("ms");

      Serial.print("Uhrzeit UTC: ");
      Serial.print(utcaktstunde);
      Serial.print(":");
      Serial.print(utcaktminute);
      Serial.print(":");
      Serial.println(utcaktsekunde);

      Serial.print("Zeitverschiebung: ");
      Serial.println(utcx);
 
      Serial.print("Uhrzeit MEZ: ");
      Serial.print(mezaktstunde24);
      Serial.print(":");
      Serial.print(mezaktminute);
      Serial.print(":");
      Serial.println(mezaktsekunde);

      Serial.print("Datum: ");
      Serial.print(datumTag);
      Serial.print(".");
      Serial.print(datumMonat);
      Serial.print(".");
      Serial.println(datumJahr);

      Serial.print("Wochentag: ");
      Serial.println(wochenTag);

      Serial.print("Schaltjahr: ");
      Serial.println(schaltJahr);

      Serial.print("Anzahl Schalttage: ");
      Serial.println(newschaltTage);

      Serial.print("Nachtmodus aktiv: ");
      Serial.println(UdpEnableDisplay);

      Serial.print("Display ausschalten um: ");
      Serial.print(UdpDisplayOff);
      Serial.println(" Uhr");

      Serial.print("Display einschalten um: ");
      Serial.print(UdpDisplayOn);
      Serial.println(" Uhr");

      Serial.print("Display Aus: ");
      Serial.println(setDisplayOff);

      Serial.print("RGB-Farbwert Rot: ");
      Serial.print(UdpRgbRed);
      Serial.println("/255");

      Serial.print("RGB-Farbwert Grün: ");
      Serial.print(UdpRgbGreen);
      Serial.println("/255");

      Serial.print("RGB-Farbwert Blau: ");
      Serial.print(UdpRgbBlue);
      Serial.println("/255");

      Serial.print("Faktor Dimmen Rot: ");
      Serial.println(fadevaluered);

      Serial.print("Faktor Dimmen Grün: ");
      Serial.println(fadevaluegreen);

      Serial.print("Faktor Dimmen Blau: ");
      Serial.println(fadevalueblue);

      Serial.print("Stromaufnahme: ");
      Serial.print(currentPerLed);
      Serial.println("mA");

      Serial.println();
      
    }  
  }

  //Ende: Ausgabe fürs Debuging

  //Pause Karin
 /* int HHAN;
  int MMAN;
  int SSAN;

  int HHAUS;
  int MMAUS;
  int SSAUS;
  //12 Uhr Pause
  for(int i = 0; UhrzeitAn[i] != "" && UhrzeitAus[i] != ""; i++){
    HHAN = UhrzeitAn[i].substring(0,2).toInt();
    MMAN = UhrzeitAn[i].substring(3,5).toInt();
    SSAN = UhrzeitAn[i].substring(6,8).toInt();

    HHAUS = UhrzeitAus[i].substring(0,2).toInt();
    MMAUS = UhrzeitAus[i].substring(3,5).toInt();
    SSAUS = UhrzeitAus[i].substring(6,8).toInt();

    
    Serial.println("HHAN: " + String(HHAN));
    Serial.println("MMAN: " + String(MMAN));
    Serial.println("SSAN: " + String(SSAN));
    //15:10:10
    //15:15:10
    //14:59:59
    
      if(mezaktstunde24 >= HHAN && mezaktstunde24 <= HHAUS){ //Stunde größer oder gleich
       if((mezaktstunde24 > HHAN || utcaktminute >= MMAN) && (mezaktstunde24 < HHAUS ||  utcaktminute <= MMAUS)){ //Stunde größer oder minute größer oder gleich
         if((mezaktstunde24 > HHAN || utcaktminute > MMAN || utcaktsekunde >= SSAN) && (mezaktstunde24 < HHAUS ||  utcaktminute < MMAUS || utcaktsekunde < SSAUS)){  //Stunde größer oder minute größer oder sekunde größer gleich
           pixels.setPixelColor(114, pixels.Color(red,green,blue));
           pixels.setPixelColor(115, pixels.Color(red,green,blue));
           pixels.setPixelColor(116, pixels.Color(red,green,blue));
           pixels.setPixelColor(117, pixels.Color(red,green,blue));
           pixels.setPixelColor(119, pixels.Color(red,green,blue));
           pixels.setPixelColor(120, pixels.Color(red,green,blue));
           pixels.setPixelColor(121, pixels.Color(red,green,blue));
           pixels.setPixelColor(123, pixels.Color(red,green,blue));
           pixels.setPixelColor(124, pixels.Color(red,green,blue));
           pixels.setPixelColor(125, pixels.Color(red,green,blue));
           pixels.setPixelColor(126, pixels.Color(red,green,blue));
           pixels.setPixelColor(127, pixels.Color(red,green,blue));
           pixels.setPixelColor(128, pixels.Color(red,green,blue));
           pixels.setPixelColor(129, pixels.Color(red,green,blue));
           pixels.setPixelColor(131, pixels.Color(red,green,blue));
           pixels.setPixelColor(132, pixels.Color(red,green,blue));
           pixels.setPixelColor(133, pixels.Color(red,green,blue));
           pixels.setPixelColor(134, pixels.Color(red,green,blue));
           pixels.setPixelColor(135, pixels.Color(red,green,blue));
           pixels.show();
         }else{
             for(int i = 114; i < 136; i++){
              pixels.setPixelColor(i, pixels.Color(0,0,0));
              pixels.show(); 
             }
          }
       }else{
           for(int i = 114; i < 136; i++){
            pixels.setPixelColor(i, pixels.Color(0,0,0));
            pixels.show(); 
           }    
        }
    }else{
        for(int i = 114; i < 136; i++){
         pixels.setPixelColor(i, pixels.Color(0,0,0));
         pixels.show(); 
        }
     }
  }
*/
 

 if(mezaktstunde24 == 12 && utcaktminute >= 0 && utcaktminute < 45 && aktiv == false){
  if(verzoegert == false){
   verzoegerung = millis();
   verzoegert = true;
  }
  if(millis() - verzoegerung > 5000){ aktiv = true; verzoegert = false;}
   pixels.setPixelColor(114, pixels.Color(red,green,blue));
   pixels.setPixelColor(115, pixels.Color(red,green,blue));
   pixels.setPixelColor(116, pixels.Color(red,green,blue));
   pixels.setPixelColor(117, pixels.Color(red,green,blue));
   pixels.setPixelColor(119, pixels.Color(red,green,blue));
   pixels.setPixelColor(120, pixels.Color(red,green,blue));
   pixels.setPixelColor(121, pixels.Color(red,green,blue));
   pixels.setPixelColor(123, pixels.Color(red,green,blue));
   pixels.setPixelColor(124, pixels.Color(red,green,blue));
   pixels.setPixelColor(125, pixels.Color(red,green,blue));
   pixels.setPixelColor(126, pixels.Color(red,green,blue));
   pixels.setPixelColor(127, pixels.Color(red,green,blue));
   pixels.setPixelColor(128, pixels.Color(red,green,blue));
   pixels.setPixelColor(129, pixels.Color(red,green,blue));
   pixels.setPixelColor(131, pixels.Color(red,green,blue));
   pixels.setPixelColor(132, pixels.Color(red,green,blue));
   pixels.setPixelColor(133, pixels.Color(red,green,blue));
   pixels.setPixelColor(134, pixels.Color(red,green,blue));
   pixels.setPixelColor(135, pixels.Color(red,green,blue));
   pixels.show();
 }

  if(mezaktstunde24 == 12 && utcaktminute == 45 && aktiv == true){
    
    if(verzoegert == false){
     verzoegerung = millis();
     verzoegert = true;
    }
  if(millis() - verzoegerung > 5000){ aktiv = false; verzoegert = false;}
   for(int i = 114; i < 136; i++){
    pixels.setPixelColor(i, pixels.Color(0,0,0));
    pixels.show();
   }
 }



if(mezaktstunde24 == 9 && utcaktminute >= 0 && utcaktminute < 15 && aktiv == false){
  if(verzoegert == false){
   verzoegerung = millis();
   verzoegert = true;
  }
  if(millis() - verzoegerung > 5000){ aktiv = true; verzoegert = false;}
   pixels.setPixelColor(114, pixels.Color(red,green,blue));
   pixels.setPixelColor(115, pixels.Color(red,green,blue));
   pixels.setPixelColor(116, pixels.Color(red,green,blue));
   pixels.setPixelColor(117, pixels.Color(red,green,blue));
   pixels.setPixelColor(119, pixels.Color(red,green,blue));
   pixels.setPixelColor(120, pixels.Color(red,green,blue));
   pixels.setPixelColor(121, pixels.Color(red,green,blue));
   pixels.setPixelColor(123, pixels.Color(red,green,blue));
   pixels.setPixelColor(124, pixels.Color(red,green,blue));
   pixels.setPixelColor(125, pixels.Color(red,green,blue));
   pixels.setPixelColor(126, pixels.Color(red,green,blue));
   pixels.setPixelColor(127, pixels.Color(red,green,blue));
   pixels.setPixelColor(128, pixels.Color(red,green,blue));
   pixels.setPixelColor(129, pixels.Color(red,green,blue));
   pixels.setPixelColor(131, pixels.Color(red,green,blue));
   pixels.setPixelColor(132, pixels.Color(red,green,blue));
   pixels.setPixelColor(133, pixels.Color(red,green,blue));
   pixels.setPixelColor(134, pixels.Color(red,green,blue));
   pixels.setPixelColor(135, pixels.Color(red,green,blue));
   pixels.show();
 }

  if(mezaktstunde24 == 9 && utcaktminute == 15 && aktiv == true){
    
    if(verzoegert == false){
     verzoegerung = millis();
     verzoegert = true;
    }
  if(millis() - verzoegerung > 5000){ aktiv = false; verzoegert = false;}
   for(int i = 114; i < 136; i++){
    pixels.setPixelColor(i, pixels.Color(0,0,0));
    pixels.show();
   }
 }

 


 

    
  
  
}



//Funktion UDP-Daten empfangen
void UdpDataReceive(){ 
  unsigned int PaketSize = 0;  
  PaketSize = udp.parsePacket();
  if(PaketSize != 0){
    if(PaketSize == NtpPaketSize){
      udp.read(NtpPaketBuffer, NtpPaketSize);
      newsecsSince1900 = GetNtpTimestamp(NtpPaketBuffer);
    }
    else if(PaketSize == UserInterfacePaketSize){
      udp.read(UserInterfacePaketBuffer, UserInterfacePaketSize);
      UserInterface(UserInterfacePaketBuffer);
    }
    else{
      //Nothing to do here
    }
  }  
}



//Funktion Nutzerdaten aus UDP-Daten auslesen
int UserInterfaceGetValue(byte Data[], int PointerStart, int PointerEnd){
  int Value = 0;
  Value = Data[PointerStart];
  Value |= Data[PointerEnd] << 8;
  return Value;
}



//Funktion Handshake
bool HandshakeOk(byte Data[]){
  bool HandshakeOk = HIGH;
  int HandshakeOkCount = 0;
  for(HandshakeOkCount = 2; HandshakeOkCount < 12; HandshakeOkCount++){
    if(Data[HandshakeOkCount] != 0){
      HandshakeOk = LOW;
    }
  }
  return HandshakeOk;
}



//Funktion Nutzerdaten übergeben
void UserInterface(byte UserInterfaceData[]) {

  //ID-Nummer
  UdpId = UserInterfaceGetValue(UserInterfaceData,0,1);
  
  switch(UdpId){
    
    case UdpIdHandshakeRequest:
      if(HandshakeOk(UserInterfaceData)){
        UserInterfaceData[0] = UdpIdHandshakeAnswer;
        UserInterfaceIp = udp.remoteIP();
        udp.beginPacket(UserInterfaceIp, localPort);
        udp.write(UserInterfaceData, 12);
        udp.endPacket();
      }
    break;
    
    case UdpIdRgb:
      UdpRgbRed = UserInterfaceGetValue(UserInterfaceData,2,3);
      UdpRgbGreen = UserInterfaceGetValue(UserInterfaceData,4,5);
      UdpRgbBlue = UserInterfaceGetValue(UserInterfaceData,6,7);
      newredminus = UdpRgbRed;
      newgreenminus = UdpRgbGreen;
      newblueminus = UdpRgbBlue;
      fadevaluered = FadeValueRed(UdpRgbRed,UdpRgbGreen,UdpRgbBlue);
      fadevaluegreen = FadeValueGreen(UdpRgbRed,UdpRgbGreen,UdpRgbBlue);
      fadevalueblue = FadeValueBlue(UdpRgbRed,UdpRgbGreen,UdpRgbBlue); 

      //Farbwerte um x Nachkommastellen verschieben 
      for(i = 0; i <= 10; i++) {

        if(fadevaluered < 1.0 && fadevaluegreen < 1.0 && fadevalueblue < 1.0){
          break;
        }

        fadevaluered = fadevaluered / 10.0;
        fadevaluegreen = fadevaluegreen / 10.0;
        fadevalueblue = fadevalueblue / 10.0;

      }

      //Arrays kopieren für den nächsten Vergleich
      for(i = ersteLED; i <= letzteLED; i++) {
        if(showletter[i] == 1){
          pixels.setPixelColor(i, pixels.Color(UdpRgbRed,UdpRgbGreen,UdpRgbBlue));
        }  
      }

      //LED's aktualisieren
      pixels.show();
            
    break;
    
    case UdpIdTime:
      UdpHour = UserInterfaceGetValue(UserInterfaceData,2,3);
      UdpMinute = UserInterfaceGetValue(UserInterfaceData,4,5);
      UdpSecond = UserInterfaceGetValue(UserInterfaceData,6,7);
      if(NoWifi == HIGH){
        utcaktstunde = UdpHour;
        utcaktminute = UdpMinute;
        utcaktsekunde = UdpSecond;
        utcx = 0;
      }
    break;
    
    case UdpIdDate:
      UdpDay = UserInterfaceGetValue(UserInterfaceData,2,3);
      UdpMonth = UserInterfaceGetValue(UserInterfaceData,4,5);
      UdpYear = UserInterfaceGetValue(UserInterfaceData,6,7);
    break;

    case UdpIdDisplay:
      UdpEnableDisplay = UserInterfaceGetValue(UserInterfaceData,2,3);
      UdpDisplayOn = UserInterfaceGetValue(UserInterfaceData,4,5);
      UdpDisplayOff = UserInterfaceGetValue(UserInterfaceData,6,7);
      setDisplayOff = LOW;
    break;
  }

}



//Funktion Dimmwerte Rot berechnen
float FadeValueRed(int Red, int Green, int Blue){
  float Value = 1.0;
  
  if(Red == 0){
    Value = 0.0;
    return Value;
  }

  //Ab hier hat Rot einen Wert

  //Drei Werte vorhanden
  if(Green > 0 && Blue > 0){
    if(Green <= Red && Green <= Blue){
      Value = (float)Red / Green;
    }
    if(Blue <= Red && Blue <= Green){
      Value = (float)Red / Blue;
    }
    return Value; 
  }

  //Ab hier haben entweder Grün oder Blau eine Null oder Beide

  //Rot und Grün mit Werten; Blau muss Null sein
  if(Green > 0 && Green < Red){
    Value = (float)Red / Green;
    return Value; 
  }

  //Ab hier hat Grün eine Null und Blau fraglich

  //Rot und Blau
  if(Blue > 0 && Blue < Red){
    Value = (float)Red / Blue;
    return Value; 
  }

  //Ab hier nur Rot vorhanden oder Rot ist kleinster Wert
  return Value; 
}



//Funktion Dimmwerte Grün berechnen
float FadeValueGreen(int Red, int Green, int Blue){
  float Value = 1.0;
  
  if(Green == 0){
    Value = 0.0;
    return Value;
  }

  //Ab hier hat Grün einen Wert

  //Drei Werte vorhanden
  if(Red > 0 && Blue > 0){
    if(Red <= Green && Red <= Blue){
      Value = (float)Green / Red;
    }
    if(Blue <= Green && Blue <= Red){
      Value = (float)Green / Blue;
    }
    return Value; 
  }

  //Ab hier haben entweder Rot oder Blau eine Null oder Beide

  //Grün und Rot mit Werten; Blau muss Null sein
  if(Red > 0 && Red < Green){
    Value = (float)Green / Red;
    return Value; 
  }

  //Ab hier hat Rot eine Null und Blau fraglich

  //Grün und Blau
  if(Blue > 0 && Blue < Green){
    Value = (float)Green / Blue;
    return Value; 
  }

  //Ab hier nur Grün vorhanden oder Grün ist kleinster Wert
  return Value;  
}



//Funktion Dimmwerte Blau berechnen
float FadeValueBlue(int Red, int Green, int Blue){
  float Value = 1.0;
  
  if(Blue == 0){
    Value = 0.0;
    return Value;
  }

  //Ab hier hat Blau einen Wert

  //Drei Werte vorhanden
  if(Red > 0 && Green > 0){
    if(Red <= Blue && Red <= Green){
      Value = (float)Blue / Red;
    }
    if(Green <= Blue && Green <= Red){
      Value = (float)Blue / Green;
    }
    return Value; 
  }

  //Ab hier haben entweder Rot oder Grün eine Null oder Beide

  //Blau und Rot mit Werten; Grün muss Null sein
  if(Red > 0 && Red < Blue){
    Value = (float)Blue / Red;
    return Value; 
  }

  //Ab hier hat Rot eine Null und Grün fraglich

  //Blau und Grün
  if(Green > 0 && Green < Blue){
    Value = (float)Blue / Green;
    return Value; 
  }
 
  //Ab hier nur Blau vorhanden oder Blau ist kleinster Wert
  return Value;
}



//Funktion NTP-Daten abrufen
void SendNtpRequest() {

  //Verbindung zu einem NTP-Server aus dem Pool aufbauen
  WiFi.hostByName(NtpServerName, TimeServerIp);

  //NTP-Anfrage an den Server stellen
  SendNtpPaket(TimeServerIp);

  delay(50);
  
}



//Funktion NTP-Zeitstempel auslesen
long GetNtpTimestamp(byte NtpData[]){
  
  unsigned long NtpTimestamp = 0;
  
  //Der Zeitstempel befindet sich im Byte 40, 41, 42, 43
  //Zeitstempel aus den vier Bytes auslesen => Ergebnis: Sekunden seit dem 1. Januar 1900, 00:00:00 Uhr 
  NtpTimestamp = word(NtpData[40], NtpData[41]) << 16 | word(NtpData[42], NtpData[43]);
  return NtpTimestamp;
}



//Funktion zur Anfrage eines NTP-Paketes
void SendNtpPaket(IPAddress& address) {
  memset(NtpPaketBuffer, 0, NtpPaketSize);
  
  //Erstes Byte
  //Leap Indicator: Bit 0, 1 => 11 => 3 => Schaltsekunde wird nicht synchronisiert
  //Version: Bit Bit 2, 3, 4 => 000 => 0 => Version xy?
  //Mode: Bit 5, 6, 7 => 111 => 7 => für den Privaten Gebrauch
  NtpPaketBuffer[0] = 0b11100011;
  
  //Zweites Byte
  //Stratum: Bit 0 - 7 => 00000000 => 0 => nicht angegeben
  NtpPaketBuffer[1] = 0;

  //Drittes Byte
  //Poll: Bit 0 - 7 => 00000110 => 6 => Abfrageintervall
  NtpPaketBuffer[2] = 6;

  //Viertes Byte
  //Precision: Bit 0 - 7 => 0xEC => Taktgenauigkeit
  NtpPaketBuffer[3] = 0xEC;

  //Die nächsten 8 Byte haben nur für Servernaachrichten eine Bedeutung
  //8 Bytes => 0

  NtpPaketBuffer[12]  = 49;
  NtpPaketBuffer[13]  = 0x4E;
  NtpPaketBuffer[14]  = 49;
  NtpPaketBuffer[15]  = 52;

  udp.beginPacket(address, 123);
  udp.write(NtpPaketBuffer, NtpPaketSize);
  udp.endPacket();
}



//Funktion zur Berechnung des Wochentages
//Montag = 1, Dienstag = 2, Mittwoch = 3, Donnerstag = 4, Freitag = 5, Samstag = 6, Sonntag = 7
unsigned int wochenTag_calc(unsigned long tage1900) {

  //Der 1. Januar 1900 war ein Montag
  unsigned int ergebnis = 1;
  
  int for_i = 0;

  for(for_i = 0; for_i < tage1900; for_i++) {
    
    if(ergebnis < 7) {
      ergebnis = ergebnis + 1;
    }
    
    else {
      ergebnis = 1;
    }
  }

  return ergebnis;
}



//Funktion zur Berechnung der Jahreszahl
unsigned int datumJahr_calc(unsigned long sec1900) {

  //NTP beginnt am 1. Januar 1900
  unsigned int ergebnis = 1900;
  unsigned int tageimjahr = 0;
  unsigned int tage = 0;
  unsigned int tage1900 = 0;

  int for_i = 0;
  bool schaltjahr = LOW;

  tage1900 = sec1900 / 86400;

  for(for_i = 0; for_i < tage1900; for_i++){

    schaltjahr = schaltJahrJN(ergebnis);

    if(schaltjahr){
      tageimjahr = 366;
    }
    
    else{
      tageimjahr = 365;
    }

    tage++;

    if(tage >= tageimjahr){
      ergebnis++;
      tage = 0;
    }
    
  }

  return ergebnis;
  
}



//Funktion zur Erkennung ob ein Schaltjahr vorliegt
bool schaltJahrJN(unsigned int jahr) {

  bool ergebnis_jn = LOW;
  
  //Erkennung von Schaltjahren
  if((jahr % 4) == 0) {
    
    ergebnis_jn = HIGH;

    if((jahr % 100) == 0) {
      
      ergebnis_jn = LOW;

      if((jahr % 400) == 0) {
        
        ergebnis_jn = HIGH;
      }
    }
  }

  else {
    ergebnis_jn = LOW;
  }

  return ergebnis_jn;
}



//Funktion zur Berechnung des Monates
int welcherMonat(int tg_nr, bool schaltjn) {

  //Monatsanfänge
  int monatMin[13] = {0, 1, 32, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335};
  //Monatsenden
  int monatMax[13] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365};

  int datum = 0;

  int y = 0;

  //Berechnung des Anfang und Ende jeden Montas im Schaltjahr
  if(schaltjn == HIGH) {

    for(y = 3; y < 13; y++) {
      monatMin[y] = monatMin[y] + 1;
    }

    for(y = 2; y < 13; y++) {
      monatMax[y] = monatMax[y] + 1;
    }
  }

  //Monat Januar
  if(tg_nr >= monatMin[1] && tg_nr <= monatMax[1]) {
    datum = 1;
  }

  //Monat Februar
  if(tg_nr >= monatMin[2] && tg_nr <= monatMax[2]) {
    datum = 2;
  }

  //Monat März
  if(tg_nr >= monatMin[3] && tg_nr <= monatMax[3]) {
    datum = 3;
  }

  //Monat April
  if(tg_nr >= monatMin[4] && tg_nr <= monatMax[4]) {
    datum = 4;
  }

  //Monat Mai
  if(tg_nr >= monatMin[5] && tg_nr <= monatMax[5]) {
    datum = 5;
  }

  //Monat Juni
  if(tg_nr >= monatMin[6] && tg_nr <= monatMax[6]) {
    datum = 6;
  }

  //Monat Juli
  if(tg_nr >= monatMin[7] && tg_nr <= monatMax[7]) {
    datum = 7;
  }

  //Monat August
  if(tg_nr >= monatMin[8] && tg_nr <= monatMax[8]) {
    datum = 8;
  }

  //Monat September
  if(tg_nr >= monatMin[9] && tg_nr <= monatMax[9]) {
    datum = 9;
  }

  //Monat Oktober
  if(tg_nr >= monatMin[10] && tg_nr <= monatMax[10]) {
    datum = 10;
  }

  //Monat November
  if(tg_nr >= monatMin[11] && tg_nr <= monatMax[11]) {
    datum = 11;
  }

  //Monat Dezember
  if(tg_nr >= monatMin[12] && tg_nr <= monatMax[12]) {
    datum = 12;
  }

  return datum;
}
