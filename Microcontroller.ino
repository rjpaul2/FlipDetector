//From the article: http://bildr.org/2012/11/force-sensitive-resistor-arduino
#include "Timer.h" // Library coutesy of Jack Christensen https://github.com/JChristensen/Timer
/***********************************Global Variables*******************************************/
int const SMOOTH_NUMBER = 30;
int const READ_INTERVAL = 100; // How often we take in a sensor reading in milliseconds
int const CHECK_FLIP_INTERVAL = 2000; // (2sec) How often we check a flip and update (CHECK_FLIP_INTERVAL/1000) * secondsSinceLastFlip
int const CHECK_NEEDS_FLIP_INTERVAL = 120000; // (2min) How often we check if we need a flip in milliseconds
int const BUZZER_PIN = 3; // Pin to output to buzzer
int const TOLERANCE = 2.0; // Voltage tolerance to register a flip, the lower the TOLERANCE, the higher the sensitivity of flip reporting
int const STARTING_AVERAGE = 1.1; //(3.3*2)/6 Starting average accross all sensors- NOTE: this assumes that two sensors- max load is an appropriate starting voltage. If a patient takes up 3 or more sensors initially , this can lead to problems in the if statement of checkFlip()


// Global Analog Inputs (SMOOTH_NUMBER readings at each read)
float Sensor_A0[SMOOTH_NUMBER]; float Sensor_A1[SMOOTH_NUMBER]; float Sensor_A2[SMOOTH_NUMBER]; float Sensor_A3[SMOOTH_NUMBER]; float Sensor_A4[SMOOTH_NUMBER]; float Sensor_A5[SMOOTH_NUMBER]; 
float currState[6] = {0.0,0.0,0.0,0.0,0.0,0.0}; // Global current state for each respective 6 analog sensors
float eqState[6] = {0.0,0.0,0.0,0.0,0.0,0.0}; // Global equilibrium state for 6 analog sensors

// Average equilibrium reading accross all sensors (in Volts) -- used to cut corner cases (i.e. someone leaning on the pad, adding voltage)
float averageEqReading = STARTING_AVERAGE; 

Timer t; // Timer for use with tracking flip times
int flipCheckID=0; //timer ID for checking flips
int testID=0; //timer ID for testing events
int readID=0; // timer ID for reading analog inputs
int packetSenderID=0; //timer ID for continually sending packets in case of no awknowledge 
int needsFlipCheckID=0; //timer ID for checking if a patient needs a flip

byte currPacket; // Current packet that needs to be sent and awknowledged
bool handlingPacket = false; // If false, the sent packet has been awknowledged and we're waiting f

int readIndex = 0; // Used for smothing inputs

bool soundBuzzer = false; // True until a patient is flipped when needed

int secondsSinceLastFlip = 0; // Used for telling if a flip is necesary

/*Function Declarations*/
void readSensors(void);
void updateState(void);
void checkFlip(void);
void printTest(void);
void sendPacket(int ROOM_NUMBER, int ID, int AWK, int TYPE);
void checkMalfunction(void);
void buzz(void);
void checkNeedsFlip(void);
void sendUntilAWK(byte packet);
byte convertPacket(int ROOM_NUMBER, int ID, int AWK, int TYPE);



/***********************************Serial Process********************************************/
void setup(){
  /*TODO: figure out a good way to comence all of this (e.g. sensors have certain reading)*/
  Serial.begin(9600);
  
  flipCheckID = t.every(CHECK_FLIP_INTERVAL, checkFlip); // Every CHECK_FLIP_INTERVAL millieconds (2 seconds), check for a flip, TODO: figure out if handlePacket() is going here or in needsFlipCheckID
  testID = t.every(1500, printTest); // For testing purposes
  readID = t.every(READ_INTERVAL, readSensors); //Every READ_INTERVAL (100) milliseconds, read the analog inputs, storing a running SMOOTH_NUMBER of inputs in the SensorA arrays
  needsFlipCheckID = t.every(CHECK_NEEDS_FLIP_INTERVAL, checkNeedsFlip); // Every CHECK_NEEDS_FLIP_INTERVAL milliseconds (2min), check if a patient requires a flip
}

/*Main loop*/
void loop(){
  readPacket(); // Constantly check for recieved signal from Wifi Module 
  t.update(); // Update timer events (see setup())
}

/***********************************Helper Functions*********************************************/
/*Smoothly reads the six analog (over SMOOTH_NUMBER readings) inputs and converts to voltages*/
void readSensors(void){
  // Read and update Analog Inputs
  Sensor_A0[readIndex] = analogRead(A0);Sensor_A1[readIndex] = analogRead(A1);Sensor_A2[readIndex] = analogRead(A2);Sensor_A3[readIndex] = analogRead(A3);Sensor_A4[readIndex] = analogRead(A4);Sensor_A5[readIndex] = analogRead(A5); 
  // Convert inputs to voltage reading
  Sensor_A0[readIndex] *= (5.0 / 1023.0);Sensor_A1[readIndex] *= (5.0 / 1023.0);Sensor_A2[readIndex] *= (5.0 / 1023.0);Sensor_A3[readIndex] *= (5.0 / 1023.0);Sensor_A4[readIndex] *= (5.0 / 1023.0);Sensor_A5[readIndex] *= (5.0 / 1023.0);
  readIndex += 1; // We will be reading in the next index our next time around
  if(readIndex>=SMOOTH_NUMBER){// Wrap-around case of sensor readings 
    readIndex = 0;
    updateState(); //At each wrap around, update global state of each sensor -- NOTE: how often the state is updated is directly impacted by SMOOTH_NUMBER and READ_INTERVAL
    //TODO: maybe put checkFlip here to decrease # of timers?, have to be careful because how often we check the sensors and SMOOTH_NUMBER can alter how often we check for a flip
  }
}

/* Updates equilibrium state of sensor - for use when a flip is registered and the patient settles down to a new position
 * Frequency of call dependent on SMOOTH_NUMBER and READ_INTERVAL (see readSensors())
 */
void updateState(void){
  float total_SensorA0 = 0;float total_SensorA1 = 0;float total_SensorA2 = 0;float total_SensorA3 = 0;float total_SensorA4 = 0;float total_SensorA5 = 0;
  // Finds the running sum of SMOOTH_NUMBER of readings every READ_INTERVAL milliseconds
  for(int i = 0; i < SMOOTH_NUMBER; i++){
    total_SensorA0 += Sensor_A0[i];
    total_SensorA1 += Sensor_A1[i];
    total_SensorA2 += Sensor_A2[i];
    total_SensorA3 += Sensor_A3[i];
    total_SensorA4 += Sensor_A4[i];
    total_SensorA5 += Sensor_A5[i];
  }
  // Uses the running sum to find the running average of SMOOTH_NUMBER of readings
  currState[0] = total_SensorA0/SMOOTH_NUMBER;currState[1] = total_SensorA1/SMOOTH_NUMBER;currState[2] = total_SensorA2/SMOOTH_NUMBER;currState[3] = total_SensorA3/SMOOTH_NUMBER;currState[4] = total_SensorA4/SMOOTH_NUMBER;currState[5] = total_SensorA5/SMOOTH_NUMBER;
}  
/*Returns average reading (in Volts) accross all sensors in current state*/
float getAverageReading(void){
  return (currState[0] + currState[1] + currState[2] + currState[3] + currState[4] + currState[5])/6;
}
/* Updates the equilibrium state and sets the flip timer to 0 if a flip is registers, does nothing if no flip otherwise*/
void checkFlip(void){
  //buzz();
  handlePacket(); // Check to see if any packet needs to be sent back to the Wifi Module TODO: Put this somewhere less frequently called
  secondsSinceLastFlip += 2; // Every second, update our timer keeping track of the current time since our last flip (because this is called every two seconds, we can just add 2)
  //if any sensor deviates from its smooth value by TOLERANCE Volts AND a high average increase in voltage is not registered (i.e. someone sitting on the bed, applying force to sensor), a flip is registered
  if((abs(eqState[0]-currState[0]) > TOLERANCE || abs(eqState[1]-currState[1]) > TOLERANCE || abs(eqState[2]-currState[2]) > TOLERANCE
  || abs(eqState[3]-currState[3]) > TOLERANCE || abs(eqState[4]-currState[4]) > TOLERANCE || abs(eqState[5]-currState[5]) > TOLERANCE) 
  && (getAverageReading() - averageEqReading) < (TOLERANCE / 6)){// Where TOLERANCE / 6 is the minimum additional average voltage to register as an outside force, not the patient, adding voltage to the pad -- TODO: test average stuff
    // If we detect a flip, update the equilibrium state to be the current state
    for(int i = 0; i < 6; i++){
      eqState[i] = currState[i];
     }
    // TODO: THIS AVERAGE SHIT MAY LEAD TO PROBLEMS need to know the perfect moment to 'record' EQ average initially (maybe 6v initially?)
    if(getAverageReading() > STARTING_AVERAGE) // Cuts off averageEqReading from dropping too low, making the outlying if statement unreachable, used if a patient uses more than 6.6V continually
      averageEqReading = getAverageReading(); // Update equilibrium average across all sensors 
    secondsSinceLastFlip = 0; //Reset timer
    soundBuzzer = false; //Shut up the buzzer
    Serial.println("FLIPPED\n");
    sendPacket(0,0,0,0); //FLIPPED TODO: check if the sent packet is output to the module when a flip occurs (and echoed)
  }
}


/*Sends packet of form [ROOM_NUMBER, ID, AWK, TYPE] 
*@param ROOM_NUMBER: set to 0 for ECE 445 purposes, would be used in the event of multiple devices
*@param ID: ID of device, set to 0 for ECE 445 purposes, would be used in the event of multiple devices
*@param AWK: set to 0 when sent. The microcontroller looks that the packet is recieved from the wifi module as a 1
*@param TYPE: set to 0 for FLIPPED and 1 for NEEDS_FLIP
*We send the packet as a byte | 0 | 0 | 0 | 0 | ROOM_NUMBER | ID | AWK | TYPE |
*/
void sendPacket(int ROOM_NUMBER, int ID, int AWK, int TYPE){
  // Convert to form | 0 | 0 | 0 | 0 | ROOM_NUMBER | ID | AWK | TYPE |
  currPacket = convertPacket(ROOM_NUMBER, ID, AWK, TYPE); //Update global packet, if another packet is already trying to be sent/awknowledged, this overwrites it, due to the fact that is a NEEDS_FLIP
  handlingPacket = true; //Continually send until an AWK is received
  Serial.write(currPacket);// Write to TX --- TODO: Test that this works just as it prints
  
}

/*Converts packet from [ROOM_NUMBER, ID, AWK, TYPE] to | 0 | 0 | 0 | 0 | ROOM_NUMBER | ID | AWK | TYPE |*/
byte convertPacket(int ROOM_NUMBER, int ID, int AWK, int TYPE){
  byte packet = 00000000;
  // Convert to form | 0 | 0 | 0 | 0 | ROOM_NUMBER | ID | AWK | TYPE |
  packet += (TYPE) & 15;
  packet += (AWK << 1) & 15;
  packet += (ID << 2) & 15;
  packet += (ROOM_NUMBER << 3)& 15;
  return packet;
}
/* Reads and analizes packet if RX triggers, else does nothing
 * ASSUMES THAT THE PACKET IS SENT ONCE FROM THE WIFI MODULE ONCE - NOT CONTINUALLY
 */
void readPacket(void){
  if(Serial.available() > 0){
    currPacket = Serial.read(); //Update recieved packet, this will be the same as our sent packet (sendPacket()) but with same/different AWK
    handlingPacket = true; //Notify handlePacket() that the recieved packet needs analyizing
  }
}

void checkMalfunction(void){
  // Somehow compare equilibrium state to current state to sound the buzzer -- probabaly make this an optional functionality
}

/*Sounds buzzer every ??? seconds until patient is flipped*/
void buzz(void){
  tone(BUZZER_PIN, 2000, 500); // 2000 HZ for 500 milliseconds
}
/* Continually anaylize global packet. If the packet is recieved back by readPacket() and AWK=0 is read, we keep on writing the packet until AWK =1 is read. The frequency of this function is specified by TODO:???
 * Here we expect the Wifi Module to send back a packet once with either AWK = 0 or AWK = 1 (maybe try to send the signal over 2 minutes until AWK=1, else send back with AWK=0)
 * Note that the Wifi Module will continually get signals until an AWK = 1 is sent and receieved
 */
void handlePacket(void){
  if(handlingPacket){// While we're supposed to be handling the packet
    if((currPacket & 2) >> 1 ==0){// If AWK = 0, meaning not awknowledged
      Serial.write(currPacket); //Send the packet back to the Wifi Module
      //Serial.println("HERE");
    }
    else{// If AWK = 1, meaning awknowledged
      handlingPacket = false; //The sent signal is awknowledged by the user, and we no longer need to send back the packet
    }
  }
}

/*Continually attemps to alert the nurse that a flip is nesasarry, upon no flip and/or no AWK, we alert our emergency buzzer - Do nothing else*/
void checkNeedsFlip(void){
  if(soundBuzzer){buzz();} //Chirp the buzzer until a flip is registered TODO: maybe put this somewhere more frequent?
  if(secondsSinceLastFlip > 6300 && secondsSinceLastFlip<7200){ //15 minute warning
   sendPacket(0,0,0,1);//TODO: also be careful here, because checkNeedsFlip goes every couple of minutes
  }
  else if(secondsSinceLastFlip > 7200){// Two-hours expired
    soundBuzzer = true;//TODO: Figure out a way to terminate this upon flip
  }
}

/*Test function*/
void printTest(void){
  //Serial.println(convertPacket(0, 0, 0, 1));
  //sendPacket(1, 0, 0, 1);
  //handlePacket(1);
  
  Serial.println("Sensor 0: " + String(currState[0]));
  Serial.println("Sensor 1: " + String(currState[1]));
  Serial.println("Sensor 2: " + String(currState[2]));
  Serial.println("Sensor 3: " + String(currState[3]));
  Serial.println("Sensor 4: " + String(currState[4]));
  Serial.println("Sensor 5: " + String(currState[5]));
  Serial.println("\n");
  

  /*
  Serial.println("Sensor 0: " + String(analogRead(A0) * (5.0 / 1023.0)));
  Serial.println("Sensor 1: " + String(analogRead(A1) * (5.0 / 1023.0)));
  Serial.println("Sensor 2: " + String(analogRead(A2) * (5.0 / 1023.0)));
  Serial.println("Sensor 3: " + String(analogRead(A3) * (5.0 / 1023.0)));
  Serial.println("Sensor 4: " + String(analogRead(A4) * (5.0 / 1023.0)));
  Serial.println("Sensor 5: " + String(analogRead(A5) * (5.0 / 1023.0)));
  Serial.println("\n");
  */
 
  /*
  Serial.println("Reading 0:" + String(Sensor_A5[0]));
  Serial.println("Reading 1:" + String(Sensor_A5[1]));
  Serial.println("Reading 2:" + String(Sensor_A5[2]));
  Serial.println("Reading 3:" + String(Sensor_A5[3]));
  Serial.println("Reading 4:" + String(Sensor_A5[4]));
  Serial.println("Reading 5:" + String(Sensor_A5[5]));
  Serial.println("Reading 6:" + String(Sensor_A5[6]));
  Serial.println("Reading 7:" + String(Sensor_A5[7]));
  Serial.println("Reading 8:" + String(Sensor_A5[8]));
  Serial.println("Reading 9:" + String(Sensor_A5[9]));
  Serial.println("\n");
  */
}

