/*
  Control Arduino Mega I/O as a PLC using commands over ethernet.
*/

#include <errno.h>
#include <SPI.h>
#include <Ethernet.h>
#include <TimeLib.h>

int ptr;

// ***** communications *****
int timeout = 15; // comms timeout in seconds
time_t t0;

// MAC address
byte mac[] = {0xA8, 0x61, 0x0A, 0xAE, 0x76, 0x05};

// define ethernet port
EthernetServer server = EthernetServer(30001);

// null IP address
IPAddress no_client(0, 0, 0, 0);

// init dummy client
EthernetClient client;

// termination character
char TERMCHAR = 0x0A;

// read buffer
const int buffer_length = 32;
char buf_arr[buffer_length] = "";
const int slice_len = 4;
char slice[slice_len];

// incoming byte
char m;

// msg components
long logic_number;
int logic_err = 0;

// return message
const int ret_length = 128;
char ret[ret_length] = "";
char ERR_TIMEOUT[ret_length] = "ERROR: read timeout";
char ERR_INVALID[ret_length] = "ERROR: invalid message";

// write buffer
int wbuf_len;
char wbuf[64];

// identity sring
char idn[60] = "arduino-ethernet-plc_5d2edc05-e223-4fd7-accd-8fe0870b12b1";

// ***** pin setup *****
const int number_of_inputs = 16;
const int number_of_outputs = 31;
int input_pins[number_of_inputs];
bool input_state[number_of_inputs];
int output_pins[number_of_outputs];
bool output_state[number_of_outputs];
int pin;

// input names
// 1 = AIRCHECKER_OFF
// 2 = ATMOSPHERE
// 3 = VACUUM
// 4 =
// 5 =
// 6 =
// 7 =
// 8 =
// 9 =
// 10 =
// 11 = ALM_TURBO50
// 12 = ALM_BACKP
// 13 = ALM_FLOW
// 14 = ALM_TEMP
// 15 = ALM_COMMS
// 16 = ALM_SOFTWARE

// output names
// 1 = SRCSHTR1
// 2 = SRCSHTR2
// 3 = SRCSHTR3
// 4 = SRCSHTR4
// 5 = SRCSHTR5
// 6 = SUBSHTR
// 7 = BACKVLV
// 8 = CHMBRVENTVLV
// 9 = QCMFLOW
// 10 = BACKP
// 11 = TURBOP
// 12 = TURBOVENTVLV
// 13 =
// 14 =
// 15 =
// 16 =
// 17 = PSUILOCK1
// 18 = PSUILOCK2
// 19 = PSUILOCK3
// 20 = PSUILOCK4
// 21 = PSUILOCK5
// 22 =
// 23 =
// 24 =
// 25 =
// 26 =
// 27 = PIDSWITCH1
// 28 = PIDSWITCH2
// 29 = PIDSWITCH3
// 30 = PIDSWITCH4
// 31 = PIDSWITCH5

// ***** logic setup *****
const int number_of_logic = 100;
bool logic_state[number_of_logic];
bool computer_control[number_of_logic];
int logic_statement;

void setup()
{
    Serial.begin(9600);
  
    // keep checking for DHCP address until its available
    while (Ethernet.begin(mac) == 0)
    {
        delay(1);
    }

    // start listening for clients
    server.begin();

    Serial.println("Connected!");
    
    // assign input pin numbers to list, 0-indexed
    // don't assign pins 0 and 1 as these are active when uploading a sketch
    // don't assign pins 4 and 10 as these are used to select function of the ethernet shield
    for (int i = 0; i < number_of_inputs; i++)
    {
        if ((i + 2 >= 4) && (i + 2 + 1 < 10))
        {
            input_pins[i] = i + 2 + 1;
        }
        else if (i + 2 + 1 >= 10)
        {
            input_pins[i] = i + 2 + 2;
        }
        else
        {
            input_pins[i] = i + 2;
        }
    }
    
    // assign output pin numbers to list, 0-indexed
    // don't assign pins 50, 51, and 52 are used by the SPI bus of the ethernet shield
    // pin 53 can only be assigned as an output when using the ethernet shield
    for (int i = 0; i < number_of_outputs; i++)
    {   
        pin = number_of_inputs + 2 + 2 + i;
        if (pin == 50)
        {
            output_pins[i] = 53;
            break; // can't go higher
        }
        else
        {
            output_pins[i] = pin;
        }
    }

    // set input pin modes
    for (int i = 0; i < number_of_inputs; i++)
    {
        pinMode(input_pins[i], INPUT);
    }
    
    // set output pin modes
    for (int i = 0; i < number_of_outputs; i++)
    {
        pinMode(output_pins[i], OUTPUT);
        digitalWrite(output_pins[i], HIGH); // set state to off
    }
}

void loop()
{
    // maintain an IP lease from the DHCP server
    Ethernet.maintain();

    if (!client.connected())
    {
        // no existing client so check for an incoming client connection with bytes available to read
        client = server.available();
    }
    
    // if the client connection has bytes to read, read them and take action
    if (client)
    {          
        // read incoming bytes into the buffer
        int i = 0;
        t0 = now();
        while (true)
        {
            // check error conditions on read
            if ((now() - t0) > timeout)
            {
                strcpy(ret, ERR_TIMEOUT);
                break;
            }
            else if (i > buffer_length - 1)
            {
                strcpy(ret, ERR_INVALID);
                break;
            }

            // read a byte into the buffer
            m = client.read();
            if (m == TERMCHAR)
            {
                // end of message reached
                break;
            }
            buf_arr[i] = m;
            i++;
        }

        // act on command
        if (strlen(ret) == 0)
        {         
            Serial.print("cmd: ");
            Serial.println(buf_arr);

            if (strcmp(buf_arr, "idn") == 0)
            {
                strcpy(ret, idn);
            }
            else if (strcmp(buf_arr, "get all input states") == 0)
            {
                for (int i = 0; i < number_of_inputs; i++)
                {
                    if (digitalRead(input_pins[i]))
                    {
                        ret[i] = 'T';
                    }
                    else
                    {
                        ret[i] = 'F';
                    }
                }
            }
            else if (strcmp(buf_arr, "get all output states") == 0)
            {
                for (int i = 0; i < number_of_outputs; i++)
                {
                    if (digitalRead(output_pins[i])) // active low
                    {
                        ret[i] = 'F';
                    }
                    else
                    {
                        ret[i] = 'T';
                    }
                }
            }
            else if (strcmp(buf_arr, "get all logic states") == 0)
            {
                for (int i = 0; i < number_of_logic; i++)
                {
                    if (logic_state[i])
                    {
                        ret[i] = 'T';
                    }
                    else
                    {
                        ret[i] = 'F';
                    }
                }
            }
            else if (strncmp(buf_arr, "set logic ", 10) == 0)
            {
                str_slice(buf_arr, slice, 10, strlen(buf_arr));
                // logic_number = atoi(slice);
                logic_number = parse_long(slice, logic_err);

                Serial.print("logic err: ");
                Serial.println(logic_err);
                Serial.println(slice);
                Serial.println(logic_number);
                
                if ((logic_number >= 1) && (logic_number <= number_of_logic))
                {
                    computer_control[logic_number - 1] = true;  
                }
                else
                {
                    strcpy(ret, ERR_INVALID);
                }

                reset_str_buffer(slice, slice_len);
            }
            else if (strncmp(buf_arr, "clear logic ", 12) == 0)
            {
                str_slice(buf_arr, slice, 12, strlen(buf_arr));
                // logic_number = atoi(slice);
                logic_number = parse_long(slice, logic_err);

                Serial.print("logic err: ");
                Serial.println(logic_err);
                Serial.println(slice);
                Serial.println(logic_number);

                if ((logic_number >= 1) && (logic_number <= number_of_logic))
                {
                    computer_control[logic_number - 1] = false;  
                }
                else
                {
                    strcpy(ret, ERR_INVALID);
                }
                
                reset_str_buffer(slice, slice_len);
            }
            else
            {
                strcpy(ret, ERR_INVALID);
                Serial.println("Invalid message");
            }
        }

        // send return message back to all clients
        server.write(ret);
        server.write(TERMCHAR);
        client.flush(); // wait until all bytes have been written

        Serial.print("ret: ");
        Serial.println(ret);

        // reset read buffer
        reset_str_buffer(buf_arr, buffer_length);

        // reset return message
        reset_str_buffer(ret, ret_length);
    }

    // ***** update logic statements, 1-indexed, outputs are active LOW, inputs are active HIGH *****
//    Serial.print("Logic state: ");
//    for (int i = 0; i < number_of_logic; i++)
//    {
//        if (logic_state[i])
//        {
//            Serial.print("T, ");
//        }
//        else
//        {
//            Serial.print("F, ");
//        }
//    }
//    Serial.println("");
//
//    Serial.print("Computer control: ");
//    for (int i = 0; i < number_of_logic; i++)
//    {
//        if (computer_control[i])
//        {
//            Serial.print("T, ");
//        }
//        else
//        {
//            Serial.print("F, ");
//        }
//    }
//    Serial.println("");
//
//    Serial.print("Input state: ");
//    for (int i = 0; i < number_of_inputs; i++)
//    {
//        if (digitalRead(input_pins[i]))
//        {
//            Serial.print("T, ");
//        }
//        else
//        {
//            Serial.print("F, ");
//        }
//    }
//    Serial.println("");
//
//    Serial.print("Output state: ");
//    for (int i = 0; i < number_of_outputs; i++)
//    {
//        if (digitalRead(output_pins[i]))
//        {
//            Serial.print("F, ");
//        }
//        else
//        {
//            Serial.print("T, ");
//        }
//    }
//    Serial.println("");


    // IF computer_control & NOT AIRCHECKER_OFF THEN SRCSHTR1 ON
    logic_statement = 1;
    if (computer_control[logic_statement - 1] && !digitalRead(input_pins[0]))
    {
        digitalWrite(output_pins[logic_statement - 1], LOW);
        logic_state[logic_statement - 1] = true;
    }
    else
    {
        digitalWrite(output_pins[logic_statement - 1], HIGH);
        logic_state[logic_statement - 1] = false;
    }

    // IF computer_control & NOT AIRCHECKER_OFF THEN SRCSHTR2 ON
    logic_statement = 2;
    if (computer_control[logic_statement - 1] && !digitalRead(input_pins[0]))
    {
        digitalWrite(output_pins[logic_statement - 1], LOW);
        logic_state[logic_statement - 1] = true;
    }
    else
    {
        digitalWrite(output_pins[logic_statement - 1], HIGH);
        logic_state[logic_statement - 1] = false;
    }

    // IF computer_control & NOT AIRCHECKER_OFF THEN SRCSHTR3 ON
    logic_statement = 3;
    if (computer_control[logic_statement - 1] && !digitalRead(input_pins[0]))
    {
        digitalWrite(output_pins[logic_statement - 1], LOW);
        logic_state[logic_statement - 1] = true;
    }
    else
    {
        digitalWrite(output_pins[logic_statement - 1], HIGH);
        logic_state[logic_statement - 1] = false;
    }

    // IF computer_control & NOT AIRCHECKER_OFF THEN SRCSHTR4 ON
    logic_statement = 4;
    if (computer_control[logic_statement - 1] && !digitalRead(input_pins[0]))
    {
        digitalWrite(output_pins[logic_statement - 1], LOW);
        logic_state[logic_statement - 1] = true;
    }
    else
    {
        digitalWrite(output_pins[logic_statement - 1], HIGH);
        logic_state[logic_statement - 1] = false;
    }

    // IF computer_control & NOT AIRCHECKER_OFF THEN SRCSHTR5 ON
    logic_statement = 5;
    if (computer_control[logic_statement - 1] && !digitalRead(input_pins[0]))
    {
        digitalWrite(output_pins[logic_statement - 1], LOW);
        logic_state[logic_statement - 1] = true;
    }
    else
    {
        digitalWrite(output_pins[logic_statement - 1], HIGH);
        logic_state[logic_statement - 1] = false;
    }

    // IF computer_control & NOT AIRCHECKER_OFF THEN SUBSHTR ON
    logic_statement = 6;
    if (computer_control[logic_statement - 1] && !digitalRead(input_pins[0]))
    {
        digitalWrite(output_pins[logic_statement - 1], LOW);
        logic_state[logic_statement - 1] = true;
    }
    else
    {
        digitalWrite(output_pins[logic_statement - 1], HIGH);
        logic_state[logic_statement - 1] = false;
    }

    // IF computer_control & NOT AIRCHECKER_OFF & BACKP THEN BACKVLV ON
    logic_statement = 7;
    if (computer_control[logic_statement - 1] && !digitalRead(input_pins[0]) && !digitalRead(output_pins[10 - 1]))
    {
        digitalWrite(output_pins[logic_statement - 1], LOW);
        logic_state[logic_statement - 1] = true;
    }
    else
    {
        digitalWrite(output_pins[logic_statement - 1], HIGH);
        logic_state[logic_statement - 1] = false;
    }

    // IF computer_control & NOT AIRCHECKER_OFF & NOT TURBOP & NOT ALM_TURBO50 THEN CHMBRVENTVLV ON
    logic_statement = 8;
    if (computer_control[logic_statement - 1] && !digitalRead(input_pins[0]) && digitalRead(output_pins[11 - 1]) && !digitalRead(input_pins[11 - 1]))
    {
        digitalWrite(output_pins[logic_statement - 1], LOW);
        logic_state[logic_statement - 1] = true;
    }
    else
    {
        digitalWrite(output_pins[logic_statement - 1], HIGH);
        logic_state[logic_statement - 1] = false;
    }

    // IF computer_control THEN QCMFLOW ON
    logic_statement = 9;
    if (computer_control[logic_statement - 1])
    {
        digitalWrite(output_pins[logic_statement - 1], LOW);
        logic_state[logic_statement - 1] = true;
    }
    else
    {
        digitalWrite(output_pins[logic_statement - 1], HIGH);
        logic_state[logic_statement - 1] = false;
    }

    // IF computer_control THEN BACKP ON
    logic_statement = 10;
    if (computer_control[logic_statement - 1])
    {
        digitalWrite(output_pins[logic_statement - 1], LOW);
        logic_state[logic_statement - 1] = true;
    }
    else
    {
        digitalWrite(output_pins[logic_statement - 1], HIGH);
        logic_state[logic_statement - 1] = false;
    }

    // IF computer_control & BACKVLV & NOT CHAMBERVENTVLV & NOT ALM_BACKP THEN TURBOP ON
    logic_statement = 11;
    if (computer_control[logic_statement - 1] && !digitalRead(output_pins[7 - 1]) && digitalRead(output_pins[8 - 1]) && !digitalRead(input_pins[12 - 1]))
    {
        digitalWrite(output_pins[logic_statement - 1], LOW);
        logic_state[logic_statement - 1] = true;
    }
    else
    {
        digitalWrite(output_pins[logic_statement - 1], HIGH);
        logic_state[logic_statement - 1] = false;
    }

    // IF computer_control & NOT TUBROP THEN TURBOVENTVLV ON
    logic_statement = 12;
    if (computer_control[logic_statement - 1] && digitalRead(output_pins[11 - 1]))
    {
        digitalWrite(output_pins[logic_statement - 1], LOW);
        logic_state[logic_statement - 1] = true;
    }
    else
    {
        digitalWrite(output_pins[logic_statement - 1], HIGH);
        logic_state[logic_statement - 1] = false;
    }


    // reserved for future use, set off
    logic_statement = 13;
    digitalWrite(output_pins[logic_statement - 1], HIGH);
    logic_state[logic_statement - 1] = false;
    
    logic_statement = 14;
    digitalWrite(output_pins[logic_statement - 1], HIGH);
    logic_state[logic_statement - 1] = false;
    
    logic_statement = 15;
    digitalWrite(output_pins[logic_statement - 1], HIGH);
    logic_state[logic_statement - 1] = false;
    
    logic_statement = 16;
    digitalWrite(output_pins[logic_statement - 1], HIGH);
    logic_state[logic_statement - 1] = false;


    // IF computer_control & NOT AIRCHECKER_OFF & NOT statement 33 THEN PSUILOCK1
    logic_statement = 17;
    if (computer_control[logic_statement - 1] && !digitalRead(input_pins[0]) && !logic_state[33 - 1])
    {
        digitalWrite(output_pins[logic_statement - 1], LOW);
        logic_state[logic_statement - 1] = true;
    }
    else
    {
        digitalWrite(output_pins[logic_statement - 1], HIGH);
        logic_state[logic_statement - 1] = false;
    }

    // IF computer_control & NOT AIRCHECKER_OFF & NOT statement 33 THEN PSUILOCK2
    logic_statement = 18;
    if (computer_control[logic_statement - 1] && !digitalRead(input_pins[0]) && !logic_state[33 - 1])
    {
        digitalWrite(output_pins[logic_statement - 1], LOW);
        logic_state[logic_statement - 1] = true;
    }
    else
    {
        digitalWrite(output_pins[logic_statement - 1], HIGH);
        logic_state[logic_statement - 1] = false;
    }

    // IF computer_control & NOT AIRCHECKER_OFF & NOT statement 33 THEN PSUILOCK3
    logic_statement = 19;
    if (computer_control[logic_statement - 1] && !digitalRead(input_pins[0]) && !logic_state[33 - 1])
    {
        digitalWrite(output_pins[logic_statement - 1], LOW);
        logic_state[logic_statement - 1] = true;
    }
    else
    {
        digitalWrite(output_pins[logic_statement - 1], HIGH);
        logic_state[logic_statement - 1] = false;
    }

    // IF computer_control & NOT AIRCHECKER_OFF & NOT statement 33 THEN PSUILOCK4
    logic_statement = 20;
    if (computer_control[logic_statement - 1] && !digitalRead(input_pins[0]) && !logic_state[33 - 1])
    {
        digitalWrite(output_pins[logic_statement - 1], LOW);
        logic_state[logic_statement - 1] = true;
    }
    else
    {
        digitalWrite(output_pins[logic_statement - 1], HIGH);
        logic_state[logic_statement - 1] = false;
    }

    // IF computer_control & NOT AIRCHECKER_OFF & NOT statement 33 THEN PSUILOCK5
    logic_statement = 21;
    if (computer_control[logic_statement - 1] && !digitalRead(input_pins[0]) && !logic_state[33 - 1])
    {
        digitalWrite(output_pins[logic_statement - 1], LOW);
        logic_state[logic_statement - 1] = true;
    }
    else
    {
        digitalWrite(output_pins[logic_statement - 1], HIGH);
        logic_state[logic_statement - 1] = false;
    }


    // reserved for future use, set off
    logic_statement = 22;
    digitalWrite(output_pins[logic_statement - 1], HIGH);
    logic_state[logic_statement - 1] = false;
    
    logic_statement = 23;
    digitalWrite(output_pins[logic_statement - 1], HIGH);
    logic_state[logic_statement - 1] = false;
    
    logic_statement = 24;
    digitalWrite(output_pins[logic_statement - 1], HIGH);
    logic_state[logic_statement - 1] = false;
    
    logic_statement = 25;
    digitalWrite(output_pins[logic_statement - 1], HIGH);
    logic_state[logic_statement - 1] = false;
    
    logic_statement = 26;
    digitalWrite(output_pins[logic_statement - 1], HIGH);
    logic_state[logic_statement - 1] = false;
  

    // IF computer_control THEN PIDSWITCH1
    logic_statement = 27;
    if (computer_control[logic_statement - 1])
    {
        digitalWrite(output_pins[logic_statement - 1], LOW);
        logic_state[logic_statement - 1] = true;
    }
    else
    {
        digitalWrite(output_pins[logic_statement - 1], HIGH);
        logic_state[logic_statement - 1] = false;
    }

    // IF computer_control THEN PIDSWITCH2
    logic_statement = 28;
    if (computer_control[logic_statement - 1])
    {
        digitalWrite(output_pins[logic_statement - 1], LOW);
        logic_state[logic_statement - 1] = true;
    }
    else
    {
        digitalWrite(output_pins[logic_statement - 1], HIGH);
        logic_state[logic_statement - 1] = false;
    }

    // IF computer_control THEN PIDSWITCH3
    logic_statement = 29;
    if (computer_control[logic_statement - 1])
    {
        digitalWrite(output_pins[logic_statement - 1], LOW);
        logic_state[logic_statement - 1] = true;
    }
    else
    {
        digitalWrite(output_pins[logic_statement - 1], HIGH);
        logic_state[logic_statement - 1] = false;
    }

    // IF computer_control THEN PIDSWITCH4
    logic_statement = 30;
    if (computer_control[logic_statement - 1])
    {
        digitalWrite(output_pins[logic_statement - 1], LOW);
        logic_state[logic_statement - 1] = true;
    }
    else
    {
        digitalWrite(output_pins[logic_statement - 1], HIGH);
        logic_state[logic_statement - 1] = false;
    }

    // IF computer_control THEN PIDSWITCH5
    logic_statement = 31;
    if (computer_control[logic_statement - 1])
    {
        digitalWrite(output_pins[logic_statement - 1], LOW);
        logic_state[logic_statement - 1] = true;
    }
    else
    {
        digitalWrite(output_pins[logic_statement - 1], HIGH);
        logic_state[logic_statement - 1] = false;
    }


    // reserved for future use, set off
    logic_statement = 32;
    digitalWrite(output_pins[logic_statement - 1], HIGH);
    logic_state[logic_statement - 1] = false;


    // IF ALM_COMMS | ALM_SOFTWARE | ALM_TEMP | ALM_FLOW
    logic_statement = 33;
    if (digitalRead(input_pins[14]) || digitalRead(input_pins[15]) || digitalRead(input_pins[13]) || digitalRead(input_pins[12]))
    {
        logic_state[logic_statement - 1] = true;
    }
    else
    {
        logic_state[logic_statement - 1] = false;
    }
}

void str_slice(char input[], char output[], size_t i_start, size_t i_end)
{    
    int j = 0;
    for (int i = i_start; i < i_end; i++)
    {
        output[j] = input[i];
        j++;
    }
}

void reset_str_buffer(char buf[], size_t buffer_len)
{
    for (int i = 0; i < buffer_len; i++)
    {
        buf[i] = (char)0;
    }
}

// adapted from https://stackoverflow.com/a/14176593
long parse_long(const char *str, int err)
{
    errno = 0;
    char *temp;
    long value = strtol(str, &temp, 10);

    long LONG_MIN = -2147483648;
    long LONG_MAX = 2147483647;
    
    if (temp == str || *temp != '\0' || ((value == LONG_MIN || value == LONG_MAX) && errno == ERANGE))
    {
        err = 1;
    }
    else
    {
        err = 0;
    }
        
    return value;
}
