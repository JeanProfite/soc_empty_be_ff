#include <stdint.h>
#include "sl_sensor_rht.h"

int16_t temp_read_BLE_ext(void){
  uint32_t rh=0;
  int32_t t=0;
  sl_sensor_rht_get(&rh, &t);
  return t/10;
}

void temp_read_BLE(uint32_t *prh,int32_t *pt){
  sl_sensor_rht_get(prh, pt);
  *pt/=10;
}
