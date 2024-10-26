#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Encoder.h>
#include <EEPROM.h>

LiquidCrystal_I2C lcd(0x27, 16, 2); // Sesuaikan alamat I2C jika perlu
Encoder encoder(2, 3); // Pin DT dan CLK pada rotary encoder
int encoderButtonPin = 4; // Pin tombol pada rotary encoder untuk mengubah faktor perkalian
int switchButtonPin = 6;  // Pin tombol untuk berpindah antara delayOn dan delayOff
int modeButtonPin = 5;    // Pin tombol untuk masuk/keluar mode edit
const int relayPin = 7; // Pin untuk relay

long delayOn = 10000; // 10 detik dalam milidetik
long delayOff = 10000; // 10 detik dalam milidetik
long countdownOn;
long countdownOff;
long lastPosition = 0;
int multiplicationState = 0; // 0: x1, 1: x60, 2: x3600
bool editingDelayOn = true; // Mulai dengan mengedit delayOn
bool editMode = false; // Mulai dengan mode countdown
bool isOff = false; // Mulai dengan countdown delayOn
unsigned long previousMillis = 0;
const long interval = 1000;
bool lcdNeedsUpdate = true; // Flag to trigger an update

void setup() {
  pinMode(encoderButtonPin, INPUT_PULLUP); // Gunakan pull-up internal untuk tombol perkalian
  pinMode(switchButtonPin, INPUT_PULLUP);   // Gunakan pull-up internal untuk tombol switch
  pinMode(modeButtonPin, INPUT_PULLUP);     // Gunakan pull-up internal untuk tombol mode edit
  pinMode(relayPin, OUTPUT); // Inisialisasi pin relay
  lcd.init();
  lcd.backlight();

  // Baca nilai dari EEPROM
  EEPROM.get(0, delayOn);
  EEPROM.get(4, delayOff);

  // Jika nilai EEPROM kosong (dengan asumsi nilai default adalah 0), beri nilai awal 10 detik
  if (delayOn == 0 && delayOff == 0) {
    delayOn = 10000;
    delayOff = 10000;
    EEPROM.put(0, delayOn);
    EEPROM.put(4, delayOff);
  }

  countdownOn = delayOn;
  countdownOff = delayOff; // Inisialisasi countdownOff
  digitalWrite(relayPin, HIGH); // Nyalakan relay
  updateLCD(false); // Tampilkan nilai awal delayOn dan delayOff dalam format hh:mm:ss
}

void loop() {
  unsigned long currentMillis = millis();
  
  if (editMode) {
    long newPosition = encoder.read();
    if (newPosition != lastPosition) {
      long delta = newPosition - lastPosition;
      lastPosition = newPosition;

      // Update delayOn atau delayOff tergantung pada mode edit
      if (editingDelayOn) {
        if (multiplicationState == 0) {
          delayOn += delta * 500;
        } else if (multiplicationState == 1) {
          delayOn += delta * 500 * 60;
        } else if (multiplicationState == 2) {
          delayOn += delta * 500 * 3600;
        }
        if (delayOn < 0) delayOn = 0; // Pastikan delayOn tidak negatif
      } else {
        if (multiplicationState == 0) {
          delayOff += delta * 500;
        } else if (multiplicationState == 1) {
          delayOff += delta * 500 * 60;
        } else if (multiplicationState == 2) {
          delayOff += delta * 500 * 3600;
        }
        if (delayOff < 0) delayOff = 0; // Pastikan delayOff tidak negatif
      }
      lcdNeedsUpdate = true;
    }

    // Logika untuk mengubah mode edit menggunakan tombol switch
    if (digitalRead(switchButtonPin) == LOW) {
      delay(50); // Debounce
      if (digitalRead(switchButtonPin) == LOW) {
        editingDelayOn = !editingDelayOn; // Ganti antara editing delayOn dan delayOff
        lcdNeedsUpdate = true; // Perbarui tampilan LCD setelah berpindah mode
        delay(200); // Tambahkan penundaan untuk mencegah multiple counts
      }
    }

    // Logika untuk mengubah mode perkalian
    if (digitalRead(encoderButtonPin) == LOW) {
      delay(50); // Debounce
      if (digitalRead(encoderButtonPin) == LOW) {
        multiplicationState = (multiplicationState + 1) % 3; // Ubah status perkalian
        lcdNeedsUpdate = true; // Perbarui tampilan LCD setelah mengubah perkalian
        delay(200); // Tambahkan penundaan untuk mencegah multiple counts
      }
    }

    // Keluar dari mode edit dan simpan nilai ke EEPROM
    if (digitalRead(modeButtonPin) == LOW) {
      delay(50); // Debounce
      if (digitalRead(modeButtonPin) == LOW) {
        editMode = false; // Keluar dari mode edit
        EEPROM.put(0, delayOn); // Simpan nilai delayOn ke EEPROM
        EEPROM.put(4, delayOff); // Simpan nilai delayOff ke EEPROM
        countdownOn = delayOn; // Reset countdownOn saat keluar dari mode edit
        countdownOff = delayOff; // Reset countdownOff saat keluar dari mode edit
        lcdNeedsUpdate = true; // Perbarui tampilan LCD setelah keluar dari mode edit
        delay(200); // Tambahkan penundaan untuk mencegah multiple counts
      }
    }
  } else {
    // Masuk ke mode edit
    if (digitalRead(modeButtonPin) == LOW) {
      delay(50); // Debounce
      if (digitalRead(modeButtonPin) == LOW) {
        digitalWrite(relayPin, LOW); // Matikan relay
        lcd.clear(); // Membersihkan layar saat masuk ke edit mode
        editMode = true; // Masuk mode edit
        lcdNeedsUpdate = true; // Perbarui tampilan LCD setelah masuk mode edit
        delay(200); // Tambahkan penundaan untuk mencegah multiple counts
      }
    }

    // Countdown logic
    if (currentMillis - previousMillis >= interval) {
      previousMillis = currentMillis;
      if (!isOff) {
        if (countdownOn > 0) {
          countdownOn -= interval; // Mengurangi countdown ON
        } else {
          isOff = true; // Berpindah ke mode OFF
          digitalWrite(relayPin, LOW); // Matikan relay
          countdownOff = delayOff; // Reset countdownOff untuk siklus baru
        }
      } else {
        if (countdownOff > 0) {
          countdownOff -= interval; // Mengurangi countdown OFF
        } else {
          isOff = false; // Berpindah ke mode ON
          digitalWrite(relayPin, HIGH); // Nyalakan relay
          countdownOn = delayOn; // Reset countdownOn untuk siklus baru
        }
      }
      lcdNeedsUpdate = true; // Perbarui tampilan countdown
    }
  }

  // Update LCD only when needed
  if (lcdNeedsUpdate) {
    updateLCD(editMode);
    lcdNeedsUpdate = false; // Reset the flag after updating
  }
}

void updateLCD(bool editing) {
  if (editing) {
    // Tampilkan mode edit
    long currentDelay = editingDelayOn ? delayOn : delayOff;
    int hours = (currentDelay / 3600000);
    int minutes = (currentDelay % 3600000) / 60000;
    int seconds = (currentDelay % 60000) / 1000;

    lcd.setCursor(0, 0);
    lcd.print(editingDelayOn ? "Editing ON  " : "Editing OFF ");
    lcd.setCursor(0, 1);
    lcd.print(hours < 10 ? "0" : "");
    lcd.print(hours);
    lcd.print(":");
    lcd.print(minutes < 10 ? "0" : "");
    lcd.print(minutes);
    lcd.print(":");
    lcd.print(seconds < 10 ? "0" : "");
    lcd.print(seconds);
  } else {
    // Tampilkan countdown
    int hoursOn = (countdownOn / 3600000);
    int minutesOn = (countdownOn % 3600000) / 60000;
    int secondsOn = (countdownOn % 60000) / 1000;
    int hoursOff = (countdownOff / 3600000);
    int minutesOff = (countdownOff % 3600000) / 60000;
    int secondsOff = (countdownOff % 60000) / 1000;

    lcd.setCursor(0, 0);
    lcd.print("ON  ");
    lcd.print(hoursOn < 10 ? "0" : "");
    lcd.print(hoursOn);
    lcd.print(":");
    lcd.print(minutesOn < 10 ? "0" : "");
    lcd.print(minutesOn);
    lcd.print(":");
    lcd.print(secondsOn < 10 ? "0" : "");
    lcd.print(secondsOn);
    
    lcd.setCursor(0, 1);
    lcd.print("OFF ");
    lcd.print(hoursOff < 10 ? "0" : "");
    lcd.print(hoursOff);
    lcd.print(":");
    lcd.print(minutesOff < 10 ? "0" : "");
    lcd.print(minutesOff);
    lcd.print(":");
    lcd.print(secondsOff < 10 ? "0" : "");
    lcd.print(secondsOff);
  }
}
