#include <WiFi.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_SCD30.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>

// pin config
#define TFT_CS   15
#define TFT_DC   2
#define TFT_RST  4
#define SD_CS    5

// wifi credentials
const char* ssid = "SSID";
const char* password = "password";

// mqtt info
const char* mqtt_server = "io.adafruit.com";
const int mqtt_port = 1883;
const char* aio_username = "IOUSER";
const char* aio_key = "IOKEY";

// mqtt topics
String baseTopic = String(aio_username) + "/feeds/";
const char* feed_temp = "temperature";
const char* feed_hum  = "humidity";
const char* feed_press= "pressure";
const char* feed_co2  = "co2";
const char* feed_time = "last_update";

// objects
WiFiClient espClient;
PubSubClient client(espClient);

Adafruit_BMP280 bmp;
Adafruit_SCD30 scd30;
Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);

// functions
void connectWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" connected!");
}

void connectMQTT() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    String clientId = "ESP32-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str(), aio_username, aio_key)) {
      Serial.println("connected!");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      delay(5000);
    }
  }
}

void publishData(const char* feed, float value) {
  String topic = baseTopic + feed;
  char payload[16];
  dtostrf(value, 6, 2, payload);
  client.publish(topic.c_str(), payload);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Smart Environmental Monitor Starting...");

  // Connect WiFi
  connectWiFi();

  // Init MQTT
  client.setServer(mqtt_server, mqtt_port);
  connectMQTT();

  // Init I2C sensors
  Wire.begin();
  if (!bmp.begin(0x76)) Serial.println("BMP280 not found!");
  if (!scd30.begin()) Serial.println("SCD30 not found!");

  // Init TFT
  tft.begin();
  tft.setRotation(-1);
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("Env Monitor Ready");

  // Init SD
  if (!SD.begin(SD_CS)) {
    Serial.println("SD init failed!");
  } else {
    Serial.println("SD ok");
  }
}

void loop() {
  if (!client.connected()) connectMQTT();
  client.loop();

  float bmpTemp = bmp.readTemperature();
  float bmpPres = bmp.readPressure() / 100.0F;

  float co2 = NAN, scdTemp = NAN, scdHum = NAN;
  if (scd30.dataReady() && scd30.read()) {
    co2 = scd30.CO2;
    scdTemp = scd30.temperature;
    scdHum = scd30.relative_humidity;
  }

  if (isnan(bmpTemp) || isnan(bmpPres) || isnan(co2) || isnan(scdTemp) || isnan(scdHum)) {
    Serial.println("Invalid reading, skipping...");
    delay(2000);
    return;
  }

  Serial.printf("BMP: T=%.2f°C, P=%.2f hPa | SCD30: CO2=%.0fppm, T=%.2f°C, H=%.2f%%\n",
                bmpTemp, bmpPres, co2, scdTemp, scdHum);

  publishData(feed_temp, scdTemp);
  publishData(feed_hum, scdHum);
  publishData(feed_press, bmpPres);
  publishData(feed_co2, co2);

  String timeStr = String(millis() / 1000) + "s";
  client.publish((baseTopic + feed_time).c_str(), timeStr.c_str());

  // TFT Display
  tft.fillRect(0, 40, 240, 120, ILI9341_BLACK);
  tft.setCursor(10, 50);
  tft.printf("T: %.2fC", scdTemp);
  tft.setCursor(10, 70);
  tft.printf("H: %.2f%%", scdHum);
  tft.setCursor(10, 90);
  tft.printf("P: %.2f hPa", bmpPres);
  tft.setCursor(10, 110);
  tft.printf("CO2: %.0f ppm", co2);

  delay(5000);
}
