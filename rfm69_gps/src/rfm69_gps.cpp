/** \copyright Copyright 2023 by Ralph Blach under the gpl3 public license. see https://www.gnu.org/licenses/gpl-3.0.en.html#license-text
for the entire text*/
/**
 *  
 *  @file  rfm69_gps.cpp
    @author Ralph Blach
    @date April 15, 2019
    @brief This program uses a Adafruit(R) feather wings with an rfm69 radio a GPS bonnet 
       with a CDtop PA161D technology gps receiver.  the specificatin is at https://www.cdtop-tech.com/products/pa1616d.  
       "The CDTop CD-PA1616D module utilizes the MediaTek new generation GNSS Chipset MT3333" quoted from the specification.
       The command spec for the MT3333 is at https://microchip.ua/simcom/GNSS/Application%20Notes/MT3333%20Platform%20NMEA%20Message%20Specification%20V1.07.pdf
       

    @note the i2c has the following setup bytes 0 through 5 have the call sign. if the call sign is shorter put spaces
          bytes 6 and 7 have the network sync words.
      byte offset 0  1  2  3  4    5    6    7
                  g  x  8  a  b    c    0xaa 0xbb
**/
#include <EEPROM.h>
#include <SPI.h>
#include <RH_RF69.h>
#include <RHReliableDatagram.h>
#define DEBUG 1
#ifdef DEBUG
  #define DEBUG_WRITE(x)     Serial.write(x)
  #define DEBUG_PRINT(x)     Serial.print (x)
  #define DEBUG_PRINTDEC(x)     Serial.print (x, DEC)
  #define DEBUG_PRINTHEX(x)     Serial.print (x, HEX)
  #define DEBUG_PRINTLN(x)  Serial.println (x)
#else
  #define DEBUG_WRITE(x) 
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTDEC(x)
  #define DEBUG_PRINTHEX(x)
  #define DEBUG_PRINTLN(x)
#endif 

// function headers
int parse_gps_data(char *const, char **const);
void write_gps(const char *data, const u32 retrys);
u8 calculate_checksum(const char *, u32);
u8 bin_to_hex(u8 value);
void blink(byte PIN, byte DELAY_MS, byte loops) ;
#define RMC_HEADER 0
#define RMC_TIME 1
#define RMC_STATUS 2
#define RMC_LATITUDE 3
#define RMC_E_W_INDICATOR 4
#define RMC_LONGITUDE 5
#define RMC_N_S_INDICATOR 6
#define RMC_SPEED_OVER_GROUND 7
#define RMC_COURSE_OVER_GROUND 8
#define RMC_DATE 9

/**
    @brief  create a name for the gpsSerial port
    @param GPSSerial
*/
#define GPSSerial Serial1
/**
    @brief this is the size of the data buffer where the data from the MK3339 will be placed.
    @param GPS_RECEIVER_BUFFER_SIZE
*/
#define GPS_RECEIVER_BUFFER_SIZE 100
/**
    @brief this is the number of array entries for the tokenizer.  you can have 15 tokens
    @param ARRAY_SIZE
*/
#define ARRAY_SIZE 15

/************ Radio Setup ***************/
/**
    @brief radio frequency
    @param RF69_FREQ
*/
// Change to 434.0 or other frequency, must match RX's freq!
#define RF69_FREQ 433.0

// Where to send packets to!
/**
    @brief destination address
    @param DEST_ADDRESS
*/
#define DEST_ADDRESS   0x01
// change addresses for each client board, any number :)
#define MY_ADDRESS     0x02


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

// Singleton instance of the radio driver
RH_RF69 rf69(RFM69_CS, RFM69_INT); /*!< Singleton instance of the radio driver */

// Class to manage message delivery and receipt, using the driver declared above
RHReliableDatagram rf69_manager(rf69, MY_ADDRESS);  /*!< Class to manage message delivery and receipt, using the driver declared above */

// Dont put these on the stack, they only need to be allocated once.
uint8_t reply_buffer[RH_RF69_MAX_MESSAGE_LEN]; /*!< the reply buffer */
const char comma = ',';
char *gps_parsed_data[ARRAY_SIZE]; /*!< array where the parsed gps to be transmitted is placed  */


// this is my ham call sign, and this is necessary for 433 mhz, in the US.  If you are not a ham radio operator
// in the United States, you cannot use the 433 Mhz radio and you cannot use my call sign.  If you are outside the US,
// you need to know the rules in your country.
// the following data structures do not need to be reallocated every time
//                                           0123456

char radiopacket[RH_RF69_MAX_MESSAGE_LEN] = "xxxxxx,";  /*!< packet to be transmitted */

char gps_data[GPS_RECEIVER_BUFFER_SIZE]; /*!< buffer to contain the received packet */
/**@def gps_data
Where the ack packet will go
*/

void rfm_69_setup()
{
    /**
        @brief This is the setup program for the Arduino

        This program sets only
        - Sets up the debug serial port 115200
        - Sets up the GPS serial port to 9600 baud
        - output GMRC packets
        - only output once every 10 seconds

        @return Nothing
    */

    // Supported NMEA Sentences:
    // 0 NMEA_SEN_GLL, // GPGLL interval - Geographic Position - Latitude longitude
    // 1 NMEA_SEN_RMC, // GPRMC interval - Recomended Minimum Specific GNSS Sentence
    // 2 NMEA_SEN_VTG, // GPVTG interval - Course Over Ground and Ground Speed
    // 3 NMEA_SEN_GGA, // GPGGA interval - GPS Fix Data
    // 4 NMEA_SEN_GSA, // GPGSA interval - GNSS DOPS and Active Satellites
    // 5 NMEA_SEN_GSV, // GPGSV interval - GNSS Satellites in View
    // 6 NMEA_SEN_GRS, //GPGRS interval – GNSS Range Residuals
    // 7 NMEA_SEN_GST, //GPGST interval – GNSS Pseudorange Errors Statistics
    // 17 NMEA_SEN_ZDA, // GPZDA interval – Time & Date
    // 18 NMEA_SEN_MCHN, //PMTKCHN interval – GNSS channel status
    // 19 NMEA_SEN_DTM, //GPDTM interval – Datum reference
    // Supported Frequency Setting
    // 0 - Disabled or not supported sentence
    // 1 - Output once every one position fix
    // 2 - Output once every two position fixes
    // 3 - Output once every three position fixes
    // 4 - Output once every four position fixes
    // 5 - Output once every five position fixes 
    
    // const char gps_init_data[] = "$PMTK314,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*29\r\n";
    // set the oupout to be RMC
    //                                     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8
    const char gps_init_data[] = "$PMTK314,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*29\r\n";
    // set the the gps to only transmit once every 10 seconds.
    const char gps_update_rate[] = "$PMTK220,10000*2F\r\n";
    // the sync words for the radio the default are 0x2d and 0xd4
    u8 syncwords []= {0x2d, 0xd4};
    //uint8_t syncwords[2] ;
    //int sync_word_index = 0;
    // 9600 baud is the default rate for the Ultimate GPS
    GPSSerial.begin(9600);
    int index = 0;
    //char written;
    u8 checksum;
    
#if defined (DEBUG)
    Serial.begin(115200);
    while (!Serial) {
        delay(1);    // wait until serial console is open, remove if not tethered to computer
    }
#endif
    //read the call sign from the eeprom. bytes 0 through 5
    while (index < 6 )
    {
      radiopacket[index] = EEPROM.read(index);
      DEBUG_PRINT("read ");DEBUG_PRINT(radiopacket[index]);DEBUG_PRINT("from addr=");DEBUG_PRINT(index);DEBUG_PRINT("\n");
      index++;
    }
    radiopacket[index++]=',';
    radiopacket[index++]=0;
    // set the index to location 6 to read the sync words bytes 6 and 7
    index = 6; 
    syncwords[0] =  EEPROM.read(index);
    DEBUG_PRINT("read 0x");DEBUG_PRINTHEX(syncwords[0]);DEBUG_PRINT(" from addr=");DEBUG_PRINT(index);DEBUG_PRINT("\n");
    index = 7;
    syncwords[1] =  EEPROM.read(index);
    DEBUG_PRINT("read 0x");DEBUG_PRINTHEX(syncwords[1]);DEBUG_PRINT(" from addr=");DEBUG_PRINT(index);DEBUG_PRINT("\n");
    
    // calculate the checksome of the gps data
    checksum = calculate_checksum(gps_init_data, strlen(gps_init_data));
    DEBUG_PRINT("checksum = "); DEBUG_PRINTHEX(checksum); DEBUG_PRINT("\n");
    // only send GPRMC  packets
    write_gps(gps_init_data, 3);
    
    // initial the GPS radio to only send updates once every 10 seconds
    write_gps(gps_update_rate, 3);
    
    pinMode(LED, OUTPUT);
    pinMode(RFM69_RST, OUTPUT);
    digitalWrite(RFM69_RST, LOW);

    DEBUG_PRINTLN("Feather Addressed RFM69 TX Test!");
    DEBUG_PRINTLN();

    // manual reset the radio
    digitalWrite(RFM69_RST, HIGH);
    delay(10);
    digitalWrite(RFM69_RST, LOW);
    delay(10);

    if (!rf69_manager.init()) {
        DEBUG_PRINTLN("RFM69 radio init failed");
        while (1);
    }
    DEBUG_PRINTLN("RFM69 radio init OK!");
    // Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM (for low power module)
    // No encryption
    if (!rf69.setFrequency(RF69_FREQ)) {
        DEBUG_PRINTLN("setFrequency failed");
    }

    // If you are using a high power RF69 eg RFM69HW, you *must* set a Tx power with the
    // ishighpowermodule flag set like this:
    rf69.setTxPower(20, true);    // range from 14-20 for power, 2nd arg must be true for 69HCW
    rf69.setSyncWords(syncwords, 2);  //set the network,  This must match for all board and this is the default

    pinMode(LED, OUTPUT);

    DEBUG_PRINT("RFM69 radio @");  DEBUG_PRINT((int)RF69_FREQ);  DEBUG_PRINTLN(" MHz");
    
    
}


void rfm_69_loop() {
    /**
        @brief This is the loop subrourine

        This program receives the data from the gps receiver,parses it, and sends it to Node 2 on the
        network.  Because this is running on the Ham band, it does not encrpt.

        @return Nothing
    */
    char gps_char;
    uint8_t number_of_tokens;
    uint16_t index;
    uint16_t gps_char_index = 0;
    uint8_t reply_buffer_len = 0 ; //this might be modified by the transmit program
    // terminate the radio packet with a 0,  after it has been used once it will have data after the #
    // this saves the slow auto intilaizaton of the packet ever time
    radiopacket[7] = 0;
    // set the gps data to zero befor receive.
    memset(gps_data, 0, sizeof(gps_data));
    reply_buffer_len = sizeof(reply_buffer);

    while (1)
    {
        // Read the serial data from the serial port attached to the software serial port.
         
        if (GPSSerial.available())
        {
            // DEBUG_PRINT("start gps read");
            gps_char = GPSSerial.read();
            // if it is a carriage return character continue
           
            if (gps_char == 13)
            {
                // DEBUG_PRINTLN("carriage returnx");
                continue;
            }

            // if it is a line feed, stop we have the packet
            if (gps_char == 10)
            {
                // DEBUG_PRINTLN("line feed");DEBUG_PRINTLN(gps_data);
                // this is a GxRMC packet.  Not 0 ascii 0 in a RMC packet means no or invalid fix
                if (strncmp(gps_data+3, "RMC", 3) != 0)
                {
                    gps_char_index = 0;  // reset the index to 0 because we did not get the expected packet
                    DEBUG_PRINTLN("Got a continue");
                    continue;
                }
                else
                    DEBUG_PRINTLN("Got a break");
                    break;
            }
            // make sure we never have a data overun.  Buffer overflows are nasty.
            if (gps_char_index < GPS_RECEIVER_BUFFER_SIZE - 3)
                gps_data[gps_char_index++] = gps_char;
            else
                gps_data[gps_char_index] = gps_char;
        }
    }
    gps_data[gps_char_index++] = 0; //make the gps data null terminated
    DEBUG_PRINT("Nema sentence = ");DEBUG_PRINTLN(gps_data);
    // now parse the data into a array of character pointers.
    number_of_tokens = parse_gps_data(gps_data, gps_parsed_data);
#if defined(DEBUG)
    for (index = 0; index < number_of_tokens; index++)
    {
        if (gps_parsed_data[index] == NULL)
        {
            break;
        }
        DEBUG_PRINT("Gps data="); DEBUG_PRINTLN(gps_parsed_data[index]);

    }
#endif
    // a little explantion here, gps_parsed data[2] is a pointer to a c string.
    // this string will always contain either an singe C string, "A" or "V".  If the string contains
    // and A, then the gps data is valid. One could use a strcmp but pointer[0] is faster
    //   0     1         2   3       4    5       6   7    8      9
    // $GPRMC,094330.000,A,3113.3156,N,12121.2686,E,0.51,193.93,171210,,,A*68<CR><LF>
    if (gps_parsed_data[RMC_STATUS][0] == 'A')
    {   
      // indexes 1, 2,3,4,5,6 have the gps data
        for (index = RMC_TIME; index < RMC_SPEED_OVER_GROUND; index++)
        {
            strcat(radiopacket, gps_parsed_data[index]);
            // do not put the comma after the last data
            strcat(radiopacket,",");
            
        }
        strcat(radiopacket, gps_parsed_data[RMC_DATE]);
    }
    else
    { 
      // put on a message that the data is bad
      strcat(radiopacket,"V,");
        for (index = 0; index < 3; index++)
        {
            strcat(radiopacket, gps_parsed_data[index]);
        }
    }
    // Send a message to the DESTINATION!
    DEBUG_PRINT("radio packet="); DEBUG_PRINTLN(radiopacket);

    if (rf69_manager.sendtoWait((uint8_t *)radiopacket, strlen(radiopacket), DEST_ADDRESS)) {
        // Now wait for a reply from the server
        reply_buffer_len = sizeof(reply_buffer);
        
    } else {
        DEBUG_PRINTLN("Sending failed (no ack)");
        blink(LED, 499, 1);
    }
}

// the char *const says the pointer cannot be changed, but the data can
int parse_gps_data(char *const gps_raw_data, char **const array_pointers)
/**
    @brief this function will tokenize a gps string

    The gps string is in the form of\n
    index =  012345678901234567890\n
    Nemas    =  $GPRMC,023936.000,A,1111.1234,N,12345.4321,W,0.54,243.41,180419,,,A*74

    The commans in the string will be set to 0, breaking the string in to multiple strings.
    Each entry on char ** const array_pointer will point to the start of the data of each string.
    The address of each string is just he starting address of the gps_raw_data + index of beginnig of
    each string\n
    so array_pointer[0] = &$GPMRC      (address of gps_raw_data)\n
      array_pointer[1] = &023936.000  (address of gps_raw-data + 7)\n
      array_pointer[2] = &A           (address of gps_raw_data + 18)\n

    This function will tokenize a NEMA sentence and place the start address in array pointers.
    the commas in the string are replace by a zero, so the strings become null terminate.
       | Value Nema sentence | index in token array_pointers |
       |:-------------------:|:-----------------------------:|
       |Nema sentence name      |0| $GxRMC, depends on the the type of fix. P=gps only,  N=gps add glosnoss
       |utc_time                |1|
       |lattitude               |2|
       |status                  |3|
       |n/s indicator           |4|
       |logitude                |5|
       |e/w indicator           |6|
       |speed over ground       |7|
       |course over ground      |8|
       |date                    |9|

       Status
       | Value | Description |
       |:------:|:---------------------------------------------:|
       | V |  fix not valid | 
       | A | fix valid |
    @param gps_raw_data a pointer to a null terminated string which contains a NEMA output string
               the pointer is a const, whilst the data is not
    @param array_pointers  an array of character pointers which will contain the address of the tokens

    @return always 0
*/
{
    unsigned char array_pointer_index = 0;  // the sub index tracks the pointer into the array of pointers
    //char comma = ',';
    unsigned char index;
    u16 length_of_raw_data = strlen(gps_raw_data);
    array_pointers[array_pointer_index++] = gps_raw_data;
    for (index = 0; index < length_of_raw_data; index++)
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
void write_gps(const char *data, const u32 retrys)
/**@brief this writes a data the gps serial port
 * 
 * @param data a pointer to the data to be written to the serial port
 * @param retrys the number of times to retry the command 
 * 
 * I would send the command atleast 4 times to make sure the gps module get the command
 * 
 */
{
  u32 index;
  u32 retry_counter;
  for (retry_counter = 0; retry_counter < retrys; retry_counter++)
    {
        DEBUG_PRINTLN("Sending init data to gps");
        DEBUG_PRINTLN(data);
        for (index = 0; index < strlen(data); index++)
        {
            GPSSerial.write(data[index]);
        }
    }
}
u8 calculate_checksum(const char * sentence, u32 length)
{
    /*this subroutine calculates the checksum of the command 

    * @param sentence a character pointer to the list of characters
            pass in the entire gps string, it will skip over the $ and subtract outh the *cc\r\n
      @param length the length of the charaters
      @return a short with the ascii encoded value in it
    */
   u8 checksum = 0;
   u32 index;
   u32 real_length = length - 5;

   for (index=1; index < real_length; index++)
   {
        checksum = checksum ^ sentence[index];
   }
   return checksum;
}
u8 bin_to_hex(u8 value)
{
    /* return the ascii character value of a bin value
    @param value
    @return the ascii value 0x30-39 for 0-9 and A-f for abcdef
    */
    if ( value <= 9 )
        return 0x30 + value;
    else
        return 'A' + (value -10);

}
void blink(byte PIN, byte DELAY_MS, byte loops) 
/**@brief this function will tokenize a gps string
 * 
 * this program will blink the led on the Arduino
 * @param PIN  the Pin on the Arduino
 * @param DELAY_MS The delay in Milliseconds
 * @param loops the number of times to blink
 * @return None
 */
{
    for (byte i = 0; i < loops; i++)  {
        digitalWrite(PIN, HIGH);
        delay(DELAY_MS);
        digitalWrite(PIN, LOW);
        delay(DELAY_MS);
    }
}
