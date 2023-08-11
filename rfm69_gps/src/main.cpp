/* Copyright 2023 by Ralph Blach under the gpl3 public license. see https://www.gnu.org/licenses/gpl-3.0.en.html#license-text
for the entire text*/

/* this program requires the installtion of the radio head Library from
https://registry.platformio.org/libraries/epsilonrt/RadioHead/installation*/
/**
 *  
 *  @file main.cpp 
    @author Ralph Blach
    @date Aug 8, 2023
    @brief This is the main file for an Arduion project
    
**/

#include <Arduino.h>
#include <rfm_69_functions.h>

// this is the arduino setup and loop files


void setup() {
   /**
        @brief This is the setup routine, Just calls rfm_69_setup

        @return Nothing
    */
  rfm_69_setup();
}

void loop() {
   /**
        @brief This is the loop subrourine

        @return Nothing
    */
  rfm_69_loop();
}
