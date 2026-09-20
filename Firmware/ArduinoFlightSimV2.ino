#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

Adafruit_MPU6050 mpu;

float GyroXDistance = 0;
float GyroYDistance = 0;
float GyroZDistance = 0;

int GyroXCategory;

float GyroXOffset = 0;
float GyroYOffset = 0;
float GyroZOffset = 0;

float roll = 0.0;
float pitch = 0.0;

// Pin definitions
const int RPWM_PIN1 = 12;  // RPWM pin for extension
const int LPWM_PIN1 = 11;  // LPWM pin for retraction
const int R_EN_PIN1 = 22;  // R_EN pin
const int L_EN_PIN1 = 23;  // L_EN pin
const int RPWM_PIN2 = 10;  // RPWM pin for extension
const int LPWM_PIN2 = 9;   // LPWM pin for retraction
const int R_EN_PIN2 = 24;  // R_EN pin
const int L_EN_PIN2 = 25;  // L_EN pin
const int RPWM_PIN3 = 8;   // RPWM pin for extension
const int LPWM_PIN3 = 7;   // LPWM pin for retraction
const int R_EN_PIN3 = 26;  // R_EN pin
const int L_EN_PIN3 = 28;  // L_EN pin
const int RPWM_PIN4 = 6;   // RPWM pin for extension
const int LPWM_PIN4 = 5;   // LPWM pin for retraction
const int R_EN_PIN4 = 32;  // R_EN pin
const int L_EN_PIN4 = 33;  // L_EN pin

const int BL_ExtendedLimiter = 53;
const int BL_RetractedLimiter = 52;
const int BR_ExtendedLimiter = 51;
const int BR_RetractedLimiter = 50;
const int FR_ExtendedLimiter = 49;
const int FR_RetractedLimiter = 48;
const int FL_ExtendedLimiter = 47;
const int FL_RetractedLimiter = 46;

int speed = 255;
int FR_Actuator_effect = 0;
int FL_Actuator_effect = 0;
int BR_Actuator_effect = 0;
int BL_Actuator_effect = 0;

bool moveUp = false;
bool moveDown = false;
bool rollRight = false;
bool rollLeft = false;

// *** CHANGED: Added new timing variables for better communication ***
unsigned long lastValidTime = 0;
unsigned long lastReadyTime = 0;
unsigned long lastIMURead = 0;  // NEW: Track IMU reading timing
const unsigned long failSafeDelayMs = 3000; // 3s tolerance
const unsigned long readyInterval = 5000;   // Send READY every 5 seconds if no data
const unsigned long imuReadInterval = 50;   // NEW: Read IMU every 50ms

bool leveling = false;
bool systemReady = false;

bool processingCommand = false;
unsigned long commandStartTime = 0;
const unsigned long commandTimeout = 100; // 100ms max per command

float targetPitch = 0.0;    // Target pitch from FlightGear
float targetRoll = 0.0;     // Target roll from FlightGear
float currentPitch = 0.0;   // Current platform pitch from IMU
float currentRoll = 0.0;    // Current platform roll from IMU
const float motionDeadband = 1.0;    // CHANGED: Increased from 0.5 to 1.0 degrees
const float maxMotionAngle = 12.0;   // CHANGED: Reduced from 15.0 to 12.0 degrees
const float motionGain = 1.0;        // CHANGED: Reduced from 0.3 to 0.2

// NEW: Add filtering for IMU readings
float filteredPitch = 0.0;
float filteredRoll = 0.0;
const float imuAlpha = 0.8;  // Low-pass filter coefficient

void computePitchRoll(const sensors_event_t& a, float& pitchDeg, float& rollDeg) {
  float ax = a.acceleration.x;
  float ay = a.acceleration.y;
  float az = a.acceleration.z;

  float pitchRad = atan2(-az, sqrt(ax*ax + ay*ay)); // X is up
  float rollRad  = atan2( ay, sqrt(ax*ax + az*az));

  pitchDeg = pitchRad * 57.2957795f - 19.72 + 1.86;
  rollDeg  = rollRad  * 57.2957795f - 0.54;
}

// NEW: Function to continuously update IMU readings
void updateIMU() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  
  float rawPitch, rawRoll;
  computePitchRoll(a, rawPitch, rawRoll);
  
  // Apply low-pass filtering to smooth readings
  filteredPitch = (imuAlpha * filteredPitch) + ((1.0 - imuAlpha) * rawPitch);
  filteredRoll = (imuAlpha * filteredRoll) + ((1.0 - imuAlpha) * rawRoll);
  
  currentPitch = filteredPitch;
  currentRoll = filteredRoll;
}

// *** ORIGINAL levelPlatform function - UNCHANGED ***
void levelPlatform() {
  const float levelThreshold = 1.0;     // Degrees tolerance for "level"
  const float maxTilt = 30.0;           // Max expected tilt in degrees
  const float moveThreshold = 0.05;      // Minimum command to trigger actuator
  const unsigned long timeout = 100000; // 100 seconds max
  unsigned long startTime = millis();
  leveling = false;

  // === Sign calibration ===
  const int kSignPitch = +1; // doesn't invert the pitch
  const int kSignRoll  = -1; // flip roll so left/right correction is correct

  Serial.println("Starting platform leveling...");

  while (millis() - startTime < timeout) {
    // Read IMU
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);

    // Compute pitch and roll using your helper
    float pitch, roll;
    computePitchRoll(a, pitch, roll);

    delay(1000);

    Serial.print("Pitch: "); Serial.print(pitch);
    Serial.print(" | Roll: "); Serial.println(roll);

    // Normalize pitch and roll
    float pitchEffect = constrain((kSignPitch * pitch) / maxTilt, -1.0, 1.0);
    float rollEffect  = constrain((kSignRoll  * roll)  / maxTilt, -1.0, 1.0);

    Serial.println("PitchEffect: "+ String(pitchEffect) + " RollEffect: " + String(rollEffect));

    // Combine effects into commands for each actuator
    float FR_cmd = -pitchEffect + rollEffect;
    float FL_cmd = -pitchEffect - rollEffect;
    float BR_cmd =  pitchEffect + rollEffect;
    float BL_cmd =  pitchEffect - rollEffect;

    // Threshold-based movement
    int FR = (FR_cmd > moveThreshold) ? 1 : (FR_cmd < -moveThreshold) ? -1 : 0;
    int FL = (FL_cmd > moveThreshold) ? 1 : (FL_cmd < -moveThreshold) ? -1 : 0;
    int BR = (BR_cmd > moveThreshold) ? 1 : (BR_cmd < -moveThreshold) ? -1 : 0;
    int BL = (BL_cmd > moveThreshold) ? 1 : (BL_cmd < -moveThreshold) ? -1 : 0;

    // Enforce symmetry to prevent slack
    int frontMax = max(abs(FR), abs(FL));
    int backMax  = max(abs(BR), abs(BL));

    FR = (FR != 0) ? frontMax * (FR > 0 ? 1 : -1) : 0;
    FL = (FL != 0) ? frontMax * (FL > 0 ? 1 : -1) : 0;
    BR = (BR != 0) ? backMax  * (BR > 0 ? 1 : -1) : 0;
    BL = (BL != 0) ? backMax  * (BL > 0 ? 1 : -1) : 0;

    Serial.println("FR: " + String(FR) + " FL: " + String(FL) + " BR: " + String(BR) + " BL: " + String(BL));

    // Read limit switches
    int BR_ExtendedLimiterState = digitalRead(BR_ExtendedLimiter);
    int BR_RetractedLimiterState = digitalRead(BR_RetractedLimiter);
    int BL_ExtendedLimiterState = digitalRead(BL_ExtendedLimiter);
    int BL_RetractedLimiterState = digitalRead(BL_RetractedLimiter);
    int FR_ExtendedLimiterState = digitalRead(FR_ExtendedLimiter);
    int FR_RetractedLimiterState = digitalRead(FR_RetractedLimiter);
    int FL_ExtendedLimiterState = digitalRead(FL_ExtendedLimiter);
    int FL_RetractedLimiterState = digitalRead(FL_RetractedLimiter);

    // === FRONT RIGHT ===
    if (FR > 0 && FR_ExtendedLimiterState == 0) {
      analogWrite(RPWM_PIN1, speed); 
      analogWrite(LPWM_PIN1, 0);
    } 
    else if (FR < 0 && FR_RetractedLimiterState == 0) {
      analogWrite(RPWM_PIN1, 0);  
      analogWrite(LPWM_PIN1, speed);
    } 
    else {
      analogWrite(RPWM_PIN1, 0); 
      analogWrite(LPWM_PIN1, 0);
    }

    // === FRONT LEFT ===
    if (FL > 0 && FL_ExtendedLimiterState == 0) {
      analogWrite(RPWM_PIN4, speed); 
      analogWrite(LPWM_PIN4, 0);
    } 
    else if (FL < 0 && FL_RetractedLimiterState == 0) {
      analogWrite(RPWM_PIN4, 0);  
      analogWrite(LPWM_PIN4, speed);
    } 
    else {
      analogWrite(RPWM_PIN4, 0);  
      analogWrite(LPWM_PIN4, 0);
    }

    // === BACK RIGHT ===
    if (BR > 0 && BR_ExtendedLimiterState == 0) {
      analogWrite(RPWM_PIN3, speed); 
      analogWrite(LPWM_PIN3, 0);
    } 
    else if (BR < 0 && BR_RetractedLimiterState == 0) {
      analogWrite(RPWM_PIN3, 0);  
      analogWrite(LPWM_PIN3, speed);
    } 
    else {
      analogWrite(RPWM_PIN3, 0);  
      analogWrite(LPWM_PIN3, 0);
    }

    // === BACK LEFT ===
    if (BL > 0 && BL_ExtendedLimiterState == 0) {
      analogWrite(RPWM_PIN2, speed); 
      analogWrite(LPWM_PIN2, 0);
    } 
    else if (BL < 0 && BL_RetractedLimiterState == 0) {
      analogWrite(RPWM_PIN2, 0);  
      analogWrite(LPWM_PIN2, speed);
    } 
    else {
      analogWrite(RPWM_PIN2, 0);  
      analogWrite(LPWM_PIN2, 0);
    }

    // Stop if level
    if (abs(pitch) < levelThreshold && abs(roll) < levelThreshold) {
      Serial.println("Platform leveled.");
      break;
    }
    delay(1000);
  }
  
  // Stop all actuators
  analogWrite(RPWM_PIN1, 0);
  analogWrite(LPWM_PIN1, 0);
  analogWrite(RPWM_PIN2, 0);
  analogWrite(LPWM_PIN2, 0);
  analogWrite(RPWM_PIN3, 0);
  analogWrite(LPWM_PIN3, 0);
  analogWrite(RPWM_PIN4, 0);
  analogWrite(LPWM_PIN4, 0);

  Serial.println("Leveling complete or timeout reached.");
}

// *** ORIGINAL tensionPlatform function - UNCHANGED ***
void tensionPlatform() {
    // Read limit switches
  int BR_ExtendedLimiterState = digitalRead(BR_ExtendedLimiter);
  int BR_RetractedLimiterState = digitalRead(BR_RetractedLimiter);
  int BL_ExtendedLimiterState = digitalRead(BL_ExtendedLimiter);
  int BL_RetractedLimiterState = digitalRead(BL_RetractedLimiter);
  int FR_ExtendedLimiterState = digitalRead(FR_ExtendedLimiter);
  int FR_RetractedLimiterState = digitalRead(FR_RetractedLimiter);
  int FL_ExtendedLimiterState = digitalRead(FL_ExtendedLimiter);
  int FL_RetractedLimiterState = digitalRead(FL_RetractedLimiter);

  while(FR_ExtendedLimiterState == 0 || FL_ExtendedLimiterState == 0 || BR_RetractedLimiterState == 0 || BL_RetractedLimiterState == 0){
    // Read limit switches
    BR_ExtendedLimiterState = digitalRead(BR_ExtendedLimiter);
    BR_RetractedLimiterState = digitalRead(BR_RetractedLimiter);
    BL_ExtendedLimiterState = digitalRead(BL_ExtendedLimiter);
    BL_RetractedLimiterState = digitalRead(BL_RetractedLimiter);
    FR_ExtendedLimiterState = digitalRead(FR_ExtendedLimiter);
    FR_RetractedLimiterState = digitalRead(FR_RetractedLimiter);
    FL_ExtendedLimiterState = digitalRead(FL_ExtendedLimiter);
    FL_RetractedLimiterState = digitalRead(FL_RetractedLimiter);

    if(FR_ExtendedLimiterState == 0){
      analogWrite(RPWM_PIN1, speed);
      analogWrite(LPWM_PIN1, 0);
    }
    else{
      analogWrite(RPWM_PIN1, 0);
      analogWrite(LPWM_PIN1, 0);
    }

    if(FL_ExtendedLimiterState == 0){
      analogWrite(RPWM_PIN4, speed);
      analogWrite(LPWM_PIN4, 0);
    }
    else{
      analogWrite(RPWM_PIN4, 0);
      analogWrite(LPWM_PIN4, 0);
    }

    if(BR_RetractedLimiterState == 0){
      analogWrite(RPWM_PIN3, 0);
      analogWrite(LPWM_PIN3, speed);
    }
    else{
      analogWrite(RPWM_PIN3, 0);
      analogWrite(LPWM_PIN3, 0);
    }
    if(BL_RetractedLimiterState == 0){
      analogWrite(RPWM_PIN2, 0);
      analogWrite(LPWM_PIN2, speed);

    }
    else{
      analogWrite(RPWM_PIN2, 0);
      analogWrite(LPWM_PIN2, 0);
    }
  }
}

void setup() {
  Serial.begin(115200);

  delay(1000);  // Stabilize serial connection

  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip");
    while (1) delay(10);
  }

  // Capture initial offsets
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  GyroXOffset = g.gyro.x;
  GyroYOffset = g.gyro.y;
  GyroZOffset = g.gyro.z;

  // Set pin modes
  pinMode(RPWM_PIN1, OUTPUT);
  pinMode(LPWM_PIN1, OUTPUT);
  pinMode(R_EN_PIN1, OUTPUT);
  pinMode(L_EN_PIN1, OUTPUT);
  pinMode(RPWM_PIN2, OUTPUT);
  pinMode(LPWM_PIN2, OUTPUT);
  pinMode(R_EN_PIN2, OUTPUT);
  pinMode(L_EN_PIN2, OUTPUT);
  pinMode(RPWM_PIN3, OUTPUT);
  pinMode(LPWM_PIN3, OUTPUT);
  pinMode(R_EN_PIN3, OUTPUT);
  pinMode(L_EN_PIN3, OUTPUT);
  pinMode(RPWM_PIN4, OUTPUT);
  pinMode(LPWM_PIN4, OUTPUT);
  pinMode(R_EN_PIN4, OUTPUT);
  pinMode(L_EN_PIN4, OUTPUT);

  // Enable motor driver channels
  digitalWrite(R_EN_PIN1, HIGH);
  digitalWrite(L_EN_PIN1, HIGH);
  digitalWrite(R_EN_PIN2, HIGH);
  digitalWrite(L_EN_PIN2, HIGH);
  digitalWrite(R_EN_PIN3, HIGH);
  digitalWrite(L_EN_PIN3, HIGH);
  digitalWrite(R_EN_PIN4, HIGH);
  digitalWrite(L_EN_PIN4, HIGH);

  pinMode(BL_ExtendedLimiter, INPUT_PULLUP);
  pinMode(BL_RetractedLimiter, INPUT_PULLUP);
  pinMode(BR_RetractedLimiter, INPUT_PULLUP);
  pinMode(BR_ExtendedLimiter, INPUT_PULLUP);
  pinMode(FL_ExtendedLimiter, INPUT_PULLUP);
  pinMode(FL_RetractedLimiter, INPUT_PULLUP);
  pinMode(FR_RetractedLimiter, INPUT_PULLUP);
  pinMode(FR_ExtendedLimiter, INPUT_PULLUP);

  tensionPlatform();
  //delay(5000);
  levelPlatform();

  // *** CHANGED: Initialize timing variables ***
  lastValidTime = millis();
  lastReadyTime = millis();
  lastIMURead = millis();  // NEW: Initialize IMU timing
  systemReady = true;
  
  // Initialize filtered values
  updateIMU();  // Get initial readings
  filteredPitch = currentPitch;
  filteredRoll = currentRoll;

  Serial.println("READY");
}

void moveDir(float targetPitch, float targetRoll) {
  // Scale the target values (so plane's 30° becomes platform's 6°)
  float scaledTargetPitch = constrain(targetPitch * motionGain, -maxMotionAngle, maxMotionAngle);
  float scaledTargetRoll = constrain(targetRoll * motionGain, -maxMotionAngle, maxMotionAngle);
  
  // Calculate error (how far we are from target)
  float pitchError = scaledTargetPitch - currentPitch;
  float rollError = scaledTargetRoll - currentRoll;
  
  // Apply deadband to prevent jittery motion
  if (abs(pitchError) < motionDeadband) pitchError = 0;
  if (abs(rollError) < motionDeadband) rollError = 0;
  
  // More aggressive error scaling for better response
  float maxTilt = 15.0;  // Max expected error for normalization
  float pitchEffect = constrain(pitchError / maxTilt, -1.0, 1.0);
  float rollEffect = constrain(rollError / maxTilt, -1.0, 1.0);
  
  // CORRECTED: Map to actuator commands (flipped pitch signs)
  float FR_cmd = +pitchEffect + rollEffect;   // Front Right (was -pitchEffect)
  float FL_cmd = +pitchEffect - rollEffect;   // Front Left  (was -pitchEffect)
  float BR_cmd = -pitchEffect + rollEffect;   // Back Right  (was +pitchEffect)
  float BL_cmd = -pitchEffect - rollEffect;   // Back Left   (was +pitchEffect)
  
  // Convert to discrete commands with threshold
  const float moveThreshold = 0.1;
  int FR = (FR_cmd > moveThreshold) ? 1 : (FR_cmd < -moveThreshold) ? -1 : 0;
  int FL = (FL_cmd > moveThreshold) ? 1 : (FL_cmd < -moveThreshold) ? -1 : 0;
  int BR = (BR_cmd > moveThreshold) ? 1 : (BR_cmd < -moveThreshold) ? -1 : 0;
  int BL = (BL_cmd > moveThreshold) ? 1 : (BL_cmd < -moveThreshold) ? -1 : 0;
  
  // Update global actuator effects
  FR_Actuator_effect = FR;
  FL_Actuator_effect = FL;
  BR_Actuator_effect = BR;
  BL_Actuator_effect = BL;
  
  // Debug output with error values
  Serial.print("Target: P="); Serial.print(scaledTargetPitch, 1);
  Serial.print(" R="); Serial.print(scaledTargetRoll, 1);
  Serial.print(" | Current: P="); Serial.print(currentPitch, 1);
  Serial.print(" R="); Serial.print(currentRoll, 1);
  Serial.print(" | Error: P="); Serial.print(pitchError, 1);
  Serial.print(" R="); Serial.print(rollError, 1);
  Serial.print(" | Cmds: FR="); Serial.print(FR);
  Serial.print(" FL="); Serial.print(FL);
  Serial.print(" BR="); Serial.print(BR);
  Serial.print(" BL="); Serial.println(BL);
}

void executeActuatorCommands() {
  // Read limit switches
  int BR_ExtendedLimiterState = digitalRead(BR_ExtendedLimiter);
  int BR_RetractedLimiterState = digitalRead(BR_RetractedLimiter);
  int BL_ExtendedLimiterState = digitalRead(BL_ExtendedLimiter);
  int BL_RetractedLimiterState = digitalRead(BL_RetractedLimiter);
  int FR_ExtendedLimiterState = digitalRead(FR_ExtendedLimiter);
  int FR_RetractedLimiterState = digitalRead(FR_RetractedLimiter);
  int FL_ExtendedLimiterState = digitalRead(FL_ExtendedLimiter);
  int FL_RetractedLimiterState = digitalRead(FL_RetractedLimiter);

  // Execute actuator commands
  // FR
  if(FR_Actuator_effect > 0 && FR_ExtendedLimiterState == 0){
    analogWrite(RPWM_PIN1, speed); 
    analogWrite(LPWM_PIN1, 0);   
  } else if (FR_Actuator_effect < 0 && FR_RetractedLimiterState == 0){
    analogWrite(RPWM_PIN1, 0); 
    analogWrite(LPWM_PIN1, speed); 
  } else {
    analogWrite(RPWM_PIN1, 0);   
    analogWrite(LPWM_PIN1, 0); 
  }

  // FL  
  if(FL_Actuator_effect > 0 && FL_ExtendedLimiterState == 0){
    analogWrite(RPWM_PIN4, speed); 
    analogWrite(LPWM_PIN4, 0); 
  } else if (FL_Actuator_effect < 0 && FL_RetractedLimiterState == 0){
    analogWrite(RPWM_PIN4, 0); 
    analogWrite(LPWM_PIN4, speed); 
  } else {
    analogWrite(RPWM_PIN4, 0); 
    analogWrite(LPWM_PIN4, 0); 
  }

  // BR
  if(BR_Actuator_effect > 0 && BR_ExtendedLimiterState == 0){
    analogWrite(RPWM_PIN3, speed); 
    analogWrite(LPWM_PIN3, 0);   
  } else if (BR_Actuator_effect < 0 && BR_RetractedLimiterState == 0){
    analogWrite(RPWM_PIN3, 0);   
    analogWrite(LPWM_PIN3, speed); 
  } else {
    analogWrite(RPWM_PIN3, 0);   
    analogWrite(LPWM_PIN3, 0); 
  }

  // BL
  if(BL_Actuator_effect > 0 && BL_ExtendedLimiterState == 0){
    analogWrite(RPWM_PIN2, speed); 
    analogWrite(LPWM_PIN2, 0);   
  } else if (BL_Actuator_effect < 0 && BL_RetractedLimiterState == 0){
    analogWrite(RPWM_PIN2, 0);   
    analogWrite(LPWM_PIN2, speed); 
  } else {
    analogWrite(RPWM_PIN2, 0);   
    analogWrite(RPWM_PIN2, 0); 
  }
}

void loop() {
  // NEW: Continuously update IMU readings
  if (millis() - lastIMURead >= imuReadInterval) {
    updateIMU();
    lastIMURead = millis();
  }

  // *** UPDATED: Process FlightGear commands ***
  if (Serial.available() > 0) {  
    leveling = false;
    
    String input = Serial.readStringUntil('\n');  
    input.trim();  

    int commaIndex = input.indexOf(',');

    if (commaIndex > 0) {
      String pitchStr = input.substring(0, commaIndex);
      String rollStr = input.substring(commaIndex + 1);

      // Get target values from FlightGear
      targetPitch = pitchStr.toFloat();
      targetRoll = rollStr.toFloat();
      
      // Use the improved moveDir function
      moveDir(targetPitch, targetRoll);
      lastValidTime = millis(); 

      // Execute actuator commands
      executeActuatorCommands();

      // Send ACK
      Serial.println("ACK");
      
    } else {
      Serial.println("ERROR: Invalid format"); 
    }  
  }
  
  // NEW: Continue to execute actuator commands even when no new FlightGear data
  // This ensures the platform keeps trying to reach the target position
  else if (!leveling && (millis() - lastValidTime) < failSafeDelayMs) {
    // Continue moving toward the last received target
    if (targetPitch != 0.0 || targetRoll != 0.0) {
      moveDir(targetPitch, targetRoll);
      executeActuatorCommands();
    }
  }

  // Fail-safe: Level platform if no valid data for too long
  if (!leveling && millis() - lastValidTime > failSafeDelayMs) {
      levelPlatform();
      leveling = true;
      // Reset targets when entering fail-safe mode
      targetPitch = 0.0;
      targetRoll = 0.0;
  }
  
  // Small delay to prevent overwhelming the system
  delay(10);
}