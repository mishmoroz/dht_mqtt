#define AIO_USERNAME  "yourname"
#define AIO_KEY       "yourkey"
#define MQTT_PROTOCOL_LEVEL    4

#define MQTT_CTRL_CONNECT   0x1
#define MQTT_CTRL_CONNECTACK  0x2
#define MQTT_CTRL_PUBLISH   0x3
#define MQTT_CTRL_PUBACK    0x4
#define MQTT_CTRL_PUBREC    0x5
#define MQTT_CTRL_PUBREL    0x6
#define MQTT_CTRL_PUBCOMP   0x7
#define MQTT_CTRL_SUBSCRIBE   0x8
#define MQTT_CTRL_SUBACK    0x9
#define MQTT_CTRL_UNSUBSCRIBE 0xA
#define MQTT_CTRL_UNSUBACK    0xB
#define MQTT_CTRL_PINGREQ   0xC
#define MQTT_CTRL_PINGRESP    0xD
#define MQTT_CTRL_DISCONNECT  0xE

#define MQTT_QOS_1        0x1
#define MQTT_QOS_0        0x0

/* Adjust as necessary, in seconds */
#define MQTT_CONN_KEEPALIVE   600

#define MQTT_CONN_USERNAMEFLAG  0x80
#define MQTT_CONN_PASSWORDFLAG  0x40
#define MQTT_CONN_WILLRETAIN  0x20
#define MQTT_CONN_WILLQOS_1   0x08
#define MQTT_CONN_WILLQOS_2   0x18
#define MQTT_CONN_WILLFLAG    0x04
#define MQTT_CONN_CLEANSESSION  0x02

#define DEFAULT_BUFFER_SIZE   200
#define DEFAULT_TIMEOUT     10000
#define DEFAULT_CRLF_COUNT    2
#include "DHT.h"
#define DHTPIN 2 // пин 2
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);
#define TINY_GSM_MODEM_SIM800


// Define the serial console for debug prints, if needed
#define TINY_GSM_DEBUG SerialMon

// Range to attempt to autobaud
#define GSM_AUTOBAUD_MIN 9600
#define GSM_AUTOBAUD_MAX 38400

// Add a reception delay, if needed
#define TINY_GSM_YIELD() { delay(2); }

// Set serial for debug console (to the Serial Monitor, default speed 115200)
#define SerialMon Serial

// or Software Serial on Uno, Nano
#include <SoftwareSerial.h>
SoftwareSerial SerialAT(5, 4); // RX, TX


// GPRS credentials
const char apn[]  = "internet";
const char user[] = "gdata";
const char pass[] = "gdata";

// MQTT details
const char* broker = "io.adafruit.com";
const char* topic = "yourname/feeds/dht";
const char* topic2 = "yourname/feeds/dht2";
uint8_t active_profile = 0;
char clientID[] ="";
char will_topic[] = "";
char will_payload[] ="";
uint8_t will_qos = 1;
uint8_t will_retain = 0;
int16_t packet_id_counter = 0;
uint8_t _buffer[150];
#include <TinyGsmClient.h>
#include <PubSubClient.h>
#include "mqtt.h"

TinyGsm modem(SerialAT);

TinyGsmClient client(modem);
PubSubClient mqtt(client);

#define LED_PIN 13
int ledStatus = LOW;

long lastReconnectAttempt = 0;

void setup() {
  dht.begin();
  // Set console baud rate
  SerialMon.begin(9600);
  delay(10);

  pinMode(LED_PIN, OUTPUT);

  SerialMon.println("Wait...");

  // Set GSM module baud rate
  SerialAT.begin(9600);
  delay(3000);
  SerialMon.println("Initializing modem...");
  modem.restart();

  String modemInfo = modem.getModemInfo();
  SerialMon.print("Modem: ");
  SerialMon.println(modemInfo);

#if TINY_GSM_USE_GPRS && defined TINY_GSM_MODEM_XBEE
  // The XBee must run the gprsConnect function BEFORE waiting for network!
  modem.gprsConnect(apn, gprsUser, gprsPass);
#endif

  SerialMon.print("Waiting for network...");
  if (!modem.waitForNetwork(240000L)) {
    SerialMon.println(" fail");
    delay(10000);
    return;
  }
  SerialMon.println(" OK");

  if (modem.isNetworkConnected()) {
    SerialMon.println("Network connected");
  }

#if TINY_GSM_USE_GPRS && defined TINY_GSM_MODEM_HAS_GPRS
    SerialMon.print(F("Connecting to "));
  SerialMon.print(apn);
    if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    SerialMon.println(" fail");
      delay(10000);
      return;
  }
  SerialMon.println(" OK");
#endif

  // MQTT Broker setup
  mqtt.setServer(broker, 1883);
  mqtt.setCallback(mqttCallback);
  mqttConnect();
}

boolean mqttConnect() {
  SerialMon.print("Connecting to ");
  SerialMon.print(broker);
  uint16_t len;
  len = MQTT_connectpacket(_buffer);
  
  SerialAT.write("AT+CIPSTART=\"TCP\",\"io.adafruit.com\",\"1883\"\n");
  delay(8000);
  printAnswer();
  Serial.println("tcp end");
  SerialAT.write("AT+CIPSEND=58\n");
  delay(1000);
  printAnswer();
  Serial.println("connect");
  SerialAT.write(_buffer, len);
  SerialAT.write(0x1A);
  delay(500);
  printAnswer();
  return true;
}
void printAnswer(){
  while(SerialAT.available() > 0){
    Serial.write(SerialAT.read());
  }
}

void loop() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
    if (isnan(t) || isnan(h)) {
    Serial.println("Ошибка!");
    return;
  }
  Serial.println("Температура: ");
  Serial.print(t);
  Serial.println(" °C ");
  Serial.println("Влажность: ");
  Serial.println(h);
  uint16_t len;
  char temperature[5];
  dtostrf(t, 5, 2, temperature);
  len = MQTT_publishPacket(_buffer,topic, temperature ,1);
  SerialAT.write("AT+CIPSEND=29\n");
  delay(500);
  //printAnswer();
  Serial.println("publish temperature");
  SerialAT.write(_buffer, len);
  SerialAT.write(0x1A);
  delay(500);
  //printAnswer();
  
  char humidity[5];
  dtostrf(h, 5, 2, humidity);
  len = MQTT_publishPacket(_buffer,topic2, humidity ,1);
  SerialAT.write("AT+CIPSEND=30\n");
  delay(500);
  //printAnswer();
  Serial.println("publish humidity");
  SerialAT.write(_buffer, len);
  SerialAT.write(0x1A);
  delay(500);
  //printAnswer();
  delay(5000);
}

void mqttCallback(char* topic, byte* payload, unsigned int len) {
  SerialMon.print("Message arrived [");
  SerialMon.print(topic);
  SerialMon.print("]: ");
  SerialMon.write(payload, len);
  SerialMon.println();
}

uint16_t MQTT_connectpacket(uint8_t* packet)
{
  uint8_t*_packet = packet;
  uint16_t _length;

  _packet[0] = (MQTT_CTRL_CONNECT << 4);
  _packet+=2;
  _packet = AddStringToBuf(_packet, "MQTT");
  _packet[0] = MQTT_PROTOCOL_LEVEL;
  _packet++;

  _packet[0] = MQTT_CONN_CLEANSESSION;
  if (will_topic && strlen(will_topic) != 0) {
    _packet[0] |= MQTT_CONN_WILLFLAG;
    if(will_qos == 1)
    _packet[0] |= MQTT_CONN_WILLQOS_1;
    else if(will_qos == 2)
    _packet[0] |= MQTT_CONN_WILLQOS_2;
    if(will_retain == 1)
    _packet[0] |= MQTT_CONN_WILLRETAIN;
  }
  if (strlen(AIO_USERNAME) != 0)
  _packet[0] |= MQTT_CONN_USERNAMEFLAG;
  if (strlen(AIO_KEY) != 0)
  _packet[0] |= MQTT_CONN_PASSWORDFLAG;
  _packet++;

  _packet[0] = MQTT_CONN_KEEPALIVE >> 8;
  _packet++;
  _packet[0] = MQTT_CONN_KEEPALIVE & 0xFF;
  _packet++;
  if (strlen(clientID) != 0) {
    _packet = AddStringToBuf(_packet, clientID);
    } else {
    _packet[0] = 0x0;
    _packet++;
    _packet[0] = 0x0;
    _packet++;
  }
  if (will_topic && strlen(will_topic) != 0) {
    _packet = AddStringToBuf(_packet, will_topic);
    _packet = AddStringToBuf(_packet, will_payload);
  }

  if (strlen(AIO_USERNAME) != 0) {
    _packet = AddStringToBuf(_packet, AIO_USERNAME);
  }
  if (strlen(AIO_KEY) != 0) {
    _packet = AddStringToBuf(_packet, AIO_KEY);
  }
  _length = _packet - packet;
  packet[1] = _length-2;

  return _length;
}

uint16_t MQTT_publishPacket(uint8_t *packet, const char *topic, char *data, uint8_t qos)
{
  uint8_t *_packet = packet;
  uint16_t _length = 0;
  uint16_t Datalen=strlen(data);

  _length += 2;
  _length += strlen(topic);
  if(qos > 0) {
    _length += 2;
  }
  _length += Datalen;
  _packet[0] = MQTT_CTRL_PUBLISH << 4 | qos << 1;
  _packet++;
  do {
    uint8_t encodedByte = _length % 128;
    _length /= 128;
    if ( _length > 0 ) {
      encodedByte |= 0x80;
    }
    _packet[0] = encodedByte;
    _packet++;
  } while ( _length > 0 );
  _packet = AddStringToBuf(_packet, topic);
  if(qos > 0) {
    _packet[0] = (packet_id_counter >> 8) & 0xFF;
    _packet[1] = packet_id_counter & 0xFF;
    _packet+=2;
    packet_id_counter++;
  }
  memmove(_packet, data, Datalen);
  _packet+= Datalen;
  _length = _packet - packet;

  return _length;
}

uint8_t* AddStringToBuf(uint8_t *_buf, const char *_string)
{
  uint16_t _length = strlen(_string);
  _buf[0] = _length >> 8;
  _buf[1] = _length & 0xFF;
  _buf+=2;
  strncpy((char *)_buf, _string, _length);
  return _buf + _length;
}
