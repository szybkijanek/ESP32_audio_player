#include <WiFi.h>
#include <SPI.h>
#include <SdFat.h>         // do listowania MP3
#include <SD.h>            // do odtwarzania przez audio.h
#include <Audio.h>
#include <GyverEncoder.h>

#define I2S_BCK_PIN    26
#define I2S_WS_PIN     25
#define I2S_DOUT_PIN   27
#define SD_CS          5

#define ENC_CLK_PIN    32
#define ENC_DT_PIN     33
#define ENC_BUTTON_PIN 35

const char* ssid = "";
const char* password = "";

Audio audio;
Encoder enc(ENC_CLK_PIN, ENC_DT_PIN, ENC_BUTTON_PIN);

SdFat sdFat;
std::vector<String> mp3Files;
int currentMp3Index = 0;

std::vector<String> radioStations;
int currentRadioIndex = 0;

bool playingFromSD = false;
bool volumeMode = false;
int volume = 21;

enum SourceType { SOURCE_SD, SOURCE_RADIO };
SourceType currentSource = SOURCE_RADIO;
const int totalSources = 2;

// ======================= LISTOWANIE MP3 przez SdFat ======================
void listMp3Files(const char* path) {
  mp3Files.clear();
  SdFile dir;
  if (!dir.open(path)) return;

  SdFile file;
  while (file.openNext(&dir, O_RDONLY)) {
    if (!file.isDir()) {
      char name[100];
      file.getName(name, sizeof(name));
      String fname = String(name);
      if (fname.endsWith(".mp3") || fname.endsWith(".MP3")) {
        mp3Files.push_back(String(path) + "/" + fname);
      }
    }
    file.close();
  }
  dir.close();
  Serial.println("Znalezione pliki MP3:");
  for (auto& f : mp3Files) Serial.println(f);
}

// ======================= WCZYTYWANIE STACJI Z radio.txt ======================
void loadRadioStations() {
  radioStations.clear();
  File file = SD.open("/radio.txt");
  if (!file) return;

  while (file.available()) {
    String url = file.readStringUntil('\n');
    url.trim();
    if (url.length() > 5) radioStations.push_back(url);
  }
  file.close();
}

// ======================= ODTWARZANIE MP3 ======================
void playCurrentMp3() {
  if (mp3Files.empty()) return;
  String filepath = mp3Files[currentMp3Index];
  Serial.println("Odtwarzam MP3: " + filepath);
  audio.connecttoFS(SD, filepath.c_str() + 1);  // usuwa pierwszy znak '/'
  playingFromSD = true;
}

void playNextMp3() {
  currentMp3Index = (currentMp3Index + 1) % mp3Files.size();
  playCurrentMp3();
}

// ======================= ODTWARZANIE RADIA ======================
void playCurrentRadio() {
  if (radioStations.empty()) return;
  String url = radioStations[currentRadioIndex];
  Serial.println("Odtwarzam radio: " + url);
  audio.connecttohost(url.c_str());
  playingFromSD = false;
}

void playCurrentSource() {
  volumeMode = false;
  if (currentSource == SOURCE_SD && !mp3Files.empty()) {
    playCurrentMp3();
  } else if (currentSource == SOURCE_RADIO && !radioStations.empty()) {
    playCurrentRadio();
  }
}

// ======================= SETUP ======================
void setup() {
  Serial.begin(115200);

  enc.setType(TYPE2);
  enc.setTickMode(AUTO);
  pinMode(ENC_BUTTON_PIN, INPUT);

  if (!sdFat.begin(SD_CS, SD_SCK_MHZ(10))) {
    Serial.println("Błąd SdFat");
    while (true);
  }

  listMp3Files("/muzyka");

      if (!SD.begin(SD_CS)) {
    Serial.println("Błąd SD.h");
    while (true);
  }
  
  loadRadioStations();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\nPołączono z Wi-Fi");

  audio.setPinout(I2S_BCK_PIN, I2S_WS_PIN, I2S_DOUT_PIN);
  audio.setVolume(volume);

  playCurrentSource();
}

// ======================= LOOP ======================
void loop() {
  enc.tick();
  audio.loop();

  if (enc.isHolded()) {
    volumeMode = true;
    Serial.println("Tryb głośności");
  }

  if (enc.isClick()) {
    Serial.println("Kliknięcie - zmiana źródła");
    if (!volumeMode) {
      currentSource = (SourceType)((currentSource + 1) % totalSources);
      audio.stopSong();
      delay(200);
      playCurrentSource();
    } else {
      volumeMode = false;
      Serial.println("Wyjście z trybu głośności");
    }
  }

  if (enc.isRight()) {
    if (volumeMode) {
      volume = min(21, volume + 1);
      audio.setVolume(volume);
      Serial.println("Głośność: " + String(volume));
    } else {
      if (currentSource == SOURCE_SD && !mp3Files.empty()) {
        currentMp3Index = (currentMp3Index + 1) % mp3Files.size();
        audio.stopSong(); delay(100);
        playCurrentMp3();
      } else if (currentSource == SOURCE_RADIO && !radioStations.empty()) {
        currentRadioIndex = (currentRadioIndex + 1) % radioStations.size();
        audio.stopSong(); delay(100);
        playCurrentRadio();
      }
    }
  }

  if (enc.isLeft()) {
    if (volumeMode) {
      volume = max(0, volume - 1);
      audio.setVolume(volume);
      Serial.println("Głośność: " + String(volume));
    } else {
      if (currentSource == SOURCE_SD && !mp3Files.empty()) {
        currentMp3Index = (currentMp3Index - 1 + mp3Files.size()) % mp3Files.size();
        audio.stopSong(); delay(100);
        playCurrentMp3();
      } else if (currentSource == SOURCE_RADIO && !radioStations.empty()) {
        currentRadioIndex = (currentRadioIndex - 1 + radioStations.size()) % radioStations.size();
        audio.stopSong(); delay(100);
        playCurrentRadio();
      }
    }
  }

  if (playingFromSD && !audio.isRunning()) {
    delay(500);
    playNextMp3();
  }

  yield();
}