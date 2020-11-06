#include <OneWire.h>
#include <DallasTemperature.h>

#define WATER_PIN D1
#define MOTOR_PIN D2
#define FAST_PIN D6
#define WATER_BTN_PIN D3
#define MOTOR_BTN_PIN D4
#define FAST_BTN_PIN D7
#define ONE_WIRE_BUS D5
#define INTERVAL 2000

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);

class Thermostat {
  private:
    float current_temperature = 10;
    float desired_temperature = 24;
    bool autonomous = true;
    bool water_relay = false;
    bool motor_relay = false;
    bool fast_relay = false;
    long previousMillis = 0;

    void read_temperature_sensor() {
      sensors.requestTemperatures();
      current_temperature = round(sensors.getTempCByIndex(0)*2)/2.0;
      Serial.print("Temp: ");
      Serial.println(current_temperature);
    }
    void write_relay_states() {
      digitalWrite(FAST_PIN, fast_relay ? LOW : HIGH);
      digitalWrite(MOTOR_PIN, motor_relay ? LOW : HIGH);
      digitalWrite(WATER_PIN, water_relay ? LOW : HIGH);
    }
    void compute_relay_states(float error) {
      if(!autonomous)
        return;
        
      if(error >= 2) {
        water_relay = 0;
        motor_relay = 0;
        fast_relay = 0;
      }
      else if (error <= -2) {
        water_relay = 1;
        motor_relay = 1;
        fast_relay = 0;
      }
    }
    
  public:
    void setDesiredTemp(float temp) {
      desired_temperature = temp;
    }
    void setAuto(bool auto_v) {
      this->autonomous = auto_v;
    }

    void setRelayState(char* relay, bool state) {
      if(relay == "water")
        this->water_relay = state;
      if(relay == "motor")
        this->motor_relay = state;
      if(relay == "fast")
        this->fast_relay = state;
    }

    float getCurrentTemp() {
      return this->current_temperature++;
    }
    
    float getDesiredTemp() {
      return this->desired_temperature;
    }

    bool getRelayState(char* relay) {
      if(relay == "water")
        return this->water_relay;
      if(relay == "motor")
        return this->motor_relay;
      if(relay == "fast")
        return this->fast_relay;

      return false;
    }

    void setup() {
      pinMode(WATER_PIN, OUTPUT);
      pinMode(MOTOR_PIN, OUTPUT);
      pinMode(FAST_PIN, OUTPUT);
      
      pinMode(WATER_BTN_PIN, INPUT_PULLUP);
      pinMode(MOTOR_BTN_PIN, INPUT_PULLUP);
      pinMode(FAST_BTN_PIN, INPUT_PULLUP);

      digitalWrite(WATER_PIN, LOW);
      digitalWrite(MOTOR_PIN, LOW);
      digitalWrite(FAST_PIN, LOW);

      sensors.begin();
    }

    void loop() {
      unsigned long currentMillis = millis();
 
      if(currentMillis - previousMillis > INTERVAL) {
        previousMillis = currentMillis;   
        read_temperature_sensor();
        float error = desired_temperature - current_temperature;
        compute_relay_states(error);
      }

      if(digitalRead(WATER_BTN_PIN) == LOW) {
        while(digitalRead(WATER_BTN_PIN) == LOW);
        this->setRelayState("water", !this->getRelayState("water"));
      }
      if(digitalRead(MOTOR_BTN_PIN) == LOW) {
        while(digitalRead(MOTOR_BTN_PIN) == LOW);
        this->setRelayState("motor", !this->getRelayState("motor"));
      }
      if(digitalRead(FAST_BTN_PIN) == LOW) {
        while(digitalRead(FAST_BTN_PIN) == LOW);
        this->setRelayState("fast", !this->getRelayState("fast"));
      }
      
      write_relay_states();
    }
};
