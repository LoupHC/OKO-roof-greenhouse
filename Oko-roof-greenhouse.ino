#include <SPI.h>
#include <SD.h>
#include <Ethernet.h>
#include "Wire.h"
#include "DHT.h"
#define DS3231_I2C_ADDRESS 0x68

  byte decToBcd(byte val){ // Convert normal decimal numbers to binary coded decimal
  return( (val/10*16) + (val%10) );
  }

  byte bcdToDec(byte val){ // Convert binary coded decimal to normal decimal numbers
  return( (val/16*10) + (val%16) );
  }

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };   //physical mac address
byte ip[] = { 192, 168, 0, 178 };                      //ip in lan (that's what you need to use in your browser. ("192.168.0.178")
byte gateway[] = { 192, 168, 0, 1 };                   //internet access via router
byte subnet[] = { 255, 255, 255, 0 };                  //subnet mask
EthernetServer server(4477);                           //server port     
String readString;

#define DHTPIN 8
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

long previousMillisdeshum = 0;
long previousMillislog = 0;
long previousMillisroof = 0;
long previousMillisheat = 0;
int deshumstate = 0;

/*#####################AJUSTEMENTS#######################################*/

//////Intervals

long loginterval = 60000; // durrée en millisecondes d'un cycle de mesure pour enregistrement dans le journal
long heatinterval = 20000; // durrée en millisecondes d'un cycle de mesure pour vérification de température pour chauffage
long roofinterval = 120000; // durrée en millisecondes d'un cycle de mesure pour vérification de température pour ventilation

//////Consignes de base

int tempset = 25; // Temperature desiree en celsius
int maxhum = 73; // Humidité max pour cylce de deshum

//////Déshumidification

int ouvdeshum = 10000; //temps d'ouverture des toits (en millisecondes) lors du cycle de deshum
int closedeshum = 15000; //temps de fermeture des toits (en millisecondes) lors du cycle de deshum
long interval1 = 480000; //intervalle durant laquelle les toits sont fermés durant le cycle de déshumidification
long interval2 = 120000; //intervalle durant laquelle toits ouverts durant le cylcle de déshumidification

//////Ventilation

int tempopen = 1; // nombre de degrés au dessus du tempset pour ouverture des toits
int tempopenstage2 = 2; // nombre de degrés au dessus du tempset nécessaires pour que la vitesse d'ouverture passe du stage 1 au stage 2
int openstage1 = 5000; //nombre de milliseconde pendant lesquels les toits ouvrent lors d'un cycle de mesure
int openstage2 = 8000; //nombre de milliseconde pendant lesquels les toits ouvrent lors d'un cycle de mesure

int tempclose = 0; //nombre de degrés en dessous du tempset pour fermeture des toits
int tempclosestage2 = 1; // nombre de degrés en dessous du tempset nécessaires pour que la vitesse de fermeture passe du stage 1 au stage 2
int tempclosestage3 = 1; // nombre de degrés en dessous du tempset - tempheat nécessaires pour que la vitesse de fermeture passe du stage 2 au stage 3
int closestage1 = 5000; //nombre de milliseconde pendant lesquels les toits ferment lors d'un cycle de mesure
int closestage2 = 8000; //nombre de milliseconde pendant lesquels les toits ferment lors d'un cycle de mesure
int closestage3 = closedeshum; //nombre de milliseconde pendant lesquels les toits ferment lors d'un cycle de mesure

//////Chaufage

int startnight = 18;
int startdip = 8;
int startday = 10;

int endnight = startdip;
int enddip = startday;
int endday = startnight;

int tempheatnight = 6; //nombre de degrés en dessous du tempset nécessaires pour enclancher le chauffage
int tempheatday = 3; //nombre de degrés en dessous du tempset nécessaires pour enclancher le chauffage
int tempheatdip = 3; //nombre de degrés en dessous du tempset nécessaires pour enclancher le chauffage
float heaton = 0.6; // nombre de degrés celcius en dessous de la température de chauffage nécessaire pour partir le chauffage
float heatoff = 0.6; // nombre de degrés celcius au dessus de la temperature de chauffage nécessaire pour fermer le chauffage
int tempheat; // cette variable change en fonction de l'heure du jour et de des consignes tempheatday et tempheatnight
int heatstate = 0; // cette variable comporte deux état possible 1 = on, 0=off et indique si les aérotherme fonctionnenent au moment d'enregistrer sur la carte SD

File myFile;

void setup() {
  Wire.begin();
 // Open serial communications and wait for port to open:
  Serial.begin(9600);
   while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }

  pinMode(2,OUTPUT); //heat
  pinMode(3,OUTPUT); //roof opening
  pinMode(5,OUTPUT); //roof closing // pin 4 is used for SD Card
  pinMode(6,OUTPUT); //light
  pinMode(7,OUTPUT); //alarm

  dht.begin();
  
  // start the Ethernet connection and the server:
  Ethernet.begin(mac, ip, gateway, subnet);
  server.begin();
  Serial.print("server is at ");
  Serial.println(Ethernet.localIP());

  Serial.print("Initializing SD card...");
      if (!SD.begin(4)) {
      Serial.println("initialization failed!");
      }
  Serial.println("initialization done.");
}

void readDS3231time(byte *second,
byte *minute,
byte *hour,
byte *dayOfWeek,
byte *dayOfMonth,
byte *month,
byte *year)

  {
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

  ethernet();

  serialdisplay();
  
  unsigned long currentMillis = millis();
  
  if(currentMillis - previousMillislog >= loginterval) {
  previousMillislog = currentMillis;
  statuslog();
 }
  
  if(currentMillis - previousMillisroof >= roofinterval){
  previousMillisroof = currentMillis;
  roofcontrol();
  }
  
  if(currentMillis - previousMillisheat >= heatinterval){
  previousMillisheat = currentMillis;
  heatercontrol();
  }
  
  float hum = dht.readHumidity();
  float temp = dht.readTemperature();
     
     /* Fermeture des toits fin du cylcre de déshumiditfication */
     
     if(currentMillis - previousMillisdeshum >= interval2 && deshumstate == 1 && temp < tempset + tempopen) {
     previousMillisdeshum = currentMillis;
     digitalWrite (5,HIGH);
     delay(closedeshum);
     digitalWrite(5,LOW);
     deshumstate = 0;
     }
  
     /* Ouverture des toits pour début cylcle de déshumidification */
     
     if(currentMillis - previousMillisdeshum >= interval1 && hum >= maxhum && deshumstate == 0) {
     previousMillisdeshum = currentMillis;
     digitalWrite (3,HIGH);
     delay(ouvdeshum);
     digitalWrite(3,LOW);
     deshumstate = 1;
     }
  
}

void ethernet(){

  byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
  // retrieve data from DS3231
  readDS3231time(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month,
  &year);

  float hum = dht.readHumidity(); //capteur DHT22 boitier de commande
  float temp = dht.readTemperature(); // capteur DHT22 boitier de commande

// Create a client connection
  EthernetClient client = server.available();
  if (client) {
    while (client.connected()) {   
      if (client.available()) {
        char c = client.read();
     
        //read char by char HTTP request
        if (readString.length() < 100) {
          //store characters to string
          readString += c;
          //Serial.print(c);
         }

         //if HTTP request has ended
         if (c == '\n') {          
           Serial.println(readString); //print to serial monitor for debuging

           if (readString.indexOf("?mhs=60") >0){
                maxhum = 60;
                }
           if (readString.indexOf("?mhs=61") >0){
                maxhum = 61;
                }
           if (readString.indexOf("?mhs=62") >0){
                maxhum = 62;
                }
           if (readString.indexOf("?mhs=63") >0){
                maxhum = 63;
                }
           if (readString.indexOf("?mhs=64") >0){
                maxhum = 64;
                }
           if (readString.indexOf("?mhs=65") >0){
                maxhum = 65;
                }
           if (readString.indexOf("?mhs=66") >0){
                maxhum = 66;
                }
           if (readString.indexOf("?mhs=67") >0){
                maxhum = 67;
                }
           if (readString.indexOf("?mhs=68") >0){
                maxhum = 68;
                }
           if (readString.indexOf("?mhs=69") >0){
                maxhum = 69;
                }
           if (readString.indexOf("?mhs=70") >0){
                maxhum = 70;
                }
           if (readString.indexOf("?mhs=71") >0){
                maxhum = 71;
                }
           if (readString.indexOf("?mhs=72") >0){
                maxhum = 72;
                }
           if (readString.indexOf("?mhs=73") >0){
                maxhum = 73;
                }
           if (readString.indexOf("?mhs=74") >0){
                maxhum = 74;
                }
           if (readString.indexOf("?mhs=75") >0){
                maxhum = 75;
                }
           if (readString.indexOf("?mhs=76") >0){
                maxhum = 76;
                }
           if (readString.indexOf("?mhs=77") >0){
                maxhum = 77;
                }
           if (readString.indexOf("?mhs=78") >0){
                maxhum = 78;
                }
           if (readString.indexOf("?mhs=79") >0){
                maxhum = 79;
                }
           if (readString.indexOf("?mhs=80") >0){
                maxhum = 80;
                }
           if (readString.indexOf("?mhs=81") >0){
                maxhum = 81;
                }
           if (readString.indexOf("?mhs=82") >0){
                maxhum = 82;
                }
           if (readString.indexOf("?mhs=83") >0){
                maxhum = 83;
                }
           if (readString.indexOf("?mhs=84") >0){
                maxhum = 84;
                }
           if (readString.indexOf("?mhs=85") >0){
                maxhum = 85;
                }
           if (readString.indexOf("?mhs=86") >0){
                maxhum = 86;
                }
           if (readString.indexOf("?mhs=87") >0){
                maxhum = 87;
                }
           if (readString.indexOf("?mhs=88") >0){
                maxhum = 88;
                }
           if (readString.indexOf("?mhs=89") >0){
                maxhum = 89;
                }
           if (readString.indexOf("?mhs=90") >0){
                maxhum = 90;
                }

           /////////////////////////////////////////////////////////////////
           if (readString.indexOf("?ts=10") >0){
                tempset = 10;
                }
           if (readString.indexOf("?ts=11") >0){
                tempset = 11;
                }
           if (readString.indexOf("?ts=12") >0){
                tempset = 12;
                }
           if (readString.indexOf("?ts=13") >0){
                tempset = 13;
                }
           if (readString.indexOf("?ts=14") >0){
                tempset = 14;
                }
           if (readString.indexOf("?ts=15") >0){
                tempset = 15;
                }
           if (readString.indexOf("?ts=16") >0){
                tempset = 16;
                }
           if (readString.indexOf("?ts=17") >0){
                tempset = 17;
                }
           if (readString.indexOf("?ts=18") >0){
                tempset = 18;
                }
           if (readString.indexOf("?ts=19") >0){
                tempset = 19;
                }
           if (readString.indexOf("?ts=20") >0){
                tempset = 20;
                }
           if (readString.indexOf("?ts=21") >0){
                tempset = 21;
                }
           if (readString.indexOf("?ts=22") >0){
                tempset = 22;
                }
           if (readString.indexOf("?ts=23") >0){
                tempset = 23;
                }
           if (readString.indexOf("?ts-24") >0){
                tempset = 24;
                }
           if (readString.indexOf("?ts=25") >0){
                tempset = 25;
                }
           if (readString.indexOf("?ts=26") >0){
                tempset = 26;
                }
           if (readString.indexOf("?ts=27") >0){
                tempset = 27;
                }
           if (readString.indexOf("?ts=28") >0){
                tempset = 28;
                }
           if (readString.indexOf("?ts=29") >0){
                tempset = 29;
                }
           if (readString.indexOf("?ts=30") >0){
                tempset = 30;
                }
           if (readString.indexOf("?ts=31") >0){
                tempset = 31;
                }
           if (readString.indexOf("?ts=32") >0){
                tempset = 32;
                }
           if (readString.indexOf("?ts=33") >0){
                tempset = 33;
                }
           if (readString.indexOf("?ts=34") >0){
                tempset = 34;
                }
           if (readString.indexOf("?ts=35") >0){
                tempset = 35;
                }
         
           client.println("HTTP/1.1 200 OK"); //send new page
           client.println("Content-Type: text/html");
           client.println();     
           client.println("<HTML>");
           client.println("<HEAD>");
           client.println("<meta name='apple-mobile-web-app-capable' content='yes' />");
           client.println("<meta name='apple-mobile-web-app-status-bar-style' content='black-translucent' />");
           
           client.println("<style type=\"text/css\">");
           client.println("body{");
           client.println("margin:10px 0px; padding:0px;");
           client.println("text-align:center;");
           client.println("}");
           client.println("h1");
           client.println("{");
           client.println("text-align: center;");
           client.println("font-family:Arial, \"Trebuchet MS\", Helvetica, sans-serif;");
           client.println("}");
           client.println("h2");
           client.println("{");
           client.println("text-align: center;");
           client.println("font-family:Arial, \"Trebuchet MS\", Helvetica, sans-serif;");
           client.println("}");
           client.println("a");
           client.println("{");
           client.println("text-decoration:none;");
           client.println("width:75px;");
           client.println("height:50px;");
           client.println("border-color:black;");
           client.println("border-top:2px solid;");
           client.println("border-bottom:2px solid;");
           client.println("border-right:2px solid;");
           client.println("border-left:2px solid;");
           client.println("border-radius:10px 10px 10px;");
           client.println("-o-border-radius:10px 10px 10px;");
           client.println("-webkit-border-radius:10px 10px 10px;");
           client.println("font-family:\"Trebuchet MS\",Arial, Helvetica, sans-serif;");
           client.println("-moz-border-radius:10px 10px 10px;");
           client.println("background-color:#293F5E;");
           client.println("padding:8px;");
           client.println("text-align:center;");
           client.println("}");
           client.println("a:link {color:white;}      /* unvisited link */");
           client.println("a:visited {color:white;}  /* visited link */");
           client.println("a:hover {color:white;}  /* mouse over link */");
           client.println("a:active {color:white;}  /* selected link */");
           client.println("</style>");
           
           client.println("<TITLE>OKO</TITLE>");
           client.println("</HEAD>");
           
           client.println("<BODY>");

           client.println("<H1>OKO</H1>");
           client.print("<H3>");
           client.print(month, DEC);
           client.print("/");
           client.print(dayOfMonth, DEC);
           client.print("/20");
           client.print(year, DEC);
           client.print(" ");
           client.print(hour, DEC);  
           client.print(":");
           if (minute<10)
           {
           client.print("0");
           }
           client.print(minute, DEC);
           client.print("</H3>");
           client.println("<hr />");

           client.println("<H2>Current status</H2>");
           client.print("<H3>Temperature :");
           client.print(temp);
           client.print(" &#176;C  <br>   Humidity :");
           client.print(hum);
           client.println(" %\t </H3>");
           client.println("<hr />");

           client.println("<h2>Automatic Control settings</h2>");

           client.println("<form method=get>");
           client.print("Temperature set (10 - 35): ");
           client.print("<input type='number' name=ts value='ts'<br>");
           client.print("<input type=submit value=submit> &emsp;");
           client.print("<p>current setting is:</p>");
           client.print(tempset);
           client.print(" &#176;C");
           client.println("</form>");
           
           client.println("<form method=get>");
           client.print("Max humidity set (60 - 90): ");
           client.print("<input type='number' name=mhs value='mhs' <br>");
           client.print("<input type=submit value=submit> &emsp;");
           client.print("<p>current setting is:</p>");
           client.print(maxhum);
           client.print(" %\t");
           client.println("</form>");\
           client.println("<hr />");
           
           client.println("<H2>Manual Control</H2>");
           
           client.println("<br />");  
           client.println("<a href=\"/?button1on\"\">Turn On Heat</a>");
           client.println("<a href=\"/?button1off\"\">Turn Off Heat</a>");
           client.println("<a href=\"/?button2on\"\">Open roof</a>");
           client.println("<a href=\"/?button2off\"\">Stop opening</a>");
           client.println("<a href=\"/?button3on\"\">Close roof</a>");
           client.println("<a href=\"/?button3off\"\">Stop closing</a><br />");
           client.println("<br />");
           client.println("<br />");
           client.println("<a href=\"/?button4on\"\">Turn lights on</a>");
           client.println("<a href=\"/?button4off\"\">Turn lights off</a>");
           client.println("<a href=\"/?button5on\"\">Alarm test</a>");
           client.println("<a href=\"/?button5off\"\">Stop alarm test</a><br /><br>");
           client.println("<br />");
           client.println("</BODY>");
           client.println("</HTML>");

           delay(1);
           //stopping client
           client.stop();

           //controls the Arduino if you press the buttons
           if (readString.indexOf("?button1on") >0){
               digitalWrite(2, HIGH);
           }
           if (readString.indexOf("?button1off") >0){
               digitalWrite(2, LOW);
           }
           if (readString.indexOf("?button2on") >0){
               digitalWrite(3, HIGH);
           }
           if (readString.indexOf("?button2off") >0){
               digitalWrite(3, LOW);
           }
           if (readString.indexOf("?button3on") >0){
               digitalWrite(5, HIGH);
           }
           if (readString.indexOf("?button3off") >0){
               digitalWrite(5, LOW);
           }
           if (readString.indexOf("?button4on") >0){
               digitalWrite(6, HIGH);
           }
           if (readString.indexOf("?button4off") >0){
               digitalWrite(6, LOW);
           }
           if (readString.indexOf("?button5on") >0){
               digitalWrite(7, HIGH);
           }
           if (readString.indexOf("?button5off") >0){
               digitalWrite(7, LOW);
           }

            //clearing string for next read
            readString="";  
           
         }
       }
    }
}
}

/*######## Écriture des status sur le moniteur série ########*/

  void serialdisplay(){ 
   
  float hum = dht.readHumidity(); // capteur DHT22 serre 6
  float temp = dht.readTemperature(); // capteur DHT22 serre 6
  
  byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
  readDS3231time(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month, &year);
  
     if (hour >= startnight or hour < endnight){ // heures considérées comme la nuit
       tempheat = tempheatnight;
     }
     
     if (hour >=startdip && hour <enddip){ // heures considérées pour le dip
        tempheat = tempheatdip;
     }
     
     if(hour >=startday && hour <endday){  // heures considérées comme le jour
       tempheat = tempheatday;
     }
  
  Serial.print(month, DEC);
  Serial.print("/");
  Serial.print(dayOfMonth, DEC);
  Serial.print("/20");
  Serial.print(year, DEC);  
  Serial.print(" ");
  Serial.print(hour, DEC);
  Serial.print(":");
    if (minute<10){
    Serial.print("0");
    }
  Serial.print(minute, DEC);
  Serial.print(";");
  Serial.print(tempset - tempheat);
  Serial.print(";");
  Serial.print(tempset - tempclose);
  Serial.print(";");
  Serial.print(tempset + tempopen);
  Serial.print(";");
  Serial.print(hum);
  Serial.print(";");
  Serial.print(temp);
  Serial.print(";");
  Serial.print(deshumstate);
  Serial.print(";");
  Serial.print(millis());
  Serial.print(";");
  Serial.print(maxhum);
  Serial.print(";");
  Serial.println(heatstate);
  }

  /*######## Écriture des status sur carte SD ########*/

 void statuslog(){ 
   
  float hum = dht.readHumidity(); // capteur DHT22 serre 6
  float temp = dht.readTemperature(); // capteur DHT22 serre 6
  
  byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
  readDS3231time(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month, &year);
  
     if (hour >= startnight or hour < endnight){ // heures considérées comme la nuit
       tempheat = tempheatnight;
     }
     
     if (hour >=startdip && hour <enddip){ // heures considérées pour le dip
        tempheat = tempheatdip;
     }
     
     if(hour >=startday && hour <endday){  // heures considérées comme le jour
       tempheat = tempheatday;
     }
  
  myFile = SD.open("datalog.txt", FILE_WRITE);
  
  myFile.print(month, DEC);
  myFile.print("/");
  myFile.print(dayOfMonth, DEC);
  myFile.print("/20");
  myFile.print(year, DEC);  
  myFile.print(" ");
  myFile.print(hour, DEC);
  myFile.print(":");
    if (minute<10){
    myFile.print("0");
    }
  myFile.print(minute, DEC);
  myFile.print(tempset - tempheat);
  myFile.print(";");
  myFile.print(tempset - tempclose);
  myFile.print(";");
  myFile.print(tempset + tempopen);
  myFile.print(";");
  myFile.print(hum);
  myFile.print(";");
  myFile.print(temp);
  myFile.print(";");
  myFile.print(deshumstate);
  myFile.print(";");
  myFile.print(millis());
  myFile.print(";");
  myFile.print(maxhum);
  myFile.print(";");
  myFile.println(heatstate);
  myFile.close();
  
 }

void roofcontrol(){
  
  float hum = dht.readHumidity(); // capteur DHT22 serre 6
  float temp = dht.readTemperature(); // capteur DHT22 serre 6
     
     /*ouverture des toits si trop chaud */
     
     if (temp >= tempset + tempopen){ 
          if (temp >= tempset + tempopenstage2){ // ouverture un peu plus rapide des toits
          digitalWrite(3,HIGH);
          delay(openstage2);
          digitalWrite(3,LOW);
          }
          else{
          digitalWrite(3,HIGH);
          delay(openstage1);
          digitalWrite(3,LOW);
          }
      }
     
     /* fermeture des toits si la tempérauture est trop basse */  
         
     if (temp <= tempset - tempclose){ 
         if (temp <= tempset - tempheat - tempclosestage3 ){ //fermeture des toits meme si le cycle de deshum est activé
         digitalWrite(5,HIGH);
         delay(closestage3);
         digitalWrite(5,LOW);
         }  
         else if (temp <= tempset - tempclosestage2 && deshumstate == 0){ //fermeture un peut plus rapide des toits si le cycle de deshum n'est pas activé
         digitalWrite(5,HIGH);
         delay(closestage2);
         digitalWrite(5,LOW);
         }  
         else if (temp <= tempset - tempclose && deshumstate == 0){ //fermeture des toits si le cycle de deshum n'est pas activé
         digitalWrite(5,HIGH);
         delay(closestage1);
         digitalWrite(5,LOW);
         }
     }
 }

void heatercontrol(){
     
  float hum = dht.readHumidity();
  float temp = dht.readTemperature();
  
  byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
  readDS3231time(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month, &year);
  
     if (hour >= startnight or hour < endnight){ // heures considérées comme la nuit
       tempheat = tempheatnight;
     }
     
     if (hour >=startdip && hour <enddip){ // heures considérées pour le dip
        tempheat = tempheatdip;
     }
     
     if(hour >=startday && hour <endday){  // heures considérées comme le jour
       tempheat = tempheatday;
     }
       
     if (temp <= tempset - tempheat - heaton){ // temperature limite pour consigne de chauffage
     digitalWrite(2,HIGH);
     heatstate = 1;
     }
     
     if (temp > tempset - tempheat + heatoff){  // fermeture du chauffage si temp plus haute que tempset - différence de chauffage
     digitalWrite(2,LOW);
     heatstate = 0;
     }
     
}


