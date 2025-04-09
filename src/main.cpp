/************************************************************
   Water pressure sensor demo(Computer serial port)
   - Obtain the water pressure through the output voltage
     of the sensor.
**************************************************************/

/************************************************************
  Water Sensor Key Parameter
  - Parts No.:KY-3-5
  - Sensing range: 0 - 1 MPa
  - Input Voltage: 5VDC
  - Output Voltage: 0.5 - 4.5 VDC
    (Linearly corresponding to 0 - 1 MPa)
  - Accuary: 0.5% - 1% FS
**************************************************************/

/************************************************************
  Water Sensor Calibration

  The output voltage offset of the sensor is 0.5V (norminal).
  However, due to the zero-drifting of the internal circuit, the
  no-load output voltage is not exactly 0.5V. Calibration needs to
  be carried out as follow.

  Calibration: connect the 3 pin wire to the Arduio UNO (VCC, GND and Signal)
  without connecting the sensor to the water pipe and run the program
  for once. Mark down the LOWEST voltage value through the serial
  monitor and revise the "OffSet" value to complete the calibration.

  After the calibration the sensor is ready for measuring!
**************************************************************/

#include <Arduino.h>
#include "RunningAverage.h"

#define SENSOR_PIN A0
#define NUM_SAMPLES 1
#define CALIBRATION_SAMPLES 30
// RunningAverage myRA(NUM_SAMPLES);
int samples = 0;
float OFFSET = 0.6116746902;
const float PSI_CONV = 0.0001450377;

void do_calibration(float knownPressurePsi) {
  // Serial.print("Start calibration");
  int value = 0;
  for (size_t i = 0; i < CALIBRATION_SAMPLES; i++) {
    value += analogRead(SENSOR_PIN);
    // Serial.print(".");
    delay(1000);
  }
  // Serial.println("Done");
  float averageValue = ((float)value / CALIBRATION_SAMPLES) * 5.00 / 1024.0;
  float knownPressurePa = knownPressurePsi / PSI_CONV; // Convert PSI to Pascals
  OFFSET = averageValue - (knownPressurePa / 300000.0); // Adjust OFFSET calculation
  Serial.print("cal");
  Serial.println(OFFSET, 10);
}

void setup() {
  pinMode(SENSOR_PIN, INPUT);
  Serial.begin(9600);
  // float knownPressurePsi = 58.0; // Replace with the known pressure value in PSI from the manometer
  // do_calibration(knownPressurePsi);
  // myRA.clear(); // explicitly start clean
}

void loop() {
  int valueRaw = analogRead(SENSOR_PIN);
  // myRA.addValue(valueRaw);
  if (samples >= NUM_SAMPLES) {
    float valueVolts = valueRaw * 5.00 / 1024;
    float valuePa = (3.0 * (valueVolts - OFFSET)) * 100000.0;
    float valuePsi = valuePa * PSI_CONV;
    Serial.print("vol");
    Serial.print(valueVolts, 10);
    Serial.print(";pas");
    Serial.print(valuePa, 10);
    Serial.print(";psi");
    Serial.println(valuePsi, 10);
    // Serial.println("psi");
  } else {
    samples++;
  }

  delay(60000);
}