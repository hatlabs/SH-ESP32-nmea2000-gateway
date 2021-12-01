#include <Arduino.h>

#define CAN_RX_PIN GPIO_NUM_34
#define CAN_TX_PIN GPIO_NUM_32
#define SDA_PIN 16
#define SCL_PIN 17

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <N2kMessages.h>
#include <ActisenseReader.h>
#include <NMEA2000_esp32.h>

#include <ReactESP.h>
using namespace reactesp;

ReactESP app;

#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels

TwoWire* i2c;

Stream *read_stream=&Serial;
Stream *forward_stream=&Serial;

tActisenseReader actisense_reader;

Adafruit_SSD1306* display;

tNMEA2000* nmea2000;

void ToggleLed() {
  static bool led_state = false;
  digitalWrite(LED_BUILTIN, led_state);
  led_state = !led_state;
}

int num_n2k_messages = 0;
void HandleStreamN2kMsg(const tN2kMsg &message) {
  // N2kMsg.Print(&Serial);
  num_n2k_messages++;
  ToggleLed();
}

int num_actisense_messages = 0;
void HandleStreamActisenseMsg(const tN2kMsg &message) {
  // N2kMsg.Print(&Serial);
  num_actisense_messages++;
  ToggleLed();
  nmea2000->SendMsg(message);
}

void setup() {
  // setup serial output
  Serial.begin(115200);
  delay(100);

  // toggle the LED pin at rate of 1 Hz
  pinMode(LED_BUILTIN, OUTPUT);
  app.onRepeatMicros(1e6 / 1, []() {
    ToggleLed();
  });

  // instantiate the NMEA2000 object
  nmea2000 = new tNMEA2000_esp32(CAN_TX_PIN, CAN_RX_PIN);

  // input the NMEA 2000 messages

  // Reserve enough buffer for sending all messages. This does not work on small
  // memory devices like Uno or Mega
  nmea2000->SetN2kCANSendFrameBufSize(250);
  nmea2000->SetN2kCANReceiveFrameBufSize(250);

  // Set Product information
  nmea2000->SetProductInformation(
      "20210331",                      // Manufacturer's Model serial code (max 32 chars)
      103,                             // Manufacturer's product code
      "SH-ESP32 NMEA 2000 USB GW",     // Manufacturer's Model ID (max 33 chars)
      "0.1.0.0 (2021-03-31)",          // Manufacturer's Software version code (max 40 chars)
      "0.0.3.1 (2021-03-07)"           // Manufacturer's Model version (max 24 chars)
  );
  // Set device information
  nmea2000->SetDeviceInformation(
      1,    // Unique number. Use e.g. Serial number.
      130,  // Device function=Analog to NMEA 2000 Gateway. See codes on
            // http://www.nmea.org/Assets/20120726%20nmea%202000%20class%20&%20function%20codes%20v%202.00.pdf
      25,   // Device class=Inter/Intranetwork Device. See codes on
           // http://www.nmea.org/Assets/20120726%20nmea%202000%20class%20&%20function%20codes%20v%202.00.pdf
      2046  // Just choosen free from code list on
            // http://www.nmea.org/Assets/20121020%20nmea%202000%20registration%20list.pdf
  );

  nmea2000->SetForwardStream(forward_stream); 
  nmea2000->SetMode(tNMEA2000::N2km_ListenAndNode);
  // nmea2000->SetForwardType(tNMEA2000::fwdt_Text); // Show bus data in clear text
  nmea2000->SetForwardOwnMessages(false); // do not echo own messages.
  nmea2000->SetMsgHandler(HandleStreamN2kMsg);
  nmea2000->Open();

  actisense_reader.SetReadStream(read_stream);
  actisense_reader.SetDefaultSource(75);
  actisense_reader.SetMsgHandler(HandleStreamActisenseMsg); 

  // No need to parse the messages at every single loop iteration; 1 ms will do
  app.onRepeat(1, []() { 
    nmea2000->ParseMessages();
    actisense_reader.ParseMessages();
  });

  // initialize the display
  i2c = new TwoWire(0);
  i2c->begin(SDA_PIN, SCL_PIN);
  display = new Adafruit_SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, i2c, -1);
  if (!display->begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
  }
  delay(100);
  display->setRotation(2);
  display->clearDisplay();
  display->display();

  // update results

  app.onRepeat(1000, []() {
    display->clearDisplay();
    display->setTextSize(1);
    display->setCursor(0, 0);
    display->setTextColor(SSD1306_WHITE);
    display->printf("SH-ESP32 N2K USB GW\n");
    display->printf("Uptime: %lu\n", millis() / 1000);
    display->printf("RX: %d\n", num_n2k_messages);
    display->printf("TX: %d\n", num_actisense_messages);
    
    display->display();

    num_n2k_messages = 0;
    num_actisense_messages = 0;
  });
}

void loop() {
  app.tick();
}
