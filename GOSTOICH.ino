///////////////////////////////////////////////////////////////////////
//             University of North Carolina Greensboro               //
//                     Croatt Research Group                         //
//                    Division of Flow Chemistry                     //
//                        Michael B. Spano                           //
//                        brzusa@gmail.com                           //
//                           March/2016                              //
///////////////////////////////////////////////////////////////////////
//  Go to our homepage for info on how to build your own system      //
//           https://chem.uncg.edu/croatt/flow-chemistry/            //
//       For more info watch: https://youtu.be/R4Ohwhk47tg           //
//                                                                   //
//////////////////////DESCRIPTION OF THIS CODE/////////////////////////
//                                                                   //
//This is a version of the Serial Event Stepper Control (SESC) called//
//GoStoich. It's designed to automate the task of creating yield maps//
//using segmented plug flow. It allows for the simultaneous injection//
//of 2 reagent pumps to create a reaction plug followed by a stream  //
//of solvent to push the plug through the reactor at the desired     //
//retention time. You currently have controll over the relative rates//
//in which the reagent pumps inject by using the command 'stoich'. At//
//the current time temperature control is done externally from this  //
//program (i.e. manually).                                           //
//                                                                   //
//             Temperature/(C)                                       //
//                        |  o   o   o   o                           //
//                        |  o   o   o   o                           //
//                        |  o   o   o   o                           //
//                        |  o   o   o   o                           //
//                        |  o   o   o   o                           //
//                        |_ _ _ _ _ _ _ _                           //
//                        Reaction time / (s)                        //
//                                                                   //
//////////////HARDWARE SETUP USING A4988 BREAKOUT BOARD////////////////
//                                                                   //
//                   [Arduino]----------[a4988]                      //
//                      5V    ----------  VDO                        //
//                      GND   ----------  GND                        //
//                     PIN 2  ----------  STEP                       //
//                     PIN 3  ----------  DIR                        //
//                                                                   //
//Connect two wires of the stepper motor and  turn the axel, if it is//
//much more difficult to spin, these wires are 1A and 1B. The other  //
//two wires make pair 2A and 2B.                                     //
//                                                                   //
//                   [a4988]------------[Motor]                      //
//                     1A    ----------    1A                        //
//                     1B    ----------    1B                        //
//                     2A    ----------    2A                        //
//                     2B    ----------    2B                        //
//                                                                   //
//                  [24V DC]------------[a4988]                      //
//                   +24V    ----------  VMOT                        //
//                    GND    ----------  GND                         //
//                                                                   //
/////////////////////COMMANDS (USE SERIAL MONITOR)/////////////////////
//                                                                   //
//set1]##] - Sets the current volume that is in stepper1             //
//reset] - Makes the current position correspond to 0mL              //
//volumes] - returns how many mL are in the syringe                  //
//setflush]##.##] - Sets the current in the flush pump's syringe.    //
//diamater]##] - Changes the internal diamater of all syringes       //
//lead]##] - Changes screw lead for all pumps [Rev. per Millimeter]  //
//plugvolume]####] - Sets the reaction plug volume in microliters    //
//reactorvolume] - Sets the total reactor volume in microliters.     //
//time] - Sets the retention time of the reaction                    //
//stoich]#.##:#.## - Sets the relative flow rates for reactor pumps  //
//info] - Displays relevant info                                     //
//snug]#] - moves pump 1, 2 or 3 Forwards at a pre determined rate   //
//slip]#] - moves pump 1, 2 or 3 Backwards at a pre determined rate  //
//                                                                   //
//////////////////////////RECOMMENDED USE//////////////////////////////
//                                                                   //
//1) Before powering on the electronics fill the syringes with       //
//reagents and insert them into the pumps.  Adjust the position by   //
//spinning the helical flex coupler with your fingers until the      //
//syringe flange is flush with chassis                               //
//                                                                   //
//2) Power on the electronics. The arduino is now in control of the  //
//pumps and the shaft will no longer rotate freely                   //
//                                                                   //
//3) Use the commands -> set1]##] to tell the arduino where the      //
//reagent syringes are and setflush]##] for the flushpump. Remember  //
//to use the correct syntax. The volume should be in mL and allows   //
//for decimal places. (i.e. 14.27)                                   //
//                                                                   //
//4) Use -> setflush]##] to tell the arduino how much solvent is in  //
//the flush pump.                                                    //
//                                                                   //
//5) Set the reaction parameters using stoich] and time] and then    //
//run the reaction using go]. You will be prompted to change these   //
//parameters once a reaction is done.                                //
//                                                                   //
///////////////////////////////////////////////////////////////////////
             
#include <AccelStepper.h> 
//Includes the functions found in the AccelStepper library
AccelStepper stepper1(1, 2, 5); 
// Defines an instance of AccelStepper called stepper1
// The STEP pin is 2 and the DIR pin is 5 
AccelStepper stepper2(1, 3, 6); 
// Defines another instance of AccelStepper called stepper 2
// The STEP pin is 3 and the DIR pin is 6
AccelStepper flushpump(1,4,7);
// Defines another instance of AccelStepper called flushpump
// The STEP pin is 4 and the DIR pin is 7
String inputString = "", command; 
boolean stringComplete = false, protect = false, bounce = false;
boolean rebound = false, sufficientSolvent, sufficientReagents; 
////////////////////////////////////////////////////////////////////////
int i;
float SyringeDiameter = 12.0;
// Syringe diameter, Units: [millimeters]
float ThreadDensity = 0.7874;
// Thread density of lead screw, Unit: [Revolutions per millimeter]
// This is the stepper motor step angle.
float position1, position2, position3, rate1, rate2, stoich1 = 1.0, stoich2 = 1.0;
float commandval;
float plugvolume = 500;//Unit: [microliters] 
float reactorvolume = 2000;//Unit: [microliters]
float reactiontime = 60; //Unit: [seconds]
int delimiterIndex[7] = {0,0,0,0,0,0};
float spm; // spm is a acronym for 'Steps Per Milliliter'
// spm is calculated during void setup() and will differ based on 
// syringeDiameter, ThreadDensity and StepAngle 
///////////////////////////////////////////////////////////////////////
void setup() {
  Serial.begin(9600);
  inputString.reserve(50);
  stepper1.setMaxSpeed(4000);
  stepper2.setMaxSpeed(4000);
  flushpump.setMaxSpeed(4000);
  stepper1.setMinPulseWidth(20);
  stepper2.setMinPulseWidth(20);
  flushpump.setMinPulseWidth(20);
  spm = ThreadDensity*200.0/(3.1415*square(SyringeDiameter/2.0)/1000.0); 
  // ThreadDensity[revolutions/mm]*200[steps/revolution]/{Pi*r^2/1000}[mL/mm}
  Serial.println(F("University of North Carolina Greensboro"));
  Serial.println(F("          Croatt Research Group")); 
  Serial.println(F("        Division of Flow Chemistry"));
  Serial.println();
  Serial.print(F("Syringe ")); Serial.print(SyringeDiameter); Serial.println(F(" mm"));
  Serial.print(F("Lead Screw ")); Serial.print(ThreadDensity); Serial.println(F(" Rev/mm"));
  Serial.print(F("spm ")); Serial.print(spm); Serial.println(F(" Steps/mL"));
  printfluidData();
  Serial.println();
  Serial.println(F("Go?"));
}
///////////////////////////////////////////////////////////////////////
float globalflowrate(){
 return (reactorvolume*60)/(reactiontime*1000);
} 
///////////////////////////////////////////////////////////////////////
void printfluidData(){
  Serial.print(F("Stoichiometry is ")); Serial.print(stoich1); Serial.print(F(":")); Serial.println(stoich2);
  Serial.print(F("The plug volume is ")); Serial.print(plugvolume); Serial.println(F(" uL"));
  Serial.print(F("Reactor volume is ")); Serial.print(reactorvolume); Serial.println(F(" uL"));
  Serial.print(F("Global flow rate is ")); Serial.print(globalflowrate()); Serial.println(F(" mL/min"));
  Serial.print(F("Reaction time is ")); Serial.print(reactiontime/60); Serial.println(F(" min"));
}
///////////////////////////////////////////////////////////////////////
void printVolumes(){
  position1 = stepper1.currentPosition();
  position2 = stepper2.currentPosition(); 
  position3 = flushpump.currentPosition();
  Serial.print(F("Current volumes are: "));
  Serial.print(-milliliters(position1),2);
  Serial.print(" mL");
  Serial.print(" & ");
  Serial.print(-milliliters(position2),2);
  Serial.print(" mL");
  Serial.println();
  Serial.print(F("The flushpump has "));
  Serial.print(-milliliters(position3),2);
  Serial.println(F(" mL"));
}
///////////////////////////////////////////////////////////////////////
float FlowToStepRate(float flowRate){
 return flowRate*spm/60; // Accepts a flowrate ##.## mL and converts it to an int steps/s
}
///////////////////////////////////////////////////////////////////////
float volumetosteps(float mL){
//Accepts a volume ##.## and returns the corresponding integer of steps
//the motor must take make.
  float s = mL * spm; 
  return s;
}
///////////////////////////////////////////////////////////////////////
float milliliters(float motorposition ){  //spm is steps per milliliter
  return motorposition / spm; 
}
///////////////////////////////////////////////////////////////////////
void activateFlush(){
  Serial.println(F("Continuing flow with solvent...")); 
  
        do{
          flushpump.moveTo(position3 + volumetosteps((reactorvolume-plugvolume)/1000));
          flushpump.setSpeed(FlowToStepRate(globalflowrate()));
          flushpump.runSpeedToPosition();
      } 
        while(flushpump.distanceToGo() != 0);
          flushpump.stop();
          Serial.println(F("Ready for new parameters"));
      }  
///////////////////////////////////////////////////////////////////////
// This function creates a string 'inputString' by compound addition
// of the characters stored in the serial buffer. The global boolean
// 'stringComplete' is then set to true to tell the main loop that 
// a new user input is available.

void serialEvent() {
  while (Serial.available()) {
    // get the new byte:
    char inChar = (char)Serial.read();
    // add it to the inputString:
    inputString += inChar;
    // if the incoming character is a newline, set a flag
    // so the main loop can do something about it:
    if (inChar == '\n') {
      stringComplete = true;
    }
  }
}
///////////////////////////////////////////////////////////////////////
void loop() {
  // First check the global boolean 'stringComplete'
  if (stringComplete) {
    // If there is a new user input, find the character delimiters 
    // and store them in the character array called 'delimiterIndex'.
    Serial.print("->  User input: ");
    Serial.print(inputString);
    delimiterIndex[0] = inputString.indexOf(']');
    delimiterIndex[1] = inputString.indexOf(']', delimiterIndex[0]+1);
    delimiterIndex[2] = inputString.indexOf(':');
    // Use the numbers stored in the character array to split the 
    // inputString into more managable pieces. These will be used
    // to control the flow of the program a bit further down.
    String command = inputString.substring(0,delimiterIndex[0]);
    String commandvalue = inputString.substring(delimiterIndex[0]+1, delimiterIndex[1]);
    String stoicha = inputString.substring(delimiterIndex[0]+1,delimiterIndex[2]);
    String stoichb = inputString.substring(delimiterIndex[2]+1);  
    // The string which contains numbers needs to be converted 
    // to a floating point number to be used in calculations.
    commandval = commandvalue.toFloat();
    // Now the original 'inputString' is cleared and the global
    // boolean 'stringComplete; is set to false.
    inputString = ""; 
    stringComplete = false;
    // Now that the user input has been processed the code
    // proceeds to manage the stepper motors. First thing
    // is to refresh where each stepper is with respect to
    // it's initial position.
    position1 = stepper1.currentPosition();
    position2 = stepper2.currentPosition(); 
    position3 = flushpump.currentPosition();
    // Now begins a multitude of 'if' statements to check what
    // the program should do with the users input. Most procedures
    // are evident in their functionality. 
    if(command == "info")printfluidData();
    // The 'snug' command followed by an integer moves one or more 
    // syringe pumps forward. This is useful when the user does 
    // not want to turn the system off to manually adjust the position
    // of the syringe block. Integer '4' is used to move all pumps
    // simultaneously
    if(command == "snug" && commandval != 0){
      switch ((int)commandval){
        case 1:
          do{
            stepper1.setSpeed(FlowToStepRate(2.5));
            stepper1.runSpeed();
            serialEvent();
          }
          while(!stringComplete);
        break;
        case 2:
          do{
            stepper2.setSpeed(FlowToStepRate(2.5));
            stepper2.runSpeed();
            serialEvent();
          }
          while(!stringComplete);
        break;
        case 3:
          do{
            flushpump.setSpeed(FlowToStepRate(2.5));
            flushpump.runSpeed();
            serialEvent();
          }
          while(!stringComplete);
        break;
        case 4:
          do{
            stepper1.setSpeed(FlowToStepRate(1));
            stepper2.setSpeed(FlowToStepRate(1));
            flushpump.setSpeed(FlowToStepRate(1));
            flushpump.runSpeed();
            stepper1.runSpeed();
            stepper2.runSpeed();
            serialEvent();
          }
          while(!stringComplete);
        default:
        break;
      }
    }
    // The 'slip' command is analogous to the 'snug' command
    // with the exception that it moves the pumps in the 
    // reverse direction. 
    if(command == "slip" && commandval != 0){
      switch ((int)commandval){
        case 1:
          do{
            stepper1.setSpeed(-FlowToStepRate(2.5));
            stepper1.runSpeed();
            serialEvent();
          }
          while(!stringComplete);
        break;
        case 2:
          do{
            stepper2.setSpeed(-FlowToStepRate(2.5));
            stepper2.runSpeed();
            serialEvent();
          }
          while(!stringComplete);
        break;
        case 3:
          do{
            flushpump.setSpeed(-FlowToStepRate(2.5));
            flushpump.runSpeed();
            serialEvent();
          }
          while(!stringComplete);
        break;
        case 4:
        do{
            stepper1.setSpeed(-FlowToStepRate(2.5));
            stepper2.setSpeed(-FlowToStepRate(2.5));
            flushpump.setSpeed(-FlowToStepRate(2.5));
            stepper2.runSpeed();
            stepper1.runSpeed();
            flushpump.runSpeed();
            serialEvent();
          }
          while(!stringComplete);
        default:
        break;
      }
    }
    // The 'go' command simply calls the go() function and initiates
    // a plug reaction.
    if(command == "go") go();
    if(command == "reactorvolume" && commandval != 0){
      reactorvolume = commandval;
      Serial.print(F("The fluidic parameters have been changed "));
      printfluidData();
    }
    if(command == "plugvolume" && commandval != 0){
      plugvolume = commandval;
      Serial.print(F("The fluidic parameters have been changed "));
      printfluidData();
    }
    if(command == "stoich" && stoich1 && stoich2){
      stoich1 = stoicha.toFloat();
      stoich2 = stoichb.toFloat();
      Serial.print(F("The fluidic parameters have been changed "));
      printfluidData();
    }
    if(command == "time" && commandval != 0){
      reactiontime = commandval;
      Serial.print(F("The fluidic parameters have been changed "));
      printfluidData();
    }    
    if (command == "volumes" )printVolumes();    
    if (command == "reset" ){
      stepper1.setCurrentPosition(0);   
      stepper2.setCurrentPosition(0); 
      flushpump.setCurrentPosition(0); 
    }
    // The 'set1', 'set2', and 'setflush' commands are essential
    // because they are used to inform the system how much fluid
    // is in each syringe! If no values are informed at the begining
    // the go() function will not function properly because the 
    // system thinks there are no reagents in the syringes!!!
    if (command == "set1" && commandval != 0){
      stepper1.setCurrentPosition(-volumetosteps(commandval));  
      printVolumes();   
    }
    if (command == "set2" && commandval != 0){
      stepper2.setCurrentPosition(-volumetosteps(commandval));
      printVolumes();
    }
    if (command == "setflush" && commandval != 0){
      flushpump.setCurrentPosition(-volumetosteps(commandval));
      printVolumes();     
    }
    if (command == "diameter" && commandval !=0){
      SyringeDiameter = commandval;
      spm = ThreadDensity*200.0/(3.1415*square(SyringeDiameter/2.0)/1000.0);
      Serial.println(F("The power train dimensions have been modified"));
      Serial.print(F("Syringe ")); Serial.print(SyringeDiameter); Serial.println(F(" mm"));
      Serial.print(F("Lead Screw ")); Serial.print(ThreadDensity); Serial.println(F(" Rev/mm"));
      Serial.print(F("spm ")); Serial.print(spm); Serial.println(F(" Steps/mL")); 
    }
    if (command == "lead" && commandval !=0){
      ThreadDensity = commandval;
      spm = ThreadDensity*200.0/(3.1415*square(SyringeDiameter/2.0)/1000.0);
      Serial.println(F("The power train dimensions have been modified"));
      Serial.print(F("Syringe ")); Serial.print(SyringeDiameter); Serial.println(F(" mm"));
      Serial.print(F("Lead Screw ")); Serial.print(ThreadDensity); Serial.println(F(" Rev/mm"));
      Serial.print(F("spm ")); Serial.print(spm); Serial.println(F(" Steps/mL")); 
    }    
    if (command == "Go" && commandval == 0) go();
  }
}   
///////////////////////////////////////////////////////////////////////
//                Done handling the Serial Event                     //
///////////////////////////////////////////////////////////////////////

void go(){
  // First thing is to check if there is sufficient liquid in the syringes
  // the code is split among several lines for printing on A4 paper
    if(position1 + volumetosteps((stoich1*plugvolume/(1000*(stoich1+stoich2)))) > 0 && position2 + volumetosteps((stoich2*plugvolume/(1000*(stoich1+stoich2)))) > 0) sufficientReagents = false;
    if(position1 + volumetosteps((stoich1*plugvolume/(1000*(stoich1+stoich2)))) <= 0 && position2 + volumetosteps((stoich2*plugvolume/(1000*(stoich1+stoich2)))) <= 0)sufficientReagents = true; 
    if(position3 + volumetosteps((reactorvolume/1000)) > 0)sufficientSolvent = false;
    if(position3 + volumetosteps(reactorvolume/1000) <= 0)sufficientSolvent = true;
  // If there is sufficient liquid then the experiment is initiated.   
    if(sufficientReagents && sufficientSolvent){
        Serial.println(F("Initiation flow experiment..."));
        Serial.println(F("Injecting reagents."));
        do{
          stepper1.moveTo(position1 + volumetosteps((stoich1*plugvolume/(1000*(stoich1+stoich2)))));
          stepper2.moveTo(position2 + volumetosteps((stoich2*plugvolume/(1000*(stoich1+stoich2)))));
          stepper1.setSpeed(FlowToStepRate(globalflowrate()*stoich1/(stoich1+stoich2)));
          stepper2.setSpeed(FlowToStepRate(globalflowrate()*stoich2/(stoich1+stoich2)));
          stepper1.runSpeedToPosition();
          stepper2.runSpeedToPosition();
      } 
        while(stepper1.distanceToGo() != 0 || stepper2.distanceToGo() != 0);
          stepper1.stop();
          stepper2.stop();
          activateFlush();
      }  
      // If there is not sufficient liquid then the experiment is postponed
      // and the user is informed to refil the syringes via the serial monitor.
      else{
        if(!sufficientReagents){
          Serial.println(F("Not enough reagents")); 
          do{
            stepper1.moveTo(position1 - volumetosteps(1));
            stepper2.moveTo(position2 - volumetosteps(1));
            stepper1.setSpeed(FlowToStepRate(60));
            stepper2.setSpeed(FlowToStepRate(60));
            stepper1.runSpeedToPosition();
            stepper2.runSpeedToPosition();
          }
          while(stepper1.distanceToGo() != 0 || stepper2.distanceToGo() != 0);
          stepper1.stop();
          stepper2.stop();
          Serial.println(F("Please refill the reagent syringes."));
        }
        if(!sufficientSolvent){
          Serial.println(F("Not enough solvent"));
          do{
            flushpump.moveTo(position3 - volumetosteps(2));
            flushpump.setSpeed(FlowToStepRate(60));
            flushpump.runSpeedToPosition();
          }
          while(flushpump.distanceToGo() != 0);
          flushpump.stop();
          Serial.println(F("Please refill the flush pump."));
        }
     }
 }
