/* @Title: Turning Tracker WiFi Module Code
 * @Author Robert J Paul
 * @Date 04/03/2017
 */

#include <Boards.h>
#include <Firmata.h>
#include "Timer.h"

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h> // Knows how to handle GET and POST
#include <ESP8266mDNS.h>

//;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;//
//;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;HTML CODE;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;//
//;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;//
/*HTML code for the form to enter the user's WiFi credentials for setup*/
String AP_wifiSetupPage =
  "<!DOCTYPE HTML>\r\n<html>\r\n\
<h3>WiFi Setup</h3>\
<form action='http://192.168.4.1/login' method='POST'>\
    SSID:<br>\
    <input type='text' name='ssid'><br>\
    Password:<br>\
    <input type='password' name='password'><br><br>\
    <input type='submit' value='Connect'>\
</form><br>\
<form action='http://192.168.4.1/' method='GET'>\
    <input type='submit' value='Refresh'>\
</form>\
</html>\n";

/*HTML code for the page when a conection to WiFi has failed*/
String AP_errorPage =
  "<!DOCTYPE HTML>\r\n<html>\r\n\
<h2>Error Connecting to Network</h2>\
<h3>Please Wait 15 Seconds And Try Again</h3>\
<form action='http://192.168.4.1/' method='GET'>\
    <input type='submit' value='Refresh'>\
</form>\
</html>\n";

/*Half of the HTML code (minus the current IP Address) for when a user has successfully connected the device to WiFi*/
String AP_successPage =
  "<!DOCTYPE HTML>\r\n<html>\r\n\
<h2>Successfully Connected to WiFi</h2>\
<h3>Please go to ";

/*The begginning of the HTML code that the user sees to alert them on the state of their patient(s)*/
String STA_infoPage = 
  "<!DOCTYPE HTML>\r\n<html>\r\n\
<h3>TTID: 1D749            ROOM NUMBER: 105</h3>\
<p>STATUS: </p>\
<h1>";

//;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;//
//;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;//
//;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;//

/*Declare constants*/
const int SUCCESS_PAGE_TIME = 20000; //In Milliseconds, how long the user is alerted that WiFi has been successfully connected before switching to an STA
const int REFRESH_RATE = 500; //How often we refresh our server
const int ERROR_PAGE_DELAY = 4000; //How long the user is told of an error while setting up AP before rerouted back to the setup page
const int FLIPPED_UP_TIME = 10000; //How long the user is alerted on the server that a flip took place before going back to a neutral state
/*Timers used for for refreshing the current page as a AP and an STA*/
Timer t; int refreshID; int toNeutralID;
/*Set the main page*/
String currentPage = AP_wifiSetupPage;
/*WiFi initially disconnected*/
bool WIFI_CONNECTED = false;
/*Helps us to not create mutiple jobs upton setting up the AP*/
bool handlingLogin = false; 
/*Our Server that behaves as an access point and a station*/
ESP8266WebServer server(80);
/*Our server can only act as a access point or a station - true for AP, false for STA*/
bool AP_ACTIVE = true; 
/*The most recent packet received from the Microcontroller*/
int currPacket;
/*The current type of message we want to send to the user - 0: FLIPPED, 1: NEEDS_FLIP, else: NONE*/
int currTYPE = 2;

//WiFiClient client;

void setupAP(void);
void refresh(void);
void handleLogin(void);
void WiFiConnected(void);
void handlePackets(void);
void toNeutral(void);
String translateTYPE(int t);



/*Sets up the access point(AP) for the user to input WiFi credentials and sets up the refresh timer*/
void setup()
{
  Serial.begin(115200);
  refreshID = t.every(REFRESH_RATE, refresh); // Every REFRESH_RATE milliseconds, refresh the current page of the server
  setupAP(); //Follow the user through the setup process
}


void loop()
{
  server.handleClient(); //Necessary for handling POSTs and GETs
  t.update(); //Necessary for our timer
  handlePackets(); //Waits for incoming packets from the MC and modifies/sends back the packet based on the state of the module
}



/*Sets up the access point which the user uses to sepcify the global WiFi network credentials*/
void setupAP(void) {
  WIFI_CONNECTED = false; //Clearly, If we're running setupAP(), we're not connect to WiFi - this is useful to refresh()'s use of setupAP()
  
  WiFi.mode(WIFI_AP); //Changes to WIFI_STA when WiFi Connected
  AP_ACTIVE = true; //We're now in AP-mode
  
  //Serial.print("Setting Tracker as Access Point ... ");
  WiFi.softAP("Turning Tracker 1D749");
  
  currentPage = AP_wifiSetupPage;

  /*Set the default page the login page*/
  server.on("/", []() {
    t.after(ERROR_PAGE_DELAY,foo); //If there's en error, let user see error message for ~4 seconds before trying again
  });

  /*Deduce user WiFi credentials and attempt to connect to the internet upon forwarding to the login page*/
  server.on("/login", []() {
    if (!handlingLogin) {
      currentPage = AP_wifiSetupPage;
      handleLogin();
    }
  });

  server.begin();

  //Serial.println("STA Address: " + WiFi.macAddress());
  //Serial.println("AP Address: " + WiFi.softAPmacAddress());
}

void foo(void){currentPage = AP_wifiSetupPage;}

/*Continually refreshes the server, given the current state of the program,  every REFRESH_RATE milliseconds*/
void refresh(void) {//TODO: Maybe make this less frequent to conserve power?
  /*If we've finished the STA-setup process, stop refreshing the AP*/
  if(!WIFI_CONNECTED)
    server.send(200, "text/html", currentPage);
  /*If we've lost connection, start over the setup process, changing our states to disconnected, changing out packets to the MC in handlePackets()*/
  else if(WiFi.status() == WL_CONNECTION_LOST || WiFi.status() == WL_DISCONNECTED){
    //Serial.println("WIFI Signal Lost! Now attempting to restart the AP-Proccess");
    setupAP();
  }
  /*Alert the user for SUCCESS_PAGE_TIME that WiFi has been successfully set up if we're in the trantition phase b/w AP-STA*/
  else if(WIFI_CONNECTED && AP_ACTIVE)
    server.send(200, "text/html", AP_successPage); 
  /*If we're STA_ACTIVE - and connected to Wifi (passing the above if statments)*/
  else if(!AP_ACTIVE){ 
      /*Update the info page with the current state of the patient*/
      String infoPage = STA_infoPage + translateTYPE(currTYPE) + "</h1>\ 
      <form><input type=button value='Refresh' onClick='window.location.reload()'></form>\
      </html>\n";
      
      server.send(200, "text/html", infoPage); //Send to the "global" IP address server
  }
}

/* Collects the user ssid and password from Display Module, saves them, and attempts to access the internet */
void handleLogin(void) {
  handlingLogin = true; //So we don't create multiple jobs
  String ssid;
  String password;
  /*We know there will only be two arguments (this is a little over-kill) but for now we leave it as a for loop*/
  if (server.args() > 0 ) {
    for ( uint8_t i = 0; i < server.args(); i++ ) {
      if (server.argName(i) == "ssid")
        ssid = server.arg(i);
      else if (server.argName(i) == "password")
        password = server.arg(i);
    }
  }
  /*Convert strings to char arrays*/
  char c_ssid[ssid.length() + 1];
  char c_password[password.length() + 1];
  ssid.toCharArray(c_ssid, ssid.length() + 1);
  password.toCharArray(c_password, password.length() + 1);

  /*Atempt to connect to the internet*/
  WiFi.begin(c_ssid, c_password);
  //Serial.print("Connecting to " + ssid);
  int timer = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    timer++;
    //Serial.print('.');
    //refreshWeb(); //Make sure the signal doesn't timeout in the while loop
    // If the network times out, send to an error page
    if (timer > 20 || WiFi.localIP() == IPAddress(172,16,166,60)) {
      //Serial.println("Could not connect to this network");
      currentPage = AP_errorPage; //Alert the user an error has occured
      handlingLogin = false; // We're done handling
      digitalWrite(32, LOW); //Reset the Device (to allow re-trying to go faster)
      return;
    }
  }
  //Serial.println("Connected!");
  handlingLogin = false; // We're done handling

  /*Update the success page with the most current IP Address for the user to use*/
  AP_successPage += WiFi.localIP().toString() + " to start using your device </h3>\
  <p>Restart device to re-setup WiFi</p>\
  </html>\n";
  
  currentPage = AP_successPage; //Alert the user that the Device has been successfully connected to WiFi
  WIFI_CONNECTED = true; 
  //WiFi.mode(WIFI_AP_STA); //Change the module to work as a server AND a router (to continually display success to the user), rather just a router
  t.after(SUCCESS_PAGE_TIME,WiFiConnected); //Wait for the user to be properly alerted that the device has been connected, then get to business connected to the WiFi
}

/*One-time function that handles all the post-setup operations
* Turns of the AP, turns on the STA, and starts the mdns responder for the global IP address
*/
void WiFiConnected(void){
  /*Change the state of our program to be STA_ACTIVE*/
  WiFi.softAPdisconnect();
  //WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  AP_ACTIVE = false; 
  delay(100);

  //Serial.println("");
  //Serial.print("IP address: ");
  //Serial.println(WiFi.localIP());

  /*Attempt to translate the IP to a readbale address TODO: fix the responder, getting error DNS address could not be found*/
  /*Should be able to go to turningtracker/local*/
  if (MDNS.begin("turningtracker", WiFi.localIP())) {
    //Serial.println("MDNS responder started");
  }

  server.begin();

  MDNS.addService("http", "tcp", 80);
}

//////////////////////////////////////////POST-WIFISETUP-FUNCTIONS///////////////////////////////////////////////////////

/*Reads incoming packets from the MC, modifies it based on the state of the module, and sends it back
*Recall from the Microcontoller code that | 0 | 0 | 0 | 0 | ROOM_NUMBER | ID | AWK | TYPE | translates to [ROOM_NUMBER, ID, AWK, TYPE]
*Also recall that for now, all we care about is AWK and TYPE*/
void handlePackets(void){
  if(Serial.available() > 0){
    currPacket = Serial.read(); //Read the incoming packet, if available
    currPacket = currPacket & 253; //Sanity clear of the AWK bit (should have been recieved as a 0) - AND with 11111101
    /*If we're still getting set up, send back the packet with AWK = 0*/
    if(AP_ACTIVE){
      Serial.write(currPacket);
    }
    /*If we're currently STA_ACTIVE, meaning we're connected to the internet and acting as a server, set AWK = 1 and capture the info from the packet*/
    else{
      currPacket |= 2; //Set AWK = 1 by ORing with 00000010
      currTYPE = currPacket & 1; // find TYPE by ANDing with 00000001
    
      /*This if statment is used for demonstration purposes - to put the display back into the PATIENT OK state, after FLIPPED state has hung on as the status
      TODO: test that spamming FLIP doesn't mess up the timer*/
      if(currTYPE == 0 && !toNeutralID)
          toNeutralID = t.after(FLIPPED_UP_TIME, toNeutral);
   
      //TODO: If we had a room # or ID of the device, this is where we would store it from the packet, instead of the dummy display now
      Serial.write(currPacket);//Alert the microcontroller to stop sending pack
   }
  }
}
/*Sets the state from FLIPPED to NONE for demo purposes and hopfully tries to detract spamming from messing with the timers*/
void toNeutral(void){
  if(currTYPE == 0){//if the type didn't switch to NEEDS_FLIP in the waiting interval
    currTYPE = 2;
    t.stop(toNeutralID);
    toNeutralID = 0;
  }
}
/*Converts the TYPE number into a human-readable string, where 0: FLIPPED, 1: NEEDS_FLIP, else: NONE*/
String translateTYPE(int t){
  switch(t){
    case 0: return "FLIPPED";
    case 1: return "PATIENT NEEDS FLIPPING";
    default: return "PATIENT OK";
  }
}


