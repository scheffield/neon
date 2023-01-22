#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define USB_MANUFACTURER   "scheffield"
#define USB_PRODUCT        "Neon rev 1"
#define USB_SERIAL         "" // Empty string for MAC adddress


#define EXTERNAL_NUM_INTERRUPTS 46
#define NUM_DIGITAL_PINS        48
#define NUM_ANALOG_INPUTS       20

// Adafruit GFX _needs_ those definitions even though we are not using them.
#define SCK                     49
#define MISO                    49
#define MOSI                    49
#define SS                      49
#define SDA                     49
#define SCL                     49

#define analogInputToDigitalPin(p)  (((p)<20)?(analogChannelToDigitalPin(p)):-1)
#define digitalPinToInterrupt(p)    (((p)<48)?(p):-1)
#define digitalPinHasPWM(p)         (p < 46)

#endif /* Pins_Arduino_h */
