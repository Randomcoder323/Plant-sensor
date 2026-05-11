/*
 * Soil Moisture Sensor — ATtiny85
 *
 * Hardware:
 *   PB0 (pin 5) — LED Red   (via R1)
 *   PB1 (pin 6) — LED Green (via R2)
 *   PB3 (pin 2) — LED Blue  (via R3)
 *   PB4 (pin 3) — Moisture sensor (ADC2)
 *   BAT1        — CR2032 coin cell (~3V)
 *
 * LED is common-cathode RGB. Drive pin HIGH to light that color.
 *
 * Moisture thresholds (tune these for your soil/sensor):
 *   DRY:   ADC < 300        → Red
 *   OK:    300 <= ADC < 600 → Green
 *   WET:   ADC >= 600       → Blue
 *
 * Power saving:
 *   - Sensor is powered via PB3 (only on during reading)
 *     Wait — PB3 is LED Blue. See note below.
 *   - ATtiny85 sleeps between readings (8s watchdog wakeup)
 *   - Reading interval: ~8 seconds
 *
 * NOTE: PB3 drives LED Blue AND is ADC3. The schematic uses PB4/ADC2
 *       for MOIST. So moisture is on ADC2 = PB4. Blue LED is PB3.
 *       To save power, the moisture sensor VCC can be wired to a GPIO
 *       and toggled. If your sensor is always-on, remove that part.
 *
 * Compile: Arduino IDE with ATtiny85 @ 1MHz internal clock
 *          Board: "ATtiny25/45/85"  Processor: "ATtiny85"  Clock: "1 MHz (internal)"
 */

#include <avr/sleep.h>
#include <avr/wdt.h>

// Pin definitions
#define LED_R   PB0
#define LED_G   PB1
#define LED_B   PB3
#define MOIST   PB4    // ADC2

// ADC channel for PB4
#define MOIST_ADC 2

// Moisture thresholds (0–1023)
// Dry soil = low capacitance = lower ADC reading (resistive sensors vary)
// Tune these for your specific sensor
#define THRESHOLD_DRY  300
#define THRESHOLD_WET  600

// LED on duration (ms) — brief flash to save battery
#define LED_ON_MS  500

// --- Watchdog ISR (wakes from sleep, does nothing else) ---
ISR(WDT_vect) { }

void setup() {
  // LED pins as output, start LOW (off)
  DDRB  |=  (1 << LED_R) | (1 << LED_G) | (1 << LED_B);
  PORTB &= ~((1 << LED_R) | (1 << LED_G) | (1 << LED_B));

  // MOIST pin as input, no pull-up
  DDRB  &= ~(1 << MOIST);
  PORTB &= ~(1 << MOIST);

  // Short startup flash (white) so you know it booted
  setLED(true, true, true);
  delay(300);
  setLED(false, false, false);
  delay(200);
}

void loop() {
  // Read moisture
  int moisture = readADC(MOIST_ADC);

  // Decide color
  if (moisture < THRESHOLD_DRY) {
    // Dry — Red
    flashLED(true, false, false);
  } else if (moisture < THRESHOLD_WET) {
    // Good — Green
    flashLED(false, true, false);
  } else {
    // Wet / waterlogged — Blue
    flashLED(false, false, true);
  }

  // Sleep for ~8 seconds to save battery
  sleepSeconds(8);
}

// --- Helpers ---

void setLED(bool r, bool g, bool b) {
  if (r) PORTB |= (1 << LED_R); else PORTB &= ~(1 << LED_R);
  if (g) PORTB |= (1 << LED_G); else PORTB &= ~(1 << LED_G);
  if (b) PORTB |= (1 << LED_B); else PORTB &= ~(1 << LED_B);
}

void flashLED(bool r, bool g, bool b) {
  setLED(r, g, b);
  delay(LED_ON_MS);
  setLED(false, false, false);
}

int readADC(uint8_t channel) {
  // Select ADC channel, Vcc as reference
  ADMUX  = channel & 0x03;           // MUX[1:0]
  ADCSRA = (1 << ADEN)               // Enable ADC
          | (1 << ADPS2)             // Prescaler /16 → ~62kHz @ 1MHz
          | (1 << ADPS0);
  
  // Discard first reading (ADC needs settling time after enable)
  ADCSRA |= (1 << ADSC);
  while (ADCSRA & (1 << ADSC));

  // Take the real reading
  ADCSRA |= (1 << ADSC);
  while (ADCSRA & (1 << ADSC));

  int result = ADC;

  // Disable ADC to save power
  ADCSRA &= ~(1 << ADEN);

  return result;
}

void sleepSeconds(uint8_t seconds) {
  // ATtiny85 watchdog max is 8s — loop if needed
  uint8_t remaining = seconds;
  while (remaining > 0) {
    uint8_t wdtTime;
    if (remaining >= 8) { wdtTime = (1<<WDP3)|(1<<WDP0); remaining -= 8; }        // 8s
    else if (remaining >= 4) { wdtTime = (1<<WDP3);       remaining -= 4; }        // 4s
    else if (remaining >= 2) { wdtTime = (1<<WDP2)|(1<<WDP1)|(1<<WDP0); remaining -= 2; } // 2s
    else                     { wdtTime = (1<<WDP2)|(1<<WDP1); remaining -= 1; }   // 1s

    // Configure watchdog
    MCUSR &= ~(1 << WDRF);
    WDTCR |= (1 << WDCE) | (1 << WDE);
    WDTCR  = wdtTime | (1 << WDIE);   // Interrupt mode (not reset)

    // Power-down sleep
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_enable();
    sleep_cpu();
    sleep_disable();

    wdt_disable();
  }
}
