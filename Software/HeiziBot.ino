/*******************************************************************
    An example of how to use a custom reply keyboard markup.


     written by Vadim Sinitski (modified by Brian Lough)
 *******************************************************************/
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include "credentials.h"  //Pin Definitions

const unsigned long BOT_MTBS = 1000;  // mean time between scan messages
unsigned long lasttime_waterpump = 0;
const int ledPin = LED_GREEN;
const int ledPin2 = LED_RELAIS;
const int analogPin = A0;  // Pin, der gelesen werden soll: Pin A3
int analogval = 0;         // Variable,
int legionellencheck_active = 0;
bool blink = 0;
X509List cert(TELEGRAM_CERTIFICATE_ROOT);
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
unsigned long bot_lasttime;  // last time messages' scan has been done
int ledStatus = 0;

void handleNewMessages(int numNewMessages) {
  Serial.println("handleNewMessages");
  Serial.println(String(numNewMessages));

  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = bot.messages[i].chat_id;
    String text = bot.messages[i].text;

    //User Authentifizierung
    if (chat_id == CHAT_ID1) {
      Serial.println("ID1");
    } else if (chat_id == CHAT_ID2) {
      Serial.println("ID2");
    } else if (chat_id == CHAT_ID3) {
      Serial.println("ID3");
    } else if (chat_id == CHAT_ID4) {
      Serial.println("ID4");
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
      bot.sendMessage(chat_id, "Warmwasser für 5 MIN AN", "");
      lasttime_waterpump = millis();
    }
    if (text == "/timer10") {
      digitalWrite(ledPin2, HIGH);  // turn the LED on (HIGH is the voltage level)
      ledStatus = 2;
      bot.sendMessage(chat_id, "Warmwasser für 10 MIN AN", "");
      lasttime_waterpump = millis();
    }

    if (text == "/aus") {
      ledStatus = 0;
      digitalWrite(ledPin2, LOW);  // turn the LED off (LOW is the voltage level)
      digitalWrite(ledPin2, LOW);  // turn the LED off (LOW is the voltage level)
      bot.sendMessage(chat_id, "Warmwasser IST AUS", "");
    }

    if (text == "/status") {
      if (ledStatus) {
        bot.sendMessage(chat_id, "WARMWASSSER ist AN", "");

      } else {
        bot.sendMessage(chat_id, "WARMWASSSER ist AUS", "");
      }
    }

    if (text == "/options") {
      String keyboardJson = "[[\"/timer5\", \"/timer10\"],[\"/status\", \"/aus\"]]";
      bot.sendMessageWithReplyKeyboard(chat_id, "Wähle aus den folgenden Optionen", "", keyboardJson, true);
    }

    if (text == "/start") {
      String welcome = "Willkommen auf der Warmwasserpumpe, " + from_name + ".\n";
      welcome += "Du kannst folgende Befehle senden.\n\n";
      welcome += "/timer5 : Warmwasserpumpe ist für 5 Min AN ON\n";
      welcome += "/timer10 : Warmwasserpumpe ist für 10 Min AN ON\n";
      welcome += "/aus : Schaltet die Warmwasserpumpe AUS\n";
      welcome += "/status : Zeigt den aktuellen Status der Wasserpumpe an\n";
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
  // Set WiFi to station mode and disconnect from an AP if it was Previously connected
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  WiFi.hostname("WARMWASSER");
  // attempt to connect to Wifi network:
  Serial.print("Connecting to Wifi SSID ");
  Serial.print(WIFI_SSID);
  Serial.print(" ");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  secured_client.setTrustAnchors(&cert);  // Add root certificate for api.telegram.org
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    digitalWrite(ledPin, blink);
    blink = !blink;
    delay(500);
  }
  Serial.println();

  Serial.print("WiFi connected. IP address: ");
  Serial.println(WiFi.localIP());

  
  
  digitalWrite(ledPin, HIGH);   // initialize pin as off
  digitalWrite(ledPin2, LOW);  // initialize pin as off

  lasttime_waterpump = millis();
}

void loop() {
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
    check_legionellen();
    ausschalten_5min();
    ausschalten_10min();
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
  } else if (legionellencheck_active == 1) {
    if (lasttime_waterpump + min_waterpump_on_duration > now) {
      digitalWrite(ledPin2, LOW);
      legionellencheck_active = 0;
      Serial.println("LegionellenCheck Beendet");
    }
  } else {
    // do nothing
  }
}

void ausschalten_5min(void) {
  unsigned long now = millis();
  unsigned long min_waterpump_on_duration = 300 * 1000;  //5min
  Serial.print("Now: ");
  Serial.print(now);
  Serial.print(" LastTime:");
  Serial.print(lasttime_waterpump);
  Serial.print(" Min: ");
  Serial.println(min_waterpump_on_duration);
  if ((now > (lasttime_waterpump + min_waterpump_on_duration)) && (ledStatus == 1)) {
    digitalWrite(ledPin, LOW);
    digitalWrite(ledPin2, LOW);
    ledStatus = 0;
    Serial.println("5 Min Timer beendet");
  }
}

void ausschalten_10min(void) {
  unsigned long now = millis();
  unsigned long min_waterpump_on_duration = 600 * 1000;  //10min
  if ((now > (lasttime_waterpump + min_waterpump_on_duration)) && (ledStatus == 2)) {
    digitalWrite(ledPin, LOW);
    digitalWrite(ledPin2, LOW);
    ledStatus = 0;
    Serial.println("10 Min Timer beendet");
  }
}