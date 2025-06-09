#include <Adafruit_Sensor.h>
#include <Adafruit_MPU6050.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// === FES & MPU6050 ===
#define FES_PIN1            23
#define FES_PIN2            25
#define MPU_ADDR            0x68
#define INT_PIN             27

// === EMG Sampling ===
#define emgPin                     34
#define EMG_SAMPLE_RATE_HZ         500
#define EMG_SAMPLE_INTERVAL_US     (1000000UL / EMG_SAMPLE_RATE_HZ)
#define MAX_EMG_SAMPLES            200

// === WiFi Config ===
const char* sta_ssid     = "farah";  
const char* sta_password = "password";
const char* backendURL   = "http://192.168.89.90:8000/api/emg-data";

WebServer server(80);

// === FES parameters ===
uint8_t      pulse_frequency      = 10;
unsigned long train_duration_ms   = 200;

// === MPU6050 ===
Adafruit_MPU6050 mpu;
float gyro_offsets[3] = {0, 0, 0};

// === FES state ===
bool fesActive = false;
unsigned long fesStartTime_us = 0;
unsigned long fesNextToggleTime_us = 0;
unsigned long fesHalfPulse_us = 0;
int fesTotalHalfPulses = 0;
int fesHalfPulseCount = 0;

// === EMG sampling ===
struct EMGSample {
    unsigned long timestamp;
    int value;
};
EMGSample emg_samples[MAX_EMG_SAMPLES];
int emg_sample_count = 0;
bool emgSamplingActive = false;
unsigned long emgStartTime_us = 0;
unsigned long emgNextSampleTime_us = 0;
unsigned long emgDuration_ms = 0;

// === Step tracking ===
uint32_t daily_step_count = 0;
unsigned long last_step_time = 0;
uint32_t DAILY_STEP_TARGET = 800;         // Hardcoded daily goal (adjust as needed)
unsigned long session_start_time = 0;     // When device was turned on
const unsigned long MAX_SESSION_TIME = 3600000; // 1 hour = 3,600,000 milliseconds
bool wasWiFiConnected = false;
unsigned long block_start_time = 0;
bool session_blocked = false;


// === Gyro Detection ===
struct Gyro_Detect {
    uint8_t intPin;
    float threshold;
    uint8_t axis;
    volatile bool triggered;
};
Gyro_Detect detect = {
    .intPin    = INT_PIN,
    .threshold = 1.0f,
    .axis      = 1,  // Y-axis
    .triggered = false
};

// ISR
void IRAM_ATTR gyroISR(void* arg) {
    Gyro_Detect* w = static_cast<Gyro_Detect*>(arg);
    w->triggered = true;
}

// === Auto-sync timing ===
unsigned long last_data_send = 0;
const unsigned long SEND_INTERVAL = 60000; // 1 min

//=== Declarations ===
void setup_wifi_station();
void send_data_backend();
void gyro_calibration(unsigned long calibration_time) ;
void sample_rate();
void data_rdy_enable() ;
void start_fes_train_nonblocking(uint8_t frequency, unsigned long duration_ms) ;
void begin_emg_sampling(unsigned long duration_ms);
void show_status() ;
void save_locally() ;
void upload_local_data() ;

void setup() {
    Serial.begin(115200);

    setup_wifi_station();

    Serial.println("MPU6050 Init...");
    Wire.begin();

    if (!mpu.begin()) {
        Serial.println("Failed to find MPU6050 chip");
        while (1) delay(10);
    }

    Serial.println("MPU6050 Found!");
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

    sample_rate();
    data_rdy_enable();

    pinMode(detect.intPin, INPUT_PULLUP);
    attachInterruptArg(
        digitalPinToInterrupt(detect.intPin),
        gyroISR,
        &detect,
        RISING
    );

    gyro_calibration(3);

    // FES pins
    pinMode(FES_PIN1, OUTPUT);
    digitalWrite(FES_PIN1, LOW);
    pinMode(FES_PIN2, OUTPUT);
    digitalWrite(FES_PIN2, LOW);

    // EMG input
    pinMode(emgPin, INPUT);

    Serial.println("Device ready!");

    session_start_time = millis();  // Record when device started
    
    Serial.println("=== StimStep Safety System ===");
    Serial.print("Daily step target: ");
    Serial.println(DAILY_STEP_TARGET);
    Serial.println("Max session: 1 hour");
}

void loop() {
    server.handleClient();

    // === EMG Sampling ===
    if (emgSamplingActive) {
        unsigned long now_us = micros();
        if ((now_us - emgStartTime_us) >= (emgDuration_ms * 1000UL)) {
            emgSamplingActive = false;
            Serial.println("EMG sampling complete");
        } else if (now_us >= emgNextSampleTime_us) {
            int rawVal = analogRead(emgPin);
            if (emg_sample_count < MAX_EMG_SAMPLES) {
                emg_samples[emg_sample_count].timestamp = now_us;
                emg_samples[emg_sample_count].value = rawVal;
                emg_sample_count++;
            }
            emgNextSampleTime_us = now_us + EMG_SAMPLE_INTERVAL_US;
        }
    }

    // === FES pulse train ===
    if (fesActive) {
        unsigned long now_us = micros();
        if (now_us >= fesNextToggleTime_us) {
            if (fesHalfPulseCount < fesTotalHalfPulses) {
                if (fesHalfPulseCount % 2 == 0) {
                    digitalWrite(FES_PIN1, HIGH);
                    digitalWrite(FES_PIN2, HIGH);
                } else {
                    digitalWrite(FES_PIN1, LOW);
                    digitalWrite(FES_PIN2, LOW);
                }
                fesHalfPulseCount++;
                fesNextToggleTime_us = now_us + fesHalfPulse_us;
            } else {
                digitalWrite(FES_PIN1, LOW);
                digitalWrite(FES_PIN2, LOW);
                fesActive = false;
            }
        }
    }

    // === Gyro trigger ===
    if (detect.triggered) {
        detect.triggered = false;

        sensors_event_t a, g, temp;
        mpu.getEvent(&a, &g, &temp);

        float rate = ((detect.axis == 0 ? g.gyro.x :
                      (detect.axis == 1 ? g.gyro.y : g.gyro.z))
                      - gyro_offsets[detect.axis]);

    if (fabs(rate) > detect.threshold) {
            
        // === SIMPLE SAFETY CHECKS ===
        bool can_stimulate = true;
        String block_reason = "";
        
        // Check daily target
        if (daily_step_count >= DAILY_STEP_TARGET) {
            session_blocked = true;
            can_stimulate = false;
            block_reason = "Daily target reached!";
        }
        
        //Check session time (1 hour max)
        if (!session_blocked) {
        unsigned long session_time = millis() - session_start_time;
        if (session_time >= MAX_SESSION_TIME) {
            can_stimulate = false;
            block_reason = "1 hour session limit reached!";
            session_blocked = true;
            block_start_time = millis();
         }
        } 
        else {
        if (millis() - block_start_time >= 3600000UL) {
            can_stimulate = true;
            session_start_time = millis();
            session_blocked = false;
            block_reason = "";
         }
        }
        //Check WiFi 
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("WiFi disconnected (data won't sync)");
            save_locally();
        }
        
        if (WiFi.status() == WL_CONNECTED && !wasWiFiConnected) 
        {
        Serial.println("📶 Wi-Fi just reconnected.");
        upload_local_data();
        wasWiFiConnected = true;
        }

        if (WiFi.status() != WL_CONNECTED) 
        {
            wasWiFiConnected = false;
        }

        // COUNT THE STEP REGARDLESS
        daily_step_count++;
        last_step_time = millis();
        
        // STIMULATE ONLY IF SAFE
        if (can_stimulate) {
            start_fes_train_nonblocking(pulse_frequency, train_duration_ms);
            begin_emg_sampling(train_duration_ms);
        
            Serial.print("Step ");
            Serial.print(daily_step_count);
            Serial.print("/");
            Serial.print(DAILY_STEP_TARGET);
            Serial.println(" - Stimulation delivered");
            
            // Warn when close to target
            if (daily_step_count >= DAILY_STEP_TARGET - 50) {
                Serial.print("Approaching target! ");
                Serial.print(DAILY_STEP_TARGET - daily_step_count);
                Serial.println(" steps remaining");
            }
            
        } else {
            Serial.print("Step ");
            Serial.print(daily_step_count);
            Serial.print(" counted but NO stimulation: ");
            Serial.println(block_reason);
        }
    }
}

    // === Simple status display every 30 seconds ===
    static unsigned long last_status = 0;
    if (millis() - last_status > 30000) {  // 30 seconds
        last_status = millis();
        show_status();
    }

    // === Auto-send data ===
    unsigned long now = millis();
    if (now - last_data_send >= SEND_INTERVAL) {
        send_data_backend();
        last_data_send = now;
    }

    // === Simple WiFi check ===
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi lost, reconnecting...");
        WiFi.disconnect();
        WiFi.begin(sta_ssid, sta_password);
        while (WiFi.status() != WL_CONNECTED) {
            delay(1000);
            Serial.println("Reconnecting to WiFi...");
        }
        Serial.println("Reconnected!");
    }

    delay(1);
}

// === WiFi Setup ===
void setup_wifi_station() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(sta_ssid, sta_password);
    Serial.print("Connecting to Wi-Fi ");
    Serial.println(sta_ssid);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected to WiFi");
    Serial.print("Station IP: ");
    Serial.println(WiFi.localIP());
}

// === Send data to backend ===
void send_data_backend() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected - cannot send data");
        return;
    }

    // Don't send if no new data
    if (emg_sample_count == 0 && daily_step_count == 0) {
        Serial.println("No new data to send");
        return;
    }

    Serial.println("Preparing data to send...");

    DynamicJsonDocument doc(8192);
    doc["device_id"] = "StimStep_001";
    doc["daily_steps"] = daily_step_count;
    doc["daily_target"] = DAILY_STEP_TARGET;
    doc["target_reached"] = daily_step_count >= DAILY_STEP_TARGET;
    doc["last_step_time"] = last_step_time;
    doc["session_minutes"] = (millis() - session_start_time) / 60000;
    doc["timestamp"] = millis();
    
    JsonArray emg_array = doc.createNestedArray("emg_samples");
    for (int i = 0; i < emg_sample_count; i++) {
        JsonObject sample = emg_array.createNestedObject();
        sample["timestamp"] = emg_samples[i].timestamp;
        sample["value"] = emg_samples[i].value;
    }

    String jsonPayload;
    serializeJson(doc, jsonPayload);

    HTTPClient http;
    http.begin(backendURL);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(10000); // 10 second timeout

    int responseCode = http.POST(jsonPayload);

    if (responseCode > 200) {
        Serial.print("Data sent successfully. Response: ");
        Serial.println(responseCode);
        emg_sample_count = 0;
        Serial.println("EMG buffer cleared");
    } else {
        Serial.print("Error sending data: ");
        Serial.println(responseCode);
    }

    http.end();
}

// === Gyro Calibration ===
void gyro_calibration(unsigned long calibration_time) {
    float sum[3] = {0, 0, 0};
    unsigned long sample_count = 0;
    unsigned long start_time = millis();
    unsigned long end_time = start_time + calibration_time * 1000UL;

    Serial.print("Calibrating gyro...");
    while (millis() < end_time) {
        sensors_event_t a, g, temp;
        mpu.getEvent(&a, &g, &temp);

        sum[0] += g.gyro.x;
        sum[1] += g.gyro.y;
        sum[2] += g.gyro.z;
        sample_count++;
        delay(10);
    }

    for (int i = 0; i < 3; i++) {
        gyro_offsets[i] = (sample_count > 0) ? sum[i] / sample_count : 0;
    }

    Serial.println("Calibration done.");
}

// === Set sample rate ===
void sample_rate() {
    uint8_t sampleDiv = 9; // 100 Hz
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x19);
    Wire.write(sampleDiv);
    Wire.endTransmission();
}

// === Enable DATA_RDY ===
void data_rdy_enable() {
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x38);
    Wire.endTransmission(false);
    Wire.requestFrom(MPU_ADDR, (uint8_t)1);
    uint8_t cur = Wire.read();
    uint8_t updated = cur | 0x01;
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x38);
    Wire.write(updated);
    Wire.endTransmission();
}

// === FES ===
void start_fes_train_nonblocking(uint8_t frequency, unsigned long duration_ms) {
    fesHalfPulse_us = (1000000UL / frequency) / 2;
    int totalPulses = (duration_ms * frequency) / 1000;
    fesTotalHalfPulses = totalPulses * 2;
    fesHalfPulseCount = 0;
    fesStartTime_us = micros();
    fesNextToggleTime_us = fesStartTime_us;
    fesActive = true;
    Serial.println("FES started");
}

// === EMG ===
void begin_emg_sampling(unsigned long duration_ms) {
    emgDuration_ms = duration_ms;
    emgStartTime_us = micros();
    emgNextSampleTime_us = emgStartTime_us;
    emgSamplingActive = true;
    Serial.println("EMG sampling started");
}

// === Simple status display function ===
void show_status() {
    Serial.println("=== STATUS ===");
    
    // Steps
    Serial.print("Steps: ");
    Serial.print(daily_step_count);
    Serial.print("/");
    Serial.print(DAILY_STEP_TARGET);
    if (daily_step_count >= DAILY_STEP_TARGET) {
        Serial.println(" ✓ TARGET REACHED!");
    } else {
        Serial.print(" (");
        Serial.print(DAILY_STEP_TARGET - daily_step_count);
        Serial.println(" remaining)");
    }
    
    // Session time
    unsigned long session_minutes = (millis() - session_start_time) / 60000;
    Serial.print("Session: ");
    Serial.print(session_minutes);
    Serial.print("/60 minutes");
    if (session_minutes >= 60) {
        Serial.println("TIME LIMIT!");
    } else {
        Serial.println("");
    }
    
    // WiFi
    Serial.print("WiFi: ");
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Connected");
    } else {
        Serial.println("Disconnected");
    }
    
    Serial.println("==============");
}

void save_locally() {
  DynamicJsonDocument doc(512);
  doc["device_id"] = "StimStep_001";
  doc["daily_steps"] = daily_step_count;
  doc["daily_target"] = DAILY_STEP_TARGET;
  doc["target_reached"] = daily_step_count >= DAILY_STEP_TARGET;
  doc["last_step_time"] = last_step_time;
  doc["session_minutes"] = (millis() - session_start_time) / 60000;
  doc["timestamp"] = millis();

  File file = SPIFFS.open("/summary.json", FILE_WRITE);
  if (file) {
    serializeJson(doc, file);
    file.close();
    Serial.println("Summary saved locally.");
  } else {
    Serial.println("Failed to save summary.");
  }
}

void upload_local_data() {
  if (WiFi.status() != WL_CONNECTED) return;

  File file = SPIFFS.open("/summary.json", FILE_READ);
  if (!file || file.size() == 0) {
    Serial.println("ℹ️ No summary to upload.");
    return;
  }

  String json = file.readString();
  file.close();

  HTTPClient http;
  http.begin(backendURL)
  http.addHeader("Content-Type", "application/json");

  int httpCode = http.POST(json);
  if (httpCode == 200) {
    Serial.println("Summary uploaded successfully.");
    SPIFFS.remove("/summary.json");
  } else {
    Serial.print("Upload failed: ");
    Serial.println(httpCode);
  }

  http.end();
}

