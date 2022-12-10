/*******************************************************************
    
     Neue Features:
     - Telegram Bot:
        - Persönliche Ansprache mit Namen bei jedem Kontakt
        - /status zeigt an wann Bot zuletzt aktiv war und ob der Bot gerade aktiv ist
        - /status zeigt an wann sich Heizibot selbst resetet hat
        - /wificheck zeigt den Verbindungspegel zum Wifi an
        - /tempcheck zeigt Temperatur des externen Sensors an der Heizung an
        - Neue Texte beim Aktivieren des Bots
    - Alexa Unterstützung:
      - Heizibot wird von Alexa gefunden.
      - Heizibot kann von Alexa gesteuert werden.
      - Heizibot kann in die Morgenroutine mit aufgenommen werden.
    - Taster Unterstützung per MQTT Protokoll
    - Stabilitätverbesserungen:
      - Automatische Wifi Wiederverbindung bei kurzen Ausfällen der FritzBOX nach 60 Sekunden
    - behobene Bugs:
      - 10 min Timer lief manchmal nur 5 min
      
 *******************************************************************/
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include "uMQTTBroker.h"
#include "credentials.h"  //Pin Definitions
#include "fauxmoESP.h"    //Alexa Support


//Tempsensor
#include <OneWire.h>
#include <DallasTemperature.h>

//Ext Temp Sensor
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

//Alexa Support
fauxmoESP fauxmo;


const unsigned long BOT_MTBS = 1000;    // mean time between scan messages
const unsigned long WIFI_MTBS = 30000;  // mean time between scan wifi connection
unsigned long lasttime_waterpump = 0;
unsigned long wifi_check_lasttime = 0;
const int ledPin = LED_GREEN;
const int ledPin2 = LED_RELAIS;
const int analogPin = A0;  // Pin, der gelesen werden soll: Pin A3
int analogval = 0;         // Variable,
int legionellencheck_active = 0;
bool blink = 0;
unsigned long min_waterpump_on_duration = 300 * 1000;  //5min
String text = "";
long lastMsg = 0;  //MQTT

int usagecounter[] = { 0, 0, 0, 0, 0, 0, 0 };  //User1, U2, U3, U4, U5, T1, T2

X509List cert(TELEGRAM_CERTIFICATE_ROOT);
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
unsigned long bot_lasttime;  // last time messages' scan has been done
int ledStatus = 0;



/////// MQTT
/*
 * Custom broker class with overwritten callback functions
 */
class myMQTTBroker : public uMQTTBroker {
public:
  virtual bool onConnect(IPAddress addr, uint16_t client_count) {
    //Serial.println(addr.toString()+" connected");
    Serial.print(addr);
    Serial.println(" verbunden");
    return true;
  }

  virtual void onDisconnect(IPAddress addr, String client_id) {
    //Serial.println(addr.toString()+" ("+client_id+") disconnected");
    Serial.print(client_id);
    Serial.println(" disconnected");
  }

  virtual bool onAuth(String username, String password, String client_id) {
    //Serial.println("Username/Password/ClientId: "+username+"/"+password+"/"+client_id);
    Serial.print(client_id);
    Serial.println(" verbunden");
    return true;
  }

  virtual void onData(String topic, const char *data, uint32_t length) {
    char data_str[length + 1];
    os_memcpy(data_str, data, length);
    data_str[length] = '\0';

    //Serial.println("received topic '"+topic+"' with data '"+(String)data_str+"'");
    String test = "";
    test = (String)data_str;
    if (topic == "esp32/heizi_request") {
      //if (String(topic) == "esp32/heizi_request") {
      //Serial.print("MQTT Daten empfangen. Heizibot ");
      if (test == "1") {
        Serial.println("on");
        digitalWrite(ledPin2, HIGH);
        ledStatus = 1;
        lasttime_waterpump = millis();
        min_waterpump_on_duration = 300 * 1000;  //5min
        usagecounter[5]++;
      } else if (test == "0") {
        Serial.println("off");
        digitalWrite(ledPin2, LOW);  // turn the LED off (LOW is the voltage level)
        ledStatus = 0;
        usagecounter[6]++;
      } else {
        Serial.print("Data_str:");
        Serial.println(test);
      }
      test = "";
    }
  }

  // Sample for the usage of the client info methods

  virtual void printClients() {
    for (int i = 0; i < getClientCount(); i++) {
      IPAddress addr;
      String client_id;

      getClientAddr(i, addr);
      getClientId(i, client_id);
      Serial.println("Client " + client_id + " on addr: " + addr.toString());
    }
  }
};

////###MQTT
myMQTTBroker myBroker;



void handleNewMessages(int numNewMessages) {
  Serial.print("handleNewMessages von: ");
  //Serial.println(String(numNewMessages));

  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = bot.messages[i].chat_id;
    String text = bot.messages[i].text;

    //User Authentifizierung
    if (chat_id == CHAT_ID1) {
      Serial.println("ID1");
      usagecounter[0]++;
    } else if (chat_id == CHAT_ID2) {
      Serial.println("ID2");
      usagecounter[1]++;
    } else if (chat_id == CHAT_ID3) {
      Serial.println("ID3");
      usagecounter[2]++;
    } else if (chat_id == CHAT_ID4) {
      Serial.println("ID4");
      usagecounter[3]++;
    } else if (chat_id == CHAT_ID5) {
      Serial.println("ID5");
      usagecounter[4]++;
    } else {
      bot.sendMessage(chat_id, "Unauthorized user, Please contact the Admin", "");
      bot.sendMessage(chat_id, chat_id, "");
      Serial.println(chat_id);
      continue;
    }


    String from_name = bot.messages[i].from_name;
    if (from_name == "")
      from_name = "Guest";

    if (text == "/timer5") {
      digitalWrite(ledPin2, HIGH);  // turn the LED on (HIGH is the voltage level)
      ledStatus = 1;
      text = from_name;
      text += " ich schalte die Heisswasser-Wasserpumpe für 5 Minuten für Dich AN";
      bot.sendMessage(chat_id, text, "Markdown");
      text = "";
      lasttime_waterpump = millis();
      min_waterpump_on_duration = 300 * 1000;  //5min
    }
    if (text == "/timer10") {
      digitalWrite(ledPin2, HIGH);  // turn the LED on (HIGH is the voltage level)
      ledStatus = 2;
      text = from_name;
      text += " ich schalte Heizibot für 10 Minuten für Dich AN";
      bot.sendMessage(chat_id, text, "Markdown");
      text = "";
      lasttime_waterpump = millis();
      min_waterpump_on_duration = 600 * 1000;  //10min
    }

    if (text == "/aus") {
      ledStatus = 0;
      digitalWrite(ledPin2, LOW);  // turn the LED off (LOW is the voltage level)
      digitalWrite(ledPin2, LOW);  // turn the LED off (LOW is the voltage level)
      text = from_name;
      text += " ich schalte Heizibot für Dich AUS";
      bot.sendMessage(chat_id, text, "Markdown");
      text = "";
      myBroker.publish("esp32/heizi_request", (String)ledStatus);
    }

    if (text == "/status") {
      long zeit_vergangen_seit_letzter_Aktivierung = (millis() - lasttime_waterpump) / 60000;
      text = (String)zeit_vergangen_seit_letzter_Aktivierung;
      text += " Minuten seit letzter Aktivierung. Heizibot ist ";
      if (ledStatus) {
        text += "AN";
      } else {
        text += "AUS";
      }
      text += ". Laufzeit seit letztem Reset: ";
      text += String(millis() / 60000);
      text += " Minuten ";
      bot.sendMessage(chat_id, text, "");
      text = "";
    }
    if (text == "/statistik") {
      text = "Anzahl Schaltvorgänge: ";
      text += usagecounter[0];
      text += " x Uli, ";
      text += usagecounter[1];
      text += " x Sandra, ";
      text += usagecounter[2];
      text += " x Frederik, ";
      text += usagecounter[3];
      text += " x Oli, ";
      text += usagecounter[4];
      text += " x Tamara, ";
      text += usagecounter[5];
      text += " x Taster1, ";
      text += usagecounter[6];
      text += " x Taster2.";
      text += " Laufzeit seit letztem Reset: ";
      text += String(millis() / 60000);
      text += " Minuten ";
      bot.sendMessage(chat_id, text, "");
      text = "";
    }

    if (text == "/wificheck") {
      long zeit_vergangen_seit_letzter_Aktivierung = (millis() - lasttime_waterpump) / 60000;
      text = "Wifi RSSI Pegel: ";
      text += (String)WiFi.RSSI();
      text += " dBm";
      bot.sendMessage(chat_id, text, "");
      text = "";
    }

    if (text == "/tempcheck") {
      sensors.requestTemperatures();  // Send the command to get temperatures
      text = "Temperatur am Sensor: ";
      text += (String)sensors.getTempCByIndex(0);
      text += " C";
      bot.sendMessage(chat_id, text, "");
      text = "";
    }

    if (text == "/options") {
      String keyboardJson = "[[\"/timer5\", \"/timer10\"],[\"/status\", \"/aus\"]]";
      bot.sendMessageWithReplyKeyboard(chat_id, "Wähle aus den folgenden Optionen", "", keyboardJson, true);
    }

    if (text == "/start") {
      String welcome = "Willkommen bei Heizibot, " + from_name + ".\n";
      welcome += "Du kannst folgende Befehle senden.\n\n";
      welcome += "/timer5 : Warmwasserpumpe ist für 5 Min AN ON\n";
      welcome += "/timer10 : Warmwasserpumpe ist für 10 Min AN ON\n";
      welcome += "/aus : Schaltet die Warmwasserpumpe AUS\n";
      welcome += "/status : Zeigt den aktuellen Status der Wasserpumpe an\n";
      welcome += "/wificheck : Zeigt den WiFi Verbindungs-Pegel an\n";
      welcome += "/tempcheck : Zeigt die Temperatur am ext Sensor an\n";
      welcome += "/options : aktiviert die Antwort Tastatur\n";
      bot.sendMessage(chat_id, welcome, "Markdown");
    }
  }
}






void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);   // initialize digital ledPin as an output.
  pinMode(ledPin2, OUTPUT);  // initialize digital ledPin as an output.
  delay(10);
  digitalWrite(ledPin, LOW);   // initialize pin as off
  digitalWrite(ledPin2, LOW);  // initialize pin as off
  wifi_reconnect();            // Set WiFi to station mode and disconnect from an AP if it was Previously connected



  //Wifi Reconnect machen
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);


  // Start the MQTT broker
  Serial.println("Starting MQTT broker");
  myBroker.init();


  //myBroker.subscribe("#"); //Subscribe to anything
  myBroker.subscribe("esp32/heizi_request");
  myBroker.subscribe("heizibot/status");

  digitalWrite(ledPin, HIGH);  // initialize pin as off
  digitalWrite(ledPin2, LOW);  // initialize pin as off

  lasttime_waterpump = millis();
  Serial.println("MQTT broker started");
  myBroker.publish("esp32/heizi_request", (String)ledStatus);
  myBroker.publish("heizibot/status", (String)ledStatus);

  sensors.begin();  // Start up the library

  //Alexa support By default, fauxmoESP creates it's own webserver on the defined port
  // The TCP port must be 80 for gen3 devices (default is 1901)
  // This has to be done before the call to enable()
  fauxmo.createServer(true);  // not needed, this is the default value
  fauxmo.setPort(80);         // This is required for gen3 devices
  fauxmo.enable(true);        //make it visible for alexa

  fauxmo.addDevice(ID_YELLOW);  // Add virtual devices 5min Timer
  fauxmo.addDevice(ID_GREEN);  // Add virtual devices AUS Timer
  

  fauxmo.onSetState([](unsigned char device_id, const char * device_name, bool state, unsigned char value) {
        
        // Callback when a command from Alexa is received. 
        // You can use device_id or device_name to choose the element to perform an action onto (relay, LED,...)
        // State is a boolean (ON/OFF) and value a number from 0 to 255 (if you say "set kitchen light to 50%" you will receive a 128 here).
        // Just remember not to delay too much here, this is a callback, exit as soon as possible.
        // If you have to do something more involved here set a flag and process it in your main loop.
        
        Serial.printf("[MAIN] Device #%d (%s) state: %s value: %d\n", device_id, device_name, state ? "ON" : "OFF", value);

        // Checking for device_id is simpler if you are certain about the order they are loaded and it does not change.
        // Otherwise comparing the device_name is safer.

        if (strcmp(device_name, ID_YELLOW)==0) {
            //digitalWrite(ledPin, state ? HIGH : LOW);
            ledStatus = 1;
            myBroker.publish("esp32/heizi_request", (String)ledStatus);
        } else if (strcmp(device_name, ID_GREEN)==0) {
            //digitalWrite(ledPin2, state ? HIGH : LOW);
            ledStatus = 0;
            myBroker.publish("esp32/heizi_request", (String)ledStatus);
        }

    });

}

void loop() {
  fauxmo.handle();
  if (millis() - bot_lasttime > BOT_MTBS) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages) {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
      /*analogval = analogRead(analogPin); // Pin einlesen
      Serial.println(analogval); // Wert ausgeben*/
    }

    bot_lasttime = millis();
    myBroker.publish("heizibot/status", (String)ledStatus);
    check_legionellen();
    ausschalten_5min();
    //ausschalten_10min();
     //Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
  }
  if (millis() - wifi_check_lasttime > WIFI_MTBS) {
    wifi_check_lasttime = millis();
    wifi_checker();
    fauxmo.setState(ID_YELLOW, bool(ledStatus), 255);
  }
}

void check_legionellen(void) {
  unsigned long now = millis();
  unsigned long tag24h = 24 * 3600 * 1000;               //24h
  unsigned long min_waterpump_on_duration = 180 * 1000;  //5min
  if ((now > (lasttime_waterpump + tag24h)) && legionellencheck_active == 0) {
    digitalWrite(ledPin, HIGH);
    lasttime_waterpump = millis();
    legionellencheck_active = 1;
    Serial.println("LegionellenCheck Aktiv");
    ledStatus = 1;
    myBroker.publish("esp32/heizi_request", (String)ledStatus);
  } else if (legionellencheck_active == 1) {
    if (lasttime_waterpump + min_waterpump_on_duration > now) {
      digitalWrite(ledPin2, LOW);
      legionellencheck_active = 0;
      Serial.println("LegionellenCheck Beendet");
      ledStatus = 0;
      myBroker.publish("esp32/heizi_request", (String)ledStatus);
    }
  } else {
    // do nothing
  }
}

void ausschalten_5min(void) {
  unsigned long now = millis();

  /*Serial.print("Ausschalten 5: ");
  Serial.print("Now: ");
  Serial.print(now);
  Serial.print(" LastTime:");
  Serial.print(lasttime_waterpump);
  Serial.print(" Min: ");
  Serial.println(min_waterpump_on_duration);*/
  if ((now > (lasttime_waterpump + min_waterpump_on_duration)) && (ledStatus == 1)) {
    digitalWrite(ledPin, LOW);
    digitalWrite(ledPin2, LOW);
    ledStatus = 0;
    Serial.print(min_waterpump_on_duration / 60000);
    Serial.println(" Minuten Timer beendet");
    myBroker.publish("esp32/heizi_request", (String)ledStatus);
  }
}



//Püft ob Wifi noch verbunden
//Bei Problemen stellt der Bot nach 60 sec eine neue Verbindung her
void wifi_checker(void) {
  switch (WiFi.status()) {
    case WL_NO_SSID_AVAIL:
      Serial.println("Configured SSID cannot be reached. Try to reconnect in 60sec");
      delay(60000);
      wifi_reconnect();
      break;
    case WL_CONNECTED:
      //Serial.println("Connection successfully established");
      break;
    case WL_CONNECT_FAILED:
      Serial.println("Connection failed. Try to reconnect in 60sec");
      delay(60000);
      wifi_reconnect();
      break;
  }
}

// attempt to connect to Wifi network:
void wifi_reconnect(void) {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  WiFi.hostname("WARMWASSER");
  Serial.print("\n Connecting to Wifi SSID ");
  Serial.print(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  secured_client.setTrustAnchors(&cert);  // Add root certificate for api.telegram.org
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    digitalWrite(ledPin, blink);
    blink = !blink;
    delay(500);
  }
  Serial.print("\n WiFi connected. IP address: ");
  Serial.println(WiFi.localIP());
  Serial.printf("Connection status: %d\n", WiFi.status());
  Serial.print("RRSI: ");
  Serial.println(WiFi.RSSI());
}