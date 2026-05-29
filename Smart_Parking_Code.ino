//Eliyas Science Info www.youtube.com/c/eliyasscienceinfo
//IoT Smart Parking System
#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WebServer.h>
#include <ESP32Servo.h>

// LCD setup (16x2 I2C)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Hardware pins
#define IR_ENTRY 18
#define IR_EXIT 19
#define SERVO_PIN 25
#define GREEN_LED 26
#define RED_LED 27

Servo barrierServo;

// WiFi credentials
const char* ssid = "Eliyas";
const char* password = "0123456789";

// Create WebServer on port 80
WebServer server(80);

int totalSlots = 4;
int availableSlots = 4;
int flagEntry = 0;
int flagExit = 0;
String gateStatus = "Closed";

// ---------------- LCD and LED functions ----------------
void updateLCD() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Slots:");
  lcd.print(availableSlots);
  lcd.print("/");
  lcd.print(totalSlots);

  lcd.setCursor(0, 1);
  lcd.print("Gate:");
  lcd.print(gateStatus.c_str());

  // Control LEDs based on available slots
  if (availableSlots > 0) {
    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(RED_LED, LOW);
  } else {
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RED_LED, HIGH);
  }
}

void showFullMessage() {
  lcd.clear();
  lcd.setCursor(2, 0);
  lcd.print("Parking FULL!");

  // Turn on red LED when full
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, HIGH);

  delay(2000);
  updateLCD();
}

// Function to handle the /data request and send JSON response
void handleData() {
  String json = "{";
  json += "\"available\":" + String(availableSlots) + ",";
  json += "\"occupied\":" + String(totalSlots - availableSlots) + ",";
  json += "\"gate\":\"" + gateStatus + "\",";
  json += "\"occupied_slots\":[";
  int occ = totalSlots - availableSlots;
  for (int i = 1; i <= occ; i++) {
    json += String(i);
    if (i < occ) json += ",";
  }
  json += "]";
  json += "}";
  server.send(200, "application/json", json);
}

// ---------------- HTML Dashboard ----------------
String getHTML() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>IoT Smart Parking System</title>
  <script src="https://cdn.tailwindcss.com"></script>
  <style>
    @import url('https://fonts.googleapis.com/css2?family=Inter:wght@400;500;700&display=swap');
    body { font-family: 'Inter', sans-serif; }
    .glow-green { text-shadow: 0 0 5px #4ade80, 0 0 10px #4ade80; }
    .glow-red { text-shadow: 0 0 5px #f87171, 0 0 10px #f87171; }
    .glow-lime { text-shadow: 0 0 5px #a3e635, 0 0 10px #a3e635; }
    .border-glow-green { box-shadow: 0 0 10px #a3e635, 0 0 20px rgba(163,230,53,0.5); border-color: #a3e635; }
    .border-glow-red { box-shadow: 0 0 10px #f87171, 0 0 20px rgba(248,113,113,0.5); border-color: #f87171; }
    .card-bg { background-color: #161b22; }
    .body-bg { background-color: #3A0007; }
  </style>
</head>
<body class="body-bg text-white min-h-screen flex flex-col items-center justify-center p-4">

  <div class="text-center mb-8">
    <h1 class="text-4xl md:text-5xl font-extrabold text-teal-400 mb-2">IoT Smart Parking System</h1>
    <p class="text-gray-400 text-lg">Real-time parking management with ESP32</p>
  </div>

  <div class="w-full max-w-5xl mb-8">
    <div class="grid grid-cols-1 md:grid-cols-3 gap-4">

      <div class="card-bg border border-gray-700 rounded-3xl p-6 md:p-8 shadow-2xl flex flex-col items-start">
        <div class="flex items-center mb-2">
          <h2 class="text-xl font-semibold text-gray-200">Available Slots</h2>
        </div>
        <p id="available" class="text-5xl md:text-6xl font-bold text-green-400 glow-green">0</p>
        <p class="text-gray-400 text-sm mt-1">out of )rawliteral";
  html += String(totalSlots);
  html += R"rawliteral( total slots</p>
      </div>

      <div class="card-bg border border-gray-700 rounded-3xl p-6 md:p-8 shadow-2xl flex flex-col items-start">
        <div class="flex items-center mb-2">
          <h2 class="text-xl font-semibold text-gray-200">Occupied Slots</h2>
        </div>
        <p id="occupied" class="text-5xl md:text-6xl font-bold text-red-400 glow-red">0</p>
        <p class="text-gray-400 text-sm mt-1">currently parked</p>
      </div>

      <div class="card-bg border border-gray-700 rounded-3xl p-6 md:p-8 shadow-2xl flex flex-col items-start">
        <div class="flex items-center mb-2">
          <h2 class="text-xl font-semibold text-gray-200">Gate Status</h2>
        </div>
        <p id="gate" class="text-4xl md:text-5xl font-bold text-red-400 glow-red transition-colors duration-300">Closed</p>
        <p class="text-gray-400 text-sm mt-1">entry gate status</p>
      </div>

    </div>
  </div>

  <div class="w-full max-w-5xl card-bg border border-gray-700 rounded-3xl p-6 md:p-8 shadow-2xl mb-8 flex flex-col items-start">
    <div class="flex items-center mb-6">
      <h2 class="text-2xl md:text-3xl font-bold text-gray-200">Parking Layout - Real-time Status</h2>
    </div>
    <div id="slots" class="grid grid-cols-2 lg:grid-cols-4 gap-4 md:gap-6 w-full">
    </div>
  </div>

  <script>
    const totalSlots = )rawliteral" + String(totalSlots) + R"rawliteral(;
    async function updateData() {
      try {
        const response = await fetch('/data');
        const data = await response.json();

        document.getElementById("available").innerText = data.available;
        document.getElementById("occupied").innerText = data.occupied;

        const gateElement = document.getElementById("gate");
        gateElement.innerText = data.gate;
        if (data.gate === "Open") {
          gateElement.classList.remove("glow-red","text-red-400");
          gateElement.classList.add("glow-lime","text-lime-400");
        } else {
          gateElement.classList.remove("glow-lime","text-lime-400");
          gateElement.classList.add("glow-red","text-red-400");
        }

        const slotsContainer = document.getElementById("slots");
        let layoutHtml = "";
        for (let i = 1; i <= totalSlots; i++) {
          const isOccupied = data.occupied_slots.includes(i);
          const slotStatus = isOccupied ? "Occupied" : "Available";
          const slotClass = isOccupied ? "bg-red-900 border-glow-red" : "bg-green-900 border-glow-green";
          const statusSubtextClass = isOccupied ? "text-red-200" : "text-green-200";

          layoutHtml += `
            <div class="p-4 rounded-xl border-2 ${slotClass} flex flex-col items-center justify-center text-center slot">
              <div class="text-xl md:text-2xl font-bold text-gray-200">Slot ${i}</div>
              <div class="text-sm md:text-base font-semibold mt-1 ${statusSubtextClass}">${slotStatus}</div>
            </div>`;
        }
        slotsContainer.innerHTML = layoutHtml;
      } catch (e) {
        console.error("Fetch failed", e);
      }
    }
    setInterval(updateData, 2000);
    window.onload = updateData;
  </script>
</body>
</html>
)rawliteral";
  return html;
}

// ---------------- Setup ----------------
void setup() {
  Serial.begin(115200);
  pinMode(IR_ENTRY, INPUT);
  pinMode(IR_EXIT, INPUT);
  pinMode(GREEN_LED, OUTPUT); // Set LED pins as outputs
  pinMode(RED_LED, OUTPUT);

  barrierServo.attach(SERVO_PIN);
  barrierServo.write(90); // closed

  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Smart Parking");
  lcd.setCursor(0, 1);
  lcd.print("Connecting WiFi");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" Connected!");
  Serial.println(WiFi.localIP());

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi Connected");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP().toString().c_str());

  updateLCD(); // Initial call to set LCD and LED state

  server.on("/", []() {
    server.send(200, "text/html", getHTML());
  });

  server.on("/data", handleData);

  server.begin();
}

// ---------------- Loop ----------------
void loop() {
  server.handleClient();

  // Entry detection
  if (digitalRead(IR_ENTRY) == LOW && flagEntry == 0) {
    flagEntry = 1;

    // Turn on the red LED when a car is first detected
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RED_LED, HIGH);

    if (availableSlots > 0) {
      gateStatus = "Open";
      updateLCD();
      barrierServo.write(0);
      delay(2000);
      barrierServo.write(90);
      gateStatus = "Closed";
      availableSlots--;
      updateLCD(); 
    } else {
      showFullMessage();
    }
  }
  
  // Logic to return to the correct LED state after the car passes
  if (digitalRead(IR_ENTRY) == HIGH && flagEntry == 1) {
    flagEntry = 0;
    updateLCD(); // This will restore the LED state based on available slots
  }

  // Exit detection
  if (digitalRead(IR_EXIT) == LOW && flagExit == 0) {
    flagExit = 1;
    if (availableSlots < totalSlots) {
      gateStatus = "Open";
      updateLCD();
      barrierServo.write(0);
      delay(2000);
      barrierServo.write(90);
      gateStatus = "Closed";
      availableSlots++;
      updateLCD();
    }
  }
  if (digitalRead(IR_EXIT) == HIGH) flagExit = 0;
}
