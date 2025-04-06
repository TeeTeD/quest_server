#include <WiFi.h>
#include <AsyncTCP.h>
#include <vector>
#include <string>
#include <EEPROM.h>
#include <Adafruit_NeoPixel.h>
#include <esp_system.h>
#include <queue>


struct LedCommand {
  uint8_t strip;
  uint16_t pixel;
  uint8_t color_red;
  uint8_t color_green;
  uint8_t color_blue;
  uint32_t duration;
};

QueueHandle_t led_queue = xQueueCreate(50, sizeof(LedCommand));

// Настройки Wi-Fi
const char* ssid = "Keenetic";
const char* password = "9250665348";

unsigned long heartbeat_bpm = 1000;  //millis

// Настройки сервера
const char* serverIP = "192.168.1.62";  // Замените на IP вашего сервера
const int serverPort = 8800;           // Замените на порт вашего сервера

// Объект асинхронного TCP-клиента
AsyncClient client;

std::string tcp_data_buffer;

//////////////////////////////////////////////////  device data //////////////////////////////////////////////////
uint32_t device_type = 0;
unsigned long lastInterruptTime = 0;
const unsigned long debounceTime = 1;
bool _test = false;
unsigned long heartbeat_timer = 0;

struct PinState {
  uint8_t pinNumber;
  bool state;
};

struct PinStateInterrupt {
  uint8_t pinNumber;
  bool state;
  bool last_state;
};

std::vector<PinState> pins_to_check_interrupt;
std::vector<PinStateInterrupt> interruptPins;

const uint16_t leds_amount = 10;

Adafruit_NeoPixel leds[leds_amount];
bool need_to_update_leds[leds_amount];
uint64_t leds_update_timer = 0;
uint16_t leds_update_bpm = 10;

uint32_t RED = leds[0].Color(255, 0, 0);
uint32_t GREEN = leds[0].Color(0, 255, 0);
uint32_t BLUE = leds[0].Color(0, 0, 255);
uint32_t WHITE = leds[0].Color(255, 255, 255);
uint32_t OFF = leds[0].Color(0, 0, 0);
uint32_t RANDOM = leds[0].Color(255, 34, 200);

uint32_t LEDS_SMOOTH_TIMING_GRADE = 50;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t getEepromDeviceType() {
  uint32_t _data = 0;
  EEPROM.begin(16);

  _data += EEPROM.read(0x00);
  _data = _data << 8;
  _data += EEPROM.read(0x01);
  _data = _data << 8;
  _data += EEPROM.read(0x02);
  _data = _data << 8;
  _data += EEPROM.read(0x03);

  EEPROM.commit();
  return _data;
}

void eepromTestDeviceType() {
  EEPROM.begin(16);

  EEPROM.write(0x00, 'a');
  EEPROM.write(0x01, 'b');
  EEPROM.write(0x02, 'c');
  EEPROM.write(0x03, 'd');

  EEPROM.commit();
}

void setup() {
  Serial.begin(115200);
  delay(10000);

  //eepromTestDeviceType();

  device_type = getEepromDeviceType();

  // Подключение к Wi-Fi
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");

  // Настройка обработчиков событий TCP-клиента
  client.onConnect([](void* arg, AsyncClient* client) {
    Serial.println("Connected to server");

    uint8_t id_buffer[4];

    id_buffer[0] = (device_type >> 24) & 0xFF;
    id_buffer[1] = (device_type >> 16) & 0xFF;
    id_buffer[2] = (device_type >> 8) & 0xFF;
    id_buffer[3] = device_type & 0xFF;

    client->write((const char*)id_buffer, 4);
    heartbeat_timer = millis();
  });

  client.onDisconnect([](void* arg, AsyncClient* client) {
    Serial.println("Disconnected from server");
    // Попытка переподключения
    client->connect(serverIP, serverPort);
  });

  client.onData(onDataCallback);

  client.onError([](void* arg, AsyncClient* client, int8_t error) {
    Serial.print("Connection error: ");
    Serial.println(error);
  });

  // Подключение к серверу
  if (!client.connect(serverIP, serverPort)) {
    Serial.println("Failed to connect to server");
  }

  xTaskCreate(
    ledProcessorTask,
    "LED Processor",
    4096,
    NULL,
    2,
    NULL
  );
}

void onDataCallback(void* arg, AsyncClient* client, void* data, size_t len) {
  std::string _data(static_cast<const char*>(data), len);
  tcp_data_buffer += _data;

  size_t delimiterPos = tcp_data_buffer.find(';');
  while (delimiterPos != std::string::npos) {
    delimiterPos = tcp_data_buffer.find(';');
    if (delimiterPos != std::string::npos) {
      std::string message = tcp_data_buffer.substr(0, delimiterPos);

      Serial.print("Received message: ");
      Serial.println(message.c_str());

      parseData(message);

      tcp_data_buffer.erase(0, delimiterPos + 1);
    }
  }
}

void parseData(std::string _mess) {
  std::string command = _mess.substr(0, 4);
  std::string data = _mess.substr(4);

  if (command == "INIT") {
    INITproc(data);
  }

  else if (command == "RSST") {
    RSSTproc(data);
  }

  else if (command == "CONF") {
    CONFproc(data);
  }

  else if (command == "SETD") {
    SETDproc(data);
  }

  else if (command == "LC00") {
    LC00proc(data);
  }

  else if (command == "LC01") {
    LC01proc(data);
  }

  else if (command == "LC02") {
    LC02proc(data);
  }

  else if (command == "LC12") {
    LC12proc(data);
  }
}

void loop() {
  if ((millis() - leds_update_timer) >= leds_update_bpm){
    for(int i = 0; i < leds_amount; i++){
      if (need_to_update_leds[i]){
        leds[i].show();
        need_to_update_leds[i] = false;
      }
    }

    leds_update_timer = millis();
  }


  if ((millis() - heartbeat_timer) >= heartbeat_bpm) {
    client.write("heartbeat;");
    heartbeat_timer = millis();
  }

  if (Serial.available() > 0) {
    String _foo = Serial.readStringUntil('\n');
    std::string _bar(_foo.c_str());
    client.write(_bar.c_str());
    parseData(_bar);
  }

  if (!pins_to_check_interrupt.empty()) {
    PinState _last = pins_to_check_interrupt.back();
    Serial.print("Пин: ");
    Serial.print(_last.pinNumber);
    Serial.print(", Состояние: ");
    Serial.println(_last.state ? "HIGH" : "LOW");

    std::string _msg = "GETD";
    _msg += _last.pinNumber / 10 + '0';
    _msg += _last.pinNumber % 10 + '0';
    _last.state ? _msg += '1' : _msg += '0';
    _msg += ';';
    client.write(_msg.c_str());

    pins_to_check_interrupt.pop_back();
  }
}
