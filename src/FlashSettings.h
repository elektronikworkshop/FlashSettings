/** FlashSettings -- A generic flash storage class for persistently storing your settings.
 *
 *
 *
 * Ripped the basic concept from Greg Cunningham's
 *
 *   https://github.com/CuriousTech/ESP8266-HVAC/blob/master/Arduino/eeMem.h
 *
 * and added
 *  * templating to make it reusable
 *  * more type checking to avoid some nasty surprises
 */
#ifndef _EW_FLASH_SETTINGS_H
#define _EW_FLASH_SETTINGS_H

// TODO: EEPROM class behaves differently in ESP and AVR - more work required

#include <EEPROM.h>

#if defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32)
# include <type_traits>
# define EW_HAVE_TYPE_TRAITS  1
#else
# error "architecture not supported"
#endif

/** The base struct for the user's settings struct
 */
class FlashDataBase
{
public:
  uint16_t m_size;
  uint16_t m_checksum;
};

template<typename T,
         size_t FlashPartitionSize = 512>
class FlashSettings
  : public T
{
public:

  FlashSettings()
  {
    /* Initialize the local size and checksum in the base class.
     * This way the user must only care about his own BS.
     *
     * We require the user to inherit from our base class though -- hehe :)
     */
     this->m_size = sizeof(T);
     this->m_checksum = 0xAAAA;
  }
  /** Initialize flash settings.
   * Loads data from flash and checks if the data type (size) changed or if
   * there was any data corruption by recomputing its checksum.
   * On failure the defaults are loaded.
   * @return
   * True if the settings were successfully loaded else false.
   */
  bool
  begin()
  {
    /* Here are some nice examples why the support of type_traits is a good
     * idea:
     */

    /* Make sure the user doesn't try to store structs bigger than the size of
     * the requested flash partition.
     */
    static_assert(sizeof(T) <= FlashPartitionSize,
                  "Flash partition smaller than the data you'd like to backup");

    /* Force the user to inherit from our nice data base class.
     */
    static_assert(std::is_base_of<FlashDataBase, T>::value,
                  "You must inherit your flash settings struct from "
                  "FlashDataBase");

    EEPROM.begin(FlashPartitionSize);

    uint8_t data[sizeof(T)];
    uint16_t *pwTemp = (uint16_t *)data;

    /* load data from flash */
    for (unsigned int i = 0; i < sizeof(T); i++) {
      data[i] = EEPROM.read(i);
    }

    /* reverting to defaults when the data size changed in code */
    if (pwTemp[0] != sizeof(T)) {
      return false;
    }

    uint16_t sum = pwTemp[1];
    pwTemp[1] = 0;  /* reset sum in data before computing new checksum */
    pwTemp[1] = fletcher16(data, sizeof(T));

    /* reverting to defaults when the checksum fails */
    if(pwTemp[1] != sum) {
      return false;
    }

    /* everything ok, load data by casting to the actual start of the data,
     * just in case someone should use this on a rtti enabled system.
     * The update member function takes care of this as well, see below ...
     */
    memcpy(static_cast<FlashDataBase*>(this), data, sizeof(T) );

    return true;
  }

  /** Check if data changed and write to flash if so.
   */
  void
  update()
  {
    uint16_t old_sum = this->m_checksum;
    this->m_checksum = 0; /* reset sum in data before computing new checksum */
    this->m_checksum = fletcher16((uint8_t*)static_cast<FlashDataBase*>(this), sizeof(T));

    /* no changes detected, no need to write data to flash */
    if (old_sum == this->m_checksum) {
      return;
    }

    /* actually write to flash */
    uint8_t *pData = (uint8_t *)static_cast<FlashDataBase*>(this);
    for (unsigned int i = 0; i < sizeof(T); i++) {
      EEPROM.write(i, pData[i]);
    }
    EEPROM.commit();
  }

  /** Computes checksum to detect data changes.
   * https://en.wikipedia.org/wiki/Fletcher%27s_checksum
   */
  static uint16_t
  fletcher16( uint8_t* data, int count)
  {
     uint16_t sum1 = 0;
     uint16_t sum2 = 0;

     for (int index = 0; index < count; ++index) {
        sum1 = (sum1 + data[index]) % 255;
        sum2 = (sum2 + sum1) % 255;
     }

     return (sum2 << 8) | sum1;
  }
};

#endif  /* #define _EW_FLASH_SETTINGS_H */
