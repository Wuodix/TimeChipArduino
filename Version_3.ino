// Code für die VWA Version 2.0
// Aktueller Stand: LCD Display funktioniert, NFC Funktioniert, bei Fingerabdruck geht kontrollieren
// Zu Machen: Code damit Arduino mit Datenbank interagieren kann (per http auf Web Server zugreifen und Daten eintragen), Ist schon fertig in http_send_test, Code damit die App mit dem Arduino interagieren kann (UDP) zb Anfrage jetzt kommt Finger jetzt den Finger speichern etc.

#include <LiquidCrystal_I2C.h> //Bibliothek für Lcd Display über I2C Schnittstelle
#include <Adafruit_Fingerprint.h> //Bibliothek für Fingerabdrucksensor
#include <MFRC522.h> //Bibliothek für RFID Reader

#include <SPI.h>         // Bibliothek um mit dem Ethernetshield kommunizieren zu können
#include <Ethernet.h>   // Bibliothek um über das Ethernet kommunizieren zu können
#include <EthernetUdp.h> //Bibliothek um über das Ethernet per UDP kommunizieren zu können

#include <Timezone.h>  //Bibliothek, die das Umschalten zwischen MEZ und MESZ übernimmt
#include <NTPClient.h> //Bibliothek um die interne Zeit mit einem NTP Server zu synchronisieren
#include <time.h>

#define TouchSensor     17 //Pin an dem der Wakeup ausgang vom Fingerabdrucksensor angesteckt wird. Ist HIGH wenn ein Finger am Sensor liegt
#define active_low      1
#define TimeDisplayClear 5000
#define localPort 8888


//Erstellt die Objekte für das LCD Display, den Fingerprintsensor (mySerial für serielle Kommunikation) und den RFID Reader
LiquidCrystal_I2C lcd(0x27,20,4);

Adafruit_Fingerprint fingerprintsen = Adafruit_Fingerprint(&Serial1);
MFRC522 rfidReader(23, 25); //(NSS/RST)

EthernetServer webServer(80);

//Helper Variablen für die "Timer" Funktion damit das Display nach x Sekunden gelöscht wird
unsigned long lcdWriteTime = 2000;

//Helper Variable um die Uhrzeit abzufragen
bool bereitsAbgefragt = false;

EthernetUDP UdpClient;
NTPClient timeClient(UdpClient, "europe.pool.ntp.org");

// Configures CET/CEST
TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120};     // Central European Summer Time
TimeChangeRule CET = {"CET ", Last, Sun, Oct, 3, 60};       // Central European Standard Time
Timezone CE(CEST, CET);


void setup() {
  Serial.begin(9600);
  Serial1.begin(57600);

  SPI.begin();
  rfidReader.PCD_Init();
  
  lcd.begin();
  lcd.backlight();
  lcd.clear();

  fingerprintsen.begin(57600);
  pinMode(TouchSensor, INPUT);

  Serial.println("Ethernet Start");
  // variable to hold Ethernet shield MAC address
  byte mac[] = { 0xA8, 0x61, 0x0A, 0xAE, 0x86, 0x76 };
  byte ip[] = {10,100,128,21};
  byte dns[] = {10,100,128,201};
  byte gateway[] = {10,100,128,111};
  byte subnet[] = {255,255,255,0};

  Ethernet.begin(mac, ip, dns, gateway, subnet);

  delay(2000);
  Serial.println(Ethernet.localIP());

  UdpClient.begin(localPort);
  webServer.begin();
  timeClient.begin();
  delay(1000);

  GetSetTime();
  
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Ready");
  
  char buf1[21] = "";
  giveTime(CE.toLocal(now()), buf1);
  lcd.setCursor(0,2);
  lcd.print(buf1);

  Serial.println(CE.toLocal(now()));
}

void loop(){
  EthernetClient webClient = webServer.available();
  
  if(!rfidReader.PICC_IsNewCardPresent() && !(active_low ^ digitalRead(TouchSensor)) && !webClient){
    if(lcdWriteTime + 120000 < millis()){
      lcd.noBacklight();
      GetSetTime();
      lcdWriteTime = 4294767000;
    }
    if(lcdWriteTime + TimeDisplayClear < millis()){
      lcd.clear();
    }

    return;
  }
  lcd.clear();
  lcd.backlight();
  bereitsAbgefragt = false;
  if((active_low ^ digitalRead(TouchSensor))){
    char ResultFing[5];
    ReadFingerprint(ResultFing);
    if(ResultFing[0] != '-'){
      Ausgabe(ResultFing, true);
    }
    while(active_low ^ digitalRead(TouchSensor)){
      
    }
  }
  else if(webClient){
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    bool first = true;
    char nachricht[3][50];

    for(int i = 0; i<3; i++){
      memset(nachricht[i], 0, 50);
    }
    
    int i = 0;
    int k = 0;
    while (webClient.connected()) {
      if (webClient.available()) {
        char c = webClient.read();
        Serial.write(c);
        if(first == false && c != '\n' && c != '\r'){
          if(!strcmp(nachricht[0],"Finger") == 0){
            nachricht[0][i] = c;
            i++;
          }
          else{
            nachricht[1][k] = c;
            k++;
          }
        }
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {
          if(first == false){
            // send a standard http response header
            webClient.println("HTTP/1.1 200 OK");
            webClient.println("Content-Type: text/html");
            webClient.println("Connection: close");  // the connection will be closed after completion of the response
            webClient.println("Refresh: 5");  // refresh the page automatically every 5 sec
            webClient.println();
            break;
          }
          else{
            first = false;
          }
        }
        if (c == '\n') {
          // you're starting a new line
          currentLineIsBlank = true;
        } else if (c != '\r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }
    }
    if(strcmp(nachricht[0], "Finger") == 0){
      Serial.println("AAAAHHHA EIN FINGER");
      Serial.println(nachricht[1]);

      int buf1;
      sscanf(nachricht[1], "%d", &buf1);

      if(AddFinger(buf1) == 1){
        webClient.println("Finger-hinzugefuegt"); 
      }
      else{
        webClient.println("Finger-nicht-hinzugefuegt");
      }
    }
    else if(strcmp(nachricht[0], "Card") == 0){
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Warte auf Karte");

      long waitingTime = millis();
      while(!rfidReader.PICC_IsNewCardPresent() && waitingTime+20000 > millis()){
        
      }

      if(waitingTime+30000<millis()){
        lcdprint("Vorgang abgebrochen");
        lcd.setCursor(0,1);
        lcd.print("Zu langsam");
        Serial.println("Karte wurde nicht eingelesen (zu langsam)");
        return;
      }
      
      rfidReader.PICC_ReadCardSerial();
      char UID[32];
      ReadNFCTag(UID);

      lcdprint("Karte eingelesen");

      char Result[40] = "CardUID:$";
      strcat(Result, UID);
      strcat(Result, "$");

      webClient.println(Result);
    }
    webClient.stop();

    delay(3000);
  }
  else{
    rfidReader.PICC_ReadCardSerial();
    char ResultRfid[32];
    ReadNFCTag(ResultRfid);
    Ausgabe(ResultRfid, false);
  }
}

void GetSetTime(){
  lcdprint("Zeit aktualisieren");
  
  while(!timeClient.update()){delay(1000); Serial.println("NTP Update didn't work!");}
  
  Serial.println ( "Adjust local clock" );
  unsigned long epoch = timeClient.getEpochTime();
  setTime(epoch - 946684800);
}

void ReadFingerprint(char Result[]){
  uint8_t p = fingerprintsen.getImage();
  if (p != FINGERPRINT_OK){
    Serial.println("Einlesen nicht ok");
    Result[0] = '-';
    Result[1] = '1';
    return;
  }
  else{
    fingerprintsen.LEDcontrol(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_BLUE); // Fingerabdruck eingelesen
    p = fingerprintsen.image2Tz();
    fingerprintsen.LEDcontrol(FINGERPRINT_LED_OFF, 0, FINGERPRINT_LED_BLUE);
    Serial.println("Fingerprint eingelesen");
  }
  if (p != FINGERPRINT_OK){
    Serial.println("bild nicht ok");
    Result[0] = '-';
    Result[1] = '1';
    return;
  }
  else{
    p = fingerprintsen.fingerSearch();
  }
  if (p != FINGERPRINT_OK) 
  {
    fingerprintsen.LEDcontrol(FINGERPRINT_LED_OFF, 0, FINGERPRINT_LED_BLUE); // Fingerabdruck nicht in Datenbank
    fingerprintsen.LEDcontrol(FINGERPRINT_LED_GRADUAL_OFF, 500, FINGERPRINT_LED_RED);
    Serial.println("Fingerprint nicht in Datenbank");
    lcdprint("Fingerabdruck wurde", "nicht gefunden","","");
    Result[0] = '-';
    Result[1] = '1';
    return;
  }
  else{
    // Fingerabdruck in Datenbank gefunden
    fingerprintsen.LEDcontrol(FINGERPRINT_LED_GRADUAL_OFF, 100, FINGERPRINT_LED_PURPLE);
    Serial.println("Fingerprint in Datenbank gefunden");

    itoa(fingerprintsen.fingerID, Result, 10);
    return;
  }
}

void ReadNFCTag(char Result[]){
  Serial.print(F("Card UID:"));    //Dump UID
  dump_byte_array(rfidReader.uid.uidByte, rfidReader.uid.size);

  static char UIDString[32] = "";
  array_to_string(rfidReader.uid.uidByte, rfidReader.uid.size, UIDString);

  // Halt PICC
  rfidReader.PICC_HaltA();

  // Stop encryption on PCD
  rfidReader.PCD_StopCrypto1();

  for(byte i = 0; i<32; i++){
    Result[i] = UIDString[i]; 
  }
}

void Ausgabe(char ID[], bool which){ //which gibt an ob Fingerabdruck oder RFID -> true = Fingerabdruck; false = RFID
    if(!which){
      convertRFIDtoFinger(ID);

      if(strcmp(ID, "-1") == 0){
        return;
      }
    }
    else{
      convertFingertoID(ID);

      if(strcmp(ID, "-1") == 0){
        return;
      }
    }

    char data[4][20];
    getData(ID, data);
    char zeile1[20] = "";
    char zeile2[20] = "";
    char zeile3[20] = "";
    char zeile4[20] = "";
    char btypbuf[20];

    strcpy(zeile1, data[0]);
    strcpy(zeile2, data[1]);
    giveTime(CE.toLocal(now()), zeile3);
    if(strcmp(data[2], "Kommen") == 0){
      strcpy(zeile4, "Arbeitsende");
      strcpy(btypbuf, "Gehen");    
    }
    else{
      strcpy(zeile4, "Arbeitsbeginn");
      strcpy(btypbuf, "Kommen");
    }
    
    lcdprint(zeile1, zeile2, zeile3, zeile4);
    
    char request[100];
    char mtbtrnr[50] = "/insert_buchung.php?Mitarbeiternummer=";
    strcat(mtbtrnr, ID);
    char btyp[30] = "&Buchungstyp=";
    strcat(btyp, btypbuf);
    char zeit[30] = "&Zeit=";
    giveTimeSrv(CE.toLocal(now()), zeit);
  
    strcpy(request,mtbtrnr);
    strcat(request,btyp);
    strcat(request,zeit);

    Serial.println(request);
    Send_Http(request);
}

void lcdprint(char zeile1[], char zeile2[], char zeile3[], char zeile4[]){
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print(zeile1);
    lcd.setCursor(0,1);
    lcd.print(zeile2);
    lcd.setCursor(0,2);
    lcd.print(zeile3);
    lcd.setCursor(0,3);
    lcd.print(zeile4);

    lcdWriteTime = millis();
}

void dump_byte_array(byte *buffer, byte bufferSize) {
    for (byte i = 0; i < bufferSize; i++) {
        Serial.print(buffer[i] < 0x10 ? " 0" : " ");
        Serial.print(buffer[i], HEX);
    }
    Serial.println();
}

void giveTime(long time, char Result[]){
  const time_t tim = time;
  tm *newtime = localtime(&tim);

  char* pattern = (char *)"%d.%m.%Y %H:%M:%S";
  strftime(Result, 20, pattern, newtime);
}

void giveTimeSrv(long time, char Result[]){
  const time_t tim = time;
  tm *newtime = localtime(&tim);

  char* pattern = (char *)"%d.%m.%Y-%H:%M:%S";
  char bufres[20];
  strftime(bufres, 20, pattern, newtime);
  strcat(Result,bufres);
}

void array_to_string(byte array[], unsigned int len, char buffer[])
{
    for (unsigned int i = 0; i < len; i++)
    {
        byte nib1 = (array[i] >> 4) & 0x0F;
        byte nib2 = (array[i] >> 0) & 0x0F;
        buffer[i*2+0] = nib1  < 0xA ? '0' + nib1  : 'A' + nib1  - 0xA;
        buffer[i*2+1] = nib2  < 0xA ? '0' + nib2  : 'A' + nib2  - 0xA;
    }
    buffer[len*2] = '\0';
}

void Send_Http(char Request[]){
  EthernetClient client;
  // connect to web server on port 80:
  byte server[] = {10,100,128,1};
  if(client.connect(server, 80)) {
    // if connected:
    Serial.println("Connected to server");
    // make a HTTP request:
    // send HTTP header
    client.println("GET " + String(Request) + " HTTP/1.1");
    client.println("Host: 10.100.128.168");
    client.println("Connection: close");
    client.println(); // end HTTP header

    while(client.connected()) {
      if(client.available()){
        // read an incoming byte from the server and print it to serial monitor:
        char c = client.read();
        Serial.print(c);
      }
    }

    // the server's disconnected, stop the client:
    client.stop();
    Serial.println();
    Serial.println("disconnected");
  } else {// if not connected:
    Serial.println("connection failed");
  }
}

void convertRFIDtoFinger(char ID[]){
  EthernetClient client;
  char bufres[20];
  // connect to web server on port 80:
  byte server[] = {10,100,128,1};
  if(client.connect(server, 80)) {
    // if connected:
    Serial.println("Connected to server");
    // make a HTTP request:
    // send HTTP header
    char request[40] = "/convert_rfid_to_finger.php?UID=";
    strcat(request,ID);
    Serial.println(request);
    client.println("GET " + String(request) + " HTTP/1.1");
    client.println("Host: 10.100.128.168");
    client.println("Connection: close");
    client.println(); // end HTTP header

    bool data = false;
    memset(bufres,0,20);
    
    int i = 0;
    while(client.connected()) {
      if(client.available()){
        char c = client.read();
        if(data){
            bufres[i] = c;
            i++;
        }

        if(c == '!'){
          data = true;
        }

        Serial.print(c);
      }
    }

    if(strcmp(bufres, "") == 0){
      lcdprint("Keine registrierte");
      lcd.setCursor(0,1);
      lcd.print("Karte");

      strcpy(ID, "-1");
    }
    else{
      Serial.println();
      Serial.println("Jetzt Data");
      Serial.println(bufres);

      strcpy(ID,bufres);
    }
    
    // the server's disconnected, stop the client:
    client.stop();
    Serial.println();
    Serial.println("disconnected");
  } else {// if not connected:
    Serial.println("connection failed");
  }
}

void convertFingertoID(char Finger[]){
  EthernetClient client;
  char bufres[20];
  // connect to web server on port 80:
  byte server[] = {10,100,128,1};
  if(client.connect(server, 80)) {
    // if connected:
    Serial.println("Connected to server");
    // make a HTTP request:
    // send HTTP header
    char request[40] = "/convert_finger_to_ID.php?Finger=";
    strcat(request,Finger);
    Serial.println(request);
    client.println("GET " + String(request) + " HTTP/1.1");
    client.println("Host: 10.100.128.168");
    client.println("Connection: close");
    client.println(); // end HTTP header

    bool data = false;
    memset(bufres,0,20);
    
    int i = 0;
    while(client.connected()) {
      if(client.available()){
        char c = client.read();
        if(data){
            bufres[i] = c;
            i++;
        }

        if(c == '!'){
          data = true;
        }

        Serial.print(c);
      }
    }

    if(strcmp(bufres, "") == 0){
      lcdprint("Kein Mitarbeiter");
      lcd.setCursor(0,1);
      lcd.print("gefunden");

      strcpy(Finger, "-1");
    }
    else{
      Serial.println();
      Serial.println("Jetzt Data");
      Serial.println(bufres);

      strcpy(Finger,bufres);
    }
    
    // the server's disconnected, stop the client:
    client.stop();
    Serial.println();
    Serial.println("disconnected");
  } else {// if not connected:
    Serial.println("connection failed");
  }
}

void getData(char* Mitarbeiternummer, char result[][20]){
  EthernetClient client;
  // connect to web server on port 80:
  byte server[] = {10,100,128,1};
  if(client.connect(server, 80)) {
    // if connected:
    Serial.println("Connected to server");
    // make a HTTP request:
    // send HTTP header
    char request[40] = "/get_mtbtr.php?Mitarbeiternummer=";
    strcat(request,Mitarbeiternummer);
    Serial.println(request);
    client.println("GET " + String(request) + " HTTP/1.1");
    client.println("Host: 10.0.0.21");
    client.println("Connection: close");
    client.println(); // end HTTP header

    bool data = false;
    char bufres[20];
    memset(bufres,0,20);

    for(int i = 0; i<4; i++){
      memset(result[i], 0, 20);
    }
    
    int i = 0;
    int k = 0;
    while(client.connected()) {
      if(client.available()){
        char c = client.read();
        if(data){
          if(c == '$'){
            strcpy(result[i], bufres);
            i++;
            memset(bufres,0,20);
            k=0;
          }
          else{
            bufres[k] = c;
            k++;
          }
        }

        if(c == '!'){
          data = true;
        }

        Serial.print(c);
      }
    }

    if(strcmp(result[2], "NoMTBTR") == 0){
      lcdprint("Mitarbeiter nicht");
      lcd.setCursor(0,1);
      lcd.print("vorhanden");
    }

    if(strcmp(result[2], "") == 0){
      strcpy(result[2], "Gehen");
    }

    Serial.println();
    Serial.println("Jetzt Data");
    Serial.println(result[0]);
    Serial.println(result[1]);
    Serial.println(result[2]);

    // the server's disconnected, stop the client:
    client.stop();
    Serial.println();
    Serial.println("disconnected");
  } else {// if not connected:
    Serial.println("connection failed");
  }
}

void lcdprint(String text){
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(text);

  lcdWriteTime = millis();
}

int AddFinger(int id){
  uint8_t x = -1;

  Serial.print("FingerID: ");
  Serial.println(id);
  x = fingerprintsen.deleteModel(id);

  if (x == FINGERPRINT_OK) {
    Serial.println("Deleted!");
  } else if (x == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
  } else if (x == FINGERPRINT_BADLOCATION) {
    Serial.println("Could not delete in that location");
  } else if (x == FINGERPRINT_FLASHERR) {
    Serial.println("Error writing to flash");
  } else {
    Serial.print("Unknown error: 0x"); Serial.println(x, HEX);
  }

  int p = -1;

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Warte auf Finger");
  
  long waitingTime = millis();
  while (p != FINGERPRINT_OK && waitingTime+20000 > millis()) {
    p = fingerprintsen.getImage();
    switch (p) {
    case FINGERPRINT_OK:
      lcdprint("Finger gefunden");
      Serial.print("Finger gefunden");
      break;
    case FINGERPRINT_NOFINGER:
      //Serial.println(".");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      lcdprint("Communication error");
      break;
    case FINGERPRINT_IMAGEFAIL:
      lcdprint("Imaging error");
      break;
    default:
      lcdprint("Unknown error");
      break;
    }
  }

  if(waitingTime+30000<millis()){
    lcdprint("Vorgang Abgebrochen");
    lcd.setCursor(0,1);
    lcd.print("Zu langsam");
    Serial.println("Finger wurde nicht eingelesen (zu langsam)");
    return;
  }

  // OK success!

  p = fingerprintsen.image2Tz(1);
  switch (p) {
    case FINGERPRINT_OK:
      lcdprint("Bild konvertiert");
      break;
    case FINGERPRINT_IMAGEMESS:
      lcdprint("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      lcdprint("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      lcdprint("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      lcdprint("Could not find fingerprint features");
      return p;
    default:
      lcdprint("Unknown error");
      return p;
  }

  lcdprint("Finger wegnehmen");
  delay(2000);
  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = fingerprintsen.getImage();
  }
  Serial.print("ID "); Serial.println(id);
  p = -1;
  lcdprint("Gleichen Finger");
  lcd.setCursor(0,1);
  lcd.print("auf den Sensor");
  lcd.setCursor(0,2);
  lcd.print("legen");
  while (p != FINGERPRINT_OK) {
    p = fingerprintsen.getImage();
    switch (p) {
    case FINGERPRINT_OK:
      lcdprint("Finger gefunden");
      break;
    case FINGERPRINT_NOFINGER:
      //Serial.println('.');
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      lcdprint("Communication error");
      break;
    case FINGERPRINT_IMAGEFAIL:
      lcdprint("Imaging error");
      break;
    default:
      lcdprint("Unknown error");
      break;
    }
  }

  // OK success!

  p = fingerprintsen.image2Tz(2);
  switch (p) {
    case FINGERPRINT_OK:
      lcdprint("Bild konvertiert");
      break;
    case FINGERPRINT_IMAGEMESS:
      lcdprint("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      lcdprint("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      lcdprint("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      lcdprint("Could not find fingerprint features");
      return p;
    default:
      lcdprint("Unknown error");
      return p;
  }

  // OK converted!
  lcdprint("Model erstellen");

  p = fingerprintsen.createModel();
  if (p == FINGERPRINT_OK) {
    lcdprint("Prints matched!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    lcdprint("Communication error");
    return p;
  } else if (p == FINGERPRINT_ENROLLMISMATCH) {
    lcdprint("Fingerprints did not match");
    return p;
  } else {
    lcdprint("Unknown error");
    return p;
  }
  
  p = fingerprintsen.storeModel(id);
  if (p == FINGERPRINT_OK) {
    lcdprint("Finger gespeichert!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    lcdprint("Communication error");
    return p;
  } else if (p == FINGERPRINT_BADLOCATION) {
    lcdprint("Could not store in that location");
    return p;
  } else if (p == FINGERPRINT_FLASHERR) {
    lcdprint("Error writing to flash");
    return p;
  } else {
    lcdprint("Unknown error");
    return p;
  }
  return 1;
}
