/* Copyright 2023 by Ralph Blach under the gpl3 public license. see https://www.gnu.org/licenses/gpl-3.0.en.html#license-text
for the entire text*/
#include <Arduino.h>
#include <rfm_69_functions.h>

// this is the arduino setup and loop files


void setup() {
  // put your setup code here, to run once:
  rfm_69_setup();
}

void loop() {
  rfm_69_loop();
}
