void IRAM_ATTR handlePinInterrupt() {
  unsigned long currentTime = millis();
  if (currentTime - lastInterruptTime > debounceTime) {
    for (auto& pin : interruptPins) {
      bool currentState = digitalRead(pin.pinNumber);
      if (pin.last_state != currentState){
        _test = true;
        pins_to_check_interrupt.push_back({pin.pinNumber, currentState});
        pin.last_state = currentState;
        break;
      }
    }
  lastInterruptTime = currentTime;
  }
}

void CONFproc(std::string data){
  uint8_t pin = ((data[0] - '0') * 10) + (data[1] - '0');
  uint8_t mode = 0;
  switch (data[2]){
    case '0':
      mode = INPUT;
      break;
    case '1':
      mode = INPUT_PULLUP;
      break;
    case '2':
      mode = OUTPUT;
      break;
  }

  pinMode(pin, mode);

  if ((mode == INPUT) || (mode == INPUT_PULLUP)){
    attachInterrupt(digitalPinToInterrupt(pin), handlePinInterrupt, CHANGE);
    interruptPins.push_back({pin, digitalRead(pin), digitalRead(pin)});
    pins_to_check_interrupt.push_back({pin, digitalRead(pin)});
  }
}

void RSSTproc(std::string data){
  esp_restart();
}

void INITproc(std::string data){
  uint8_t id_buffer[4];

  id_buffer[0] = (device_type >> 24) & 0xFF;
  id_buffer[1] = (device_type >> 16) & 0xFF;
  id_buffer[2] = (device_type >> 8) & 0xFF;
  id_buffer[3] = device_type & 0xFF;

  client.write((const char*)id_buffer, 4);
}

void SETDproc(std::string data){
  uint8_t pin = ((data[0] - '0') * 10) + (data[1] - '0');
  bool mode = 0;

  switch (data[2]){
    case '0':
      mode = 0;
      break;
    case '1':
      mode = 1;
      break;
  }
  Serial.println(pin);
  Serial.println(mode);
  digitalWrite(pin, mode);
}

void LC00proc(std::string data){
  uint16_t pin = ((data[0] - '0') * 10) + (data[1] - '0');
  uint16_t leds_amount = ((data[2] - '0') * 1000) + ((data[3] - '0') * 100) + ((data[4] - '0') * 10) + (data[5] - '0');
  uint8_t brightness = ((data[6] - '0') * 100) + ((data[7] - '0') * 10) + (data[8] - '0');
  uint8_t strip_number = data[9] - '0';

  leds[strip_number].setPin(pin);
  leds[strip_number].updateLength(leds_amount);
  leds[strip_number].begin();
  leds[strip_number].setBrightness(brightness);
  leds[strip_number].clear();
  leds[strip_number].show();
}

void LC01proc(std::string data){
  uint8_t strip_number = data[0] - '0';

  leds[strip_number].show();
}

void LC02proc(std::string data){
  for (int i = 0; i < data.size(); i++){
    data[i] -= '0';
  }

  uint8_t strip_number = data[0];
  uint16_t pixel_number = (data[1] * 1000) + (data[2] * 100) + (data[3] * 10) + data[4];
  uint8_t red = (data[5] * 100) + (data[6] * 10) + data[7];
  uint8_t green = (data[8] * 100) + (data[9] * 10) + data[10];
  uint8_t blue = (data[11] * 100) + (data[12] * 10) + data[13];

  leds[strip_number].setPixelColor(pixel_number, red, green, blue);
}

void LC12proc(std::string data){
  for (char &c : data) {
    c -= '0';
  }

  if(data.size() < 19) {
    Serial.println("Ошибка: некорректная длина строки данных");
    return;
  }

  uint8_t strip_number = data[0];
  uint16_t pixel_number = (data[1] * 1000) + (data[2] * 100) + (data[3] * 10) + data[4];
  uint8_t red = (data[5] * 100) + (data[6] * 10) + data[7];
  uint8_t green = (data[8] * 100) + (data[9] * 10) + data[10];
  uint8_t blue = (data[11] * 100) + (data[12] * 10) + data[13];
  uint32_t time = (data[14] * 10000) + (data[15] * 1000) + (data[16] * 100) + (data[17] * 10) + data[18];

  uint32_t delay_time = time >= LEDS_SMOOTH_TIMING_GRADE ? time / LEDS_SMOOTH_TIMING_GRADE : 0;

  LedCommand cmd = {
    .strip = strip_number,
    .pixel = pixel_number,
    .color_red = red,
    .color_green = green,
    .color_blue = blue,
    .duration = delay_time
  };

  xQueueSend(led_queue, &cmd, 100 / portTICK_PERIOD_MS);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ledProcessorTask(void *pvParameters) {
  LedCommand cmd;
  
  while(true) {
    if(xQueueReceive(led_queue, &cmd, portMAX_DELAY)) {
      uint32_t current_color = leds[cmd.strip].getPixelColor(cmd.pixel);

      uint8_t current_r = (current_color >> 16) & 0xFF;
      uint8_t current_g = (current_color >> 8) & 0xFF;
      uint8_t current_b = current_color & 0xFF;
      
      for(uint8_t i = 1; i <= LEDS_SMOOTH_TIMING_GRADE; i++) {
        uint8_t r = map_uint8(i, 0, LEDS_SMOOTH_TIMING_GRADE, current_r, cmd.color_red);
        uint8_t g = map_uint8(i, 0, LEDS_SMOOTH_TIMING_GRADE, current_g, cmd.color_green);
        uint8_t b = map_uint8(i, 0, LEDS_SMOOTH_TIMING_GRADE, current_b, cmd.color_blue);
        
        leds[cmd.strip].setPixelColor(cmd.pixel, r, g, b);
        need_to_update_leds[cmd.strip] = true;
        
        vTaskDelay(cmd.duration / portTICK_PERIOD_MS);
      }
      Serial.println("[Task] Done!");
    }
  }
}

uint8_t map_uint8(uint8_t x, uint8_t in_min, uint8_t in_max, uint8_t out_min, uint8_t out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}























