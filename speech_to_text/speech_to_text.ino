#include <I2S.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <FS.h>

const char* ssid = "JioFiber-Bks";
const char* password = "Home@3754";
const char* deepgramApiKey = "7c4dcb073d9fb6c1e271089b812b34322d67149b";

#define SAMPLE_RATE     16000
#define BITS_PER_SAMPLE 16
#define CHANNELS        1
#define RECORD_TIME_SEC 10
#define FILE_NAME       "/recorded.wav"

void writeWavHeader(File file, int dataLen) {
  uint8_t header[44] = {
    'R','I','F','F', 0,0,0,0,'W','A','V','E','f','m','t',' ',
    16,0,0,0, 1,0, 1,0, 0,0,0,0, 0,0, 0,0,
    2,0, 16,0,'d','a','t','a', 0,0,0,0
  };

  int byteRate = SAMPLE_RATE * CHANNELS * (BITS_PER_SAMPLE / 8);
  int fileSize = dataLen + 36;

  header[4]  = fileSize;
  header[5]  = fileSize >> 8;
  header[6]  = fileSize >> 16;
  header[7]  = fileSize >> 24;

  header[24] = SAMPLE_RATE;
  header[25] = SAMPLE_RATE >> 8;
  header[26] = SAMPLE_RATE >> 16;
  header[27] = SAMPLE_RATE >> 24;

  header[28] = byteRate;
  header[29] = byteRate >> 8;
  header[30] = byteRate >> 16;
  header[31] = byteRate >> 24;

  header[40] = dataLen;
  header[41] = dataLen >> 8;
  header[42] = dataLen >> 16;
  header[43] = dataLen >> 24;

  file.write(header, 44);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  if (!SPIFFS.begin(true)) {
    Serial.println("❌ SPIFFS mount failed");
    return;
  }

  Serial.print("📶 Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi connected");

  Serial.println("🎙 Starting mic...");
  I2S.setAllPins(-1, 42, 41, -1, -1); // BCK=-1, WS=42, SD=41
  if (!I2S.begin(PDM_MONO_MODE, SAMPLE_RATE, BITS_PER_SAMPLE)) {
    Serial.println("❌ I2S init failed");
    while (1);
  }

  File file = SPIFFS.open(FILE_NAME, FILE_WRITE);
  if (!file) {
    Serial.println("❌ Failed to open file");
    return;
  }

  int totalBytes = SAMPLE_RATE * (BITS_PER_SAMPLE / 8) * RECORD_TIME_SEC;
  int16_t buffer[512];
  int bytesWritten = 0;

  Serial.println("🎤 Recording...");
  writeWavHeader(file, totalBytes);

  unsigned long startTime = millis();
  while ((millis() - startTime) < (RECORD_TIME_SEC * 1000)) {
    int bytesRead = I2S.read(buffer, sizeof(buffer));
    if (bytesRead > 0) {
      file.write((uint8_t*)buffer, bytesRead);
      bytesWritten += bytesRead;
    }
  }

  file.close();
  I2S.end();
  Serial.println("✅ Recording complete");

  File audioFile = SPIFFS.open(FILE_NAME);
  if (!audioFile) {
    Serial.println("❌ Audio file open failed");
    return;
  }

  HTTPClient http;
  http.begin("https://api.deepgram.com/v1/listen");
  http.addHeader("Authorization", String("Token ") + deepgramApiKey);
  http.addHeader("Content-Type", "audio/wav");

  int httpResponseCode = http.sendRequest("POST", &audioFile, audioFile.size());

  if (httpResponseCode > 0) {
    String response = http.getString();

    int start = response.indexOf("\"transcript\":\"") + 14;
    int end = response.indexOf("\"", start);
    String transcript = response.substring(start, end);

    Serial.println("🗣 Recognized Speech:");
    Serial.println(transcript);
  } else {
    Serial.print("❌ HTTP Error: ");
    Serial.println(httpResponseCode);
  }

  audioFile.close();
  http.end();
}

void loop() {
  // idle
  delay(2000);
  setup();
}
