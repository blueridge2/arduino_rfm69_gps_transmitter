/* Copyright 2023 by Ralph Blach under the gpl3 public license. see https://www.gnu.org/licenses/gpl-3.0.en.html#license-text
for the entire text*/

/* this program requires the installtion of the radio head Library from
https://registry.platformio.org/libraries/epsilonrt/RadioHead/installation*/


#include <Arduino.h>
#include <rfm_69_functions.h>

// this is the arduino setup and loop files


void setup() {
  rfm_69_setup();
}

void loop() {
  rfm_69_loop();
}
