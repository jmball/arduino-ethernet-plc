/* 
  Control Arduino Mega I/O as a PLC using commands over ethernet.
*/

#include <SPI.h>
#include <Ethernet.h>
#include <TimeLib.h>

// MAC address
byte mac[] = {0xA8, 0x61, 0x0A, 0xAE, 0x76, 0x05};

// read buffer
char buf_arr[9];

// incoming byte
char m;

// termination character
char TERMCHAR = 0x0A;

// msg separator
char sep[2] = ",";

// index of separtor in msg
int sep_ix;

// msg components
int pin; // pin number
String state_str; // state as a string ("off" or "on")
int state; // state as an int (0 or 1)

// pin setup
int pin_min = 23;
int pin_max = 53;

// comms timeout in seconds
int timeout = 15;
time_t t0;

// return message
String ret = String("");

// identity sring
char idn[60] = "arduino-ethernet-plc_5d2edc05-e223-4fd7-accd-8fe0870b12b1";

// define ethernet port
EthernetServer server = EthernetServer(30001);

// null IP address
IPAddress no_client(0, 0, 0, 0);


void setup() {
  // keep checking for DHCP address until its available
  while (Ethernet.begin(mac) == 0) {
    delay(1);
  }

  // start listening for clients
  server.begin();

  // setup relay pins
  for (int i = pin_min; i <= pin_max; i++) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH); // set state to off
  }
}


void loop() {
  // maintain an IP lease from the DHCP server
  Ethernet.maintain();

  // check for an incoming client connection with bytes available to read
  EthernetClient client = server.available();
  
  // if the client connection has bytes to read, read them and take action 
  if (client) {
    // read incoming bytes into the buffer
    int i = 0;
    t0 = now();
    while (true) {
      // check error conditions on read
      if ((now() - t0) > timeout) {
        ret = "ERROR: read timeout";
        break;
      }
      else if (i > 8) {
        ret = "ERROR: invalid message";
        break;
      }

      // read a byte into the buffer
      m = client.read();
      if (m == TERMCHAR){
        // end of message reached
        break;  
      }
      buf_arr[i] = m;
      i++;
    }

    // act on command
    if (ret == "") {
      // convert buffer to string
      String cmd = String(buf_arr);

      if (cmd == "idn") {
        ret = idn;
      }
      else {
        // find pin state separator in cmd
        sep_ix = cmd.indexOf(sep);
  
        // check separator position is valid
        if (sep_ix <= 0) {
          ret = "ERROR: invalid message";
        }
        else {
          // decompose pin and state
          pin = cmd.substring(0, sep_ix).toInt();
          state_str = cmd.substring(sep_ix + 1);
    
          // check pin is valid pin on Arduino Mega
          if (pin < 23 || pin > 53) {
            ret = "ERROR: invalid pin number";
          }
          else {
            // check state is valid
            if (state_str == "on") {
              state = 0;
            }
            else if (state_str == "off") {
              state = 1;
            }
            else {
              ret = "ERROR: invalid pin state";
            }
          }
          
          // set relay
          if (ret == "") {
            digitalWrite(pin, state);
          }
        }
      }
    }

    // create write buffer (server.write needs char array not String) from 
    // return message and send back to all clients
    int wbuf_len = ret.length() + 1; 
    char wbuf[wbuf_len];
    ret.toCharArray(wbuf, wbuf_len);
    server.write(wbuf);
    server.write(TERMCHAR);

    // close the client connection
    client.stop();

    // reset read buffer
    for (int i = 0; i < 9; i++) {
      buf_arr[i] = '\0';
    }

    // reset return msg
    ret = "";
  }
}
