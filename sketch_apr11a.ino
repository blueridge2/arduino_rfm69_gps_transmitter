 /**
  * @file  sketch_apr11a.ino
 * @author Ralph Blach
 * @date April 15, 2019
 * @brief This is a program to receive gps information on an Arduino(R) and transmit it on a 
 * rfm69 radio
 * 
 * This program uses a Adafruit(R) feather wings GPS bonnet with a GlobalTop(R) MTK3339 on on it.   
 * 
 *
 **/
#include <SPI.h>
#include <RH_RF69.h>
#include <RHReliableDatagram.h>
/**
 * @brief  create a name for the gpsSerial port
 * @param GPSSerial
 */
#define GPSSerial Serial1
/**
 * @brief this is the size of the data buffer where the data from the MK3339 will be placed.
 * @param GPS_RECEIVER_BUFFER_SIZE
 */
#define GPS_RECEIVER_BUFFER_SIZE 100
#define GPSSerial Serial1
/**
 * @brief this is the number of array entries for the tokenizer.  you can have 15 tokens
 * @param ARRAY_SIZE
 */
#define ARRAY_SIZE 15

/************ Radio Setup ***************/
/**
 * @brief radio frequency
 * @param RF69_FREQ
 */
// Change to 434.0 or other frequency, must match RX's freq!
#define RF69_FREQ 433.0

// Where to send packets to!
/**
 * @brief destination address
 * @param DEST_ADDRESS
 */
#define DEST_ADDRESS   1
// change addresses for each client board, any number :)
#define MY_ADDRESS     2


#if defined (__AVR_ATmega32U4__) // Feather 32u4 w/Radio
  #define RFM69_CS      8
  #define RFM69_INT     7
  #define RFM69_RST     4
  #define LED           13
#endif

#if defined(ADAFRUIT_FEATHER_M0) // Feather M0 w/Radio
  #define RFM69_CS      8
  #define RFM69_INT     3
  #define RFM69_RST     4
  #define LED           13
#endif

#if defined (__AVR_ATmega328P__)  // Feather 328P w/wing
  #define RFM69_INT     3  // 
  #define RFM69_CS      4  //
  #define RFM69_RST     2  // "A"
  #define LED           13
#endif

#if defined(ESP8266)    // ESP8266 feather w/wing
  #define RFM69_CS      2    // "E"
  #define RFM69_IRQ     15   // "B"
  #define RFM69_RST     16   // "D"
  #define LED           0
#endif

#if defined(ESP32)    // ESP32 feather w/wing
  #define RFM69_RST     13   // same as LED
  #define RFM69_CS      33   // "B"
  #define RFM69_INT     27   // "A"
  #define LED           13
#endif

/* Teensy 3.x w/wing
#define RFM69_RST     9   // "A"
#define RFM69_CS      10   // "B"
#define RFM69_IRQ     4    // "C"
#define RFM69_IRQN    digitalPinToInterrupt(RFM69_IRQ )
*/
 
/* WICED Feather w/wing 
#define RFM69_RST     PA4     // "A"
#define RFM69_CS      PB4     // "B"
#define RFM69_IRQ     PA15    // "C"
#define RFM69_IRQN    RFM69_IRQ
*/

// Singleton instance of the radio driver
RH_RF69 rf69(RFM69_CS, RFM69_INT);

// Class to manage message delivery and receipt, using the driver declared above
RHReliableDatagram rf69_manager(rf69, MY_ADDRESS);


int16_t packetnum = 0;  // packet counter, we increment per xmission

void setup() 
{
/**
 * @brief This is the setup program for the Arduino
 * 
 * This program sets only 
 *  - Sets up the debug serial port 115200
 *  - Sets up the GPS serial port to 9600 baud
 *  - output GMRC packets
 *  - only output once every 10 seconds
 * 
 * @return Nothing
 */
 // set up the GPS to only transmit GPRMC packets.  Thats all thats needed
  char gps_init_data[] = "$PMTK314,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*29\r\n";
 // set the the gps to only transmit once every 10 seconds.
  char gps_update_rate[] = "$PMTK220,10000*2F\r\n";
  int index;
  int j_index;
  char chr_to_send;
  // 9600 baud is the default rate for the Ultimate GPS
  GPSSerial.begin(9600);
  Serial.begin(115200);
  while (!Serial) { delay(1); } // wait until serial console is open, remove if not tethered to computer
  // only send GPRMC  packets
  for(j_index=0; j_index<3; j_index++)
  {
    //Serial.println("Sending init data to gps");
    //Serial.println(gps_init_data);
    for (index=0; index<strlen(gps_init_data); index++)
    {
      GPSSerial.write(gps_init_data[index]);
    }
  }
  // initial the GPS radio to only send updates once every 10 seconds
  for(j_index=0; j_index<3; j_index++)
  {
    //Serial.println("Sending update rate to gps");
    //Serial.println(gps_init_data);
    for (index=0; index<sizeof(gps_update_rate); index++)
    {
      GPSSerial.write(gps_update_rate[index]);
    }
  }
  pinMode(LED, OUTPUT);     
  pinMode(RFM69_RST, OUTPUT);
  digitalWrite(RFM69_RST, LOW);

  Serial.println("Feather Addressed RFM69 TX Test!");
  Serial.println();

  // manual reset the radio
  digitalWrite(RFM69_RST, HIGH);
  delay(10);
  digitalWrite(RFM69_RST, LOW);
  delay(10);
  
  if (!rf69_manager.init()) {
    Serial.println("RFM69 radio init failed");
    while (1);
  }
  Serial.println("RFM69 radio init OK!");
  // Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM (for low power module)
  // No encryption
  if (!rf69.setFrequency(RF69_FREQ)) {
    Serial.println("setFrequency failed");
  }

  // If you are using a high power RF69 eg RFM69HW, you *must* set a Tx power with the
  // ishighpowermodule flag set like this:
  rf69.setTxPower(20, true);  // range from 14-20 for power, 2nd arg must be true for 69HCW
  
  pinMode(LED, OUTPUT);

  Serial.print("RFM69 radio @");  Serial.print((int)RF69_FREQ);  Serial.println(" MHz");
}


// Dont put these on the stack, they only need to be allocated once.
uint8_t reply_buffer[RH_RF69_MAX_MESSAGE_LEN];
uint8_t data[] = "  OK";
const char comma = ',';
char *gps_parsed_data[ARRAY_SIZE];

// this is my ham call sign, and this is necessary for 433 mhz, in the US.  If you are not a ham radio operator
// in the United States, you cannot use the 433 Mhz radio and you cannot use my call sign.  If you are outside the US, 
// you need to know the rules in your country.
// the following data structures do not need to be reallocated every time
//                                           0123456
char radiopacket[RH_RF69_MAX_MESSAGE_LEN] = "kf4wbk#";
char gps_data[GPS_RECEIVER_BUFFER_SIZE];
       

void loop() {
  /**
 * @brief This is the loop subrourine
 * 
 * This program receives the data from the gps receiver,parses it, and sends it to Node 2 on the 
 * network.  Because this is running on the Ham band, it does not encrpt.
 * 
 * @return Nothing
 */
  char gps_char;
  uint8_t number_of_tokens;
  uint16_t index;
  uint16_t gps_char_index = 0;
  uint8_t reply_buffer_len; //this might be modified by the transmit program
  // terminate the radio packet with a 0,  after it has been used once it will have data after the #
  // this saves the slow auto intilaizaton of the packet ever time
  radiopacket[7] = 0; 
  // set the gps data to zero befor receive.
  memset(gps_data, 0, sizeof(gps_data));
  reply_buffer_len = sizeof(reply_buffer);
  
  while(1)
  {  
    // Read the serial data from the serial port attached to the software serial port.
    if (GPSSerial.available()) 
    {
      gps_char = GPSSerial.read();
      //Serial.print("char=");Serial.write(gps_char);Serial.print("\n");
      // if it is a new line character continue
      if (gps_char == 13)
        continue;
     
      // if it is a line feed, stop we have the packet
      if (gps_char == 10)
      {
        //Serial.println("Got return");
        if (strncmp(gps_data,"$GPRMC", 6) != 0)
        {
          gps_char_index = 0;  // reset the index to 0 because we did not get the expected packet
          //Serial.println("Got a continue");
          continue;
        }
        else
          break;
      }
      // make sure we never have a data overun.  Buffer overflows are nasty.
      if (gps_char_index < GPS_RECEIVER_BUFFER_SIZE-3)
        gps_data[gps_char_index++] = gps_char;
       else
        gps_data[gps_char_index] = gps_char; 
    }
  }
  gps_data[gps_char_index++] = 0; //make the gps data null terminated
  Serial.println(gps_data);
  // now parse the data into a array of character pointers.
  number_of_tokens = parse_gps_data(gps_data, gps_parsed_data);
#if defined(DEBUG)
  for (index=0; index < number_of_tokens; index++)
    {
        if (gps_parsed_data[index] == NULL)
        {
            break;
        }
        Serial.print("Gps data=");Serial.println(gps_parsed_data[index]);
        
    }
#endif
  // a little explantion here, gps_parsed data[2] is a pointer to a c string.
  // this string will always contain either an singe C string, "A" or "V".  If the string contains 
  // and A, then the gps data is valid. One could use a strcmp but pointer[0] is faster 
  if (gps_parsed_data[2][0] == 'A')
  { // indexes 3, 4, 5, and 6 have the gps data
    for (index=3; index<3+4; index++)
    {
      strcat(radiopacket, gps_parsed_data[index]);
      // do not put the comma after the last data
      if (index < 6)
        strcat(radiopacket,",");
    }
  }
  else
  {
    for (index=0; index<3; index++)
    {
      strcat(radiopacket, gps_parsed_data[index]);
    }
  }
  // Send a message to the DESTINATION!
  if (rf69_manager.sendtoWait((uint8_t *)radiopacket, strlen(radiopacket), DEST_ADDRESS)) {
    // Now wait for a reply from the server
    reply_buffer_len = sizeof(reply_buffer);
    uint8_t from;   
    if (rf69_manager.recvfromAckTimeout(reply_buffer, &reply_buffer_len, 2000, &from)) {
      reply_buffer[reply_buffer_len] = 0; // zero out remaining string
      
      //Serial.print("Got reply from #"); Serial.print(from);
      //Serial.print(" [RSSI :");
      //Serial.print(rf69.lastRssi());
      //Serial.print("] : ");
      //Serial.println((char*)reply_buffer);     
      Blink(LED, 100, 3); //blink LED 3 times, 40ms between blinks
    } else {
      Serial.println("No reply, is anyone listening?");
    }
  } else {
    Serial.println("Sending failed (no ack)");
  }
}

// the char *const says the pointer cannot be changed, but the data can
int parse_gps_data(char *const gps_raw_data, char **const array_pointers)
/**
 * @brief this function will tokenize a gps string 
 * 
 * The gps string is in the form of 
 * 012345678901234567890
 * $GPRMC,023936.000,A,1111.1234,N,12345.4321,W,0.54,243.41,180419,,,A*74
 *
 * The commans in the string will be set to 0, breaking the string in to multiple strings.
 * Each entry on char ** const array_pointer will point to the start of the data of each string.
 * The address of each string is just he starting address of the gps_raw_data + index of beginnig of 
 * each string
 * so array_pointer[0] = &$GPMRC      (address of gps_raw_data)
 *    array_pointer[1] = &023936.000  (address of gps_raw-data + 7)
 *    array_pointer[2] = &A           (address of gps_raw_data + 18)
 * 
 * This function will tokenize a NEMA sentence and place the start address in array pointers.
 * the commas in the string are replace by a zero, so the strings become null terminate.
 *     | Value Nema sentence | index in token array_pointers |
 *     |:-------------------:|:-----------------------------:|
 *     |Nema sentence name |0|
 *     |utc_time           |1|
 *     |active             |2|
 *     |lattitude          |3|
 *     |n/s indicator      |4|
 *     |logitude           |5|
 *     |e/w indicator      |6|
 *
 * @param char *const gps_raw_data, a pointer to a null terminated string which contains a NEMA output string
 *             the pointer is a const, whilst the data is not
 * @param char ** const array_pointer  an array of character pointers which will contain the address of the tokens
 * 
 * @return always 0
 */
{
    unsigned char array_pointer_index = 0;  // the sub index tracks the pointer into the array of pointers
    //char comma = ',';
    unsigned char index;
    uint16_t length_of_raw_data = strlen(gps_raw_data);
    array_pointers[array_pointer_index++] = gps_raw_data;
    for (index=0; index< length_of_raw_data; index++)
    {
        if (gps_raw_data[index] == comma) 
        {
            // if the current is a comma, and the next is anything not a comma, 
            // then put the address of the next character in the array_pointers and increment 
            // the index
            if (gps_raw_data[index + 1] != comma)
            {
                array_pointers[array_pointer_index++] = gps_raw_data + index + 1;
            }
            // replace the comma with a 0 zero so token is null terminate.
            // this will handle multile commans and make sure all commas are set to zero
            gps_raw_data[index] = 0;
            // make sure we dont exceed the array of pointer size.
            if (array_pointer_index >= ARRAY_SIZE)
            {
              break;  
            }
        }
    }
    // return the number of tokens that we have parsed, that is the array_pointer_index
    return array_pointer_index;
}
   

void Blink(byte PIN, byte DELAY_MS, byte loops) {
  for (byte i=0; i<loops; i++)  {
    digitalWrite(PIN,HIGH);
    delay(DELAY_MS);
    digitalWrite(PIN,LOW);
    delay(DELAY_MS);
  }
}
