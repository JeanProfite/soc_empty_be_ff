#include <stdint.h>
#include "sl_sensor_rht.h"

int16_t temp_read_BLE(void){
  uint32_t rh=0;
  int32_t t=0;
  sl_sensor_rht_get(&rh, &t);
  return t;
}
