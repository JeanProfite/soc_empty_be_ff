/***************************************************************************//**
 * @file
 * @brief Core application logic.
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/
#include "em_common.h"
#include "app_assert.h"
#include "sl_bluetooth.h"
#include "gatt_db.h"
#include "app.h"
#include "sl_sensor_rht.h"
#include "temperature.h"
#include "sl_simple_timer.h"
#include "sl_simple_led_instances.h"

uint32_t rh=0;
int32_t t=0x8000;
int step=0;
sl_simple_timer_t timer;
uint8_t connection_notify=0;
int ledState = 0;

// The advertising set handle allocated from Bluetooth stack.
static uint8_t advertising_set_handle = 0xff;

/**************************************************************************//**
 * Application Init.
 *****************************************************************************/
SL_WEAK void app_init(void)
{
  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application init code here!                         //
  // This is called once during start-up.                                    //
  /////////////////////////////////////////////////////////////////////////////

  app_log_info("%s\n", __FUNCTION__);
}

/**************************************************************************//**
 * Application Process Action.
 *****************************************************************************/
SL_WEAK void app_process_action(void)
{
  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application code here!                              //
  // This is called infinitely.                                              //
  // Do not call blocking functions from here!                               //
  /////////////////////////////////////////////////////////////////////////////
}

void callback(){
  temp_read_BLE(&rh,&t);
  app_log_info("[Notify]Temperature format BLE: %d \n",t);
  sl_bt_gatt_server_send_notification(connection_notify,gattdb_temperature,sizeof(t),(uint8_t*)&t);
}

/**************************************************************************//**
 * Bluetooth stack event handler.
 * This overrides the dummy weak implementation.
 *
 * @param[in] evt Event coming from the Bluetooth stack.
 *****************************************************************************/
void sl_bt_on_event(sl_bt_msg_t *evt)
{
  sl_status_t sc;
  bd_addr address;
  uint8_t address_type;
  uint8_t system_id[8];

  switch (SL_BT_MSG_ID(evt->header)) {
    // -------------------------------
    // This event indicates the device has started and the radio is ready.
    // Do not call any stack command before receiving this boot event!
    case sl_bt_evt_system_boot_id:

      // Extract unique ID from BT Address.
      sc = sl_bt_system_get_identity_address(&address, &address_type);
      app_assert_status(sc);

      // Pad and reverse unique ID to get System ID.
      system_id[0] = address.addr[5];
      system_id[1] = address.addr[4];
      system_id[2] = address.addr[3];
      system_id[3] = 0xFF;
      system_id[4] = 0xFE;
      system_id[5] = address.addr[2];
      system_id[6] = address.addr[1];
      system_id[7] = address.addr[0];

      sc = sl_bt_gatt_server_write_attribute_value(gattdb_system_id,
                                                   0,
                                                   sizeof(system_id),
                                                   system_id);
      app_assert_status(sc);

      // Create an advertising set.
      sc = sl_bt_advertiser_create_set(&advertising_set_handle);
      app_assert_status(sc);

      // Set advertising interval to 100ms.
      sc = sl_bt_advertiser_set_timing(
        advertising_set_handle,
        160, // min. adv. interval (milliseconds * 1.6)
        160, // max. adv. interval (milliseconds * 1.6)
        0,   // adv. duration
        0);  // max. num. adv. events
      app_assert_status(sc);
      // Start general advertising and enable connections.
      sc = sl_bt_advertiser_start(
        advertising_set_handle,
        sl_bt_advertiser_general_discoverable,
        sl_bt_advertiser_connectable_scannable);
      app_assert_status(sc);
      break;

    // -------------------------------
    // This event indicates that a new connection was opened.
    case sl_bt_evt_connection_opened_id:
      app_log_info("%s Connection OPENED\n", __FUNCTION__);
      //___Sensor initialisation__
      sc = sl_sensor_rht_init();
      app_assert_status(sc);
      app_log_info("%s Initialisation Si70xx OK\n Initialisation LED OK\n", __FUNCTION__);
      //sl_simple_led_init_instances();
      break;

    // -------------------------------
    // This event indicates that a connection was closed.
    case sl_bt_evt_connection_closed_id:
      app_log_info("%s Connection CLOSED\n", __FUNCTION__);
      //__Sensor deinitialisation__
      sl_sensor_rht_deinit();
      app_log_info("%s Si70xx desinitialized\n", __FUNCTION__);

      // Restart advertising after client has disconnected.
      sc = sl_bt_advertiser_start(
        advertising_set_handle,
        sl_bt_advertiser_general_discoverable,
        sl_bt_advertiser_connectable_scannable);
      app_assert_status(sc);
      break;

      // -------------------------------
      // This event indicates that a client request a read.
     case sl_bt_evt_gatt_server_user_read_request_id:
       app_log_info("%s Read from client\n", __FUNCTION__);
       if (evt->data.evt_gatt_server_user_read_request.characteristic == gattdb_temperature) {
           temp_read_BLE(&rh,&t);
           app_log_info("Temperature format BLE: %d \n",t);
           sl_bt_gatt_server_send_user_read_response(
               evt->data.evt_gatt_server_user_read_request.connection,gattdb_temperature,
               0,
               sizeof(t),
               (uint8_t*)&t,
               NULL);
       }
       break;

       // -------------------------------
       // This event indicates that a client changed a characteristic status.
     case sl_bt_evt_gatt_server_characteristic_status_id:
       if (evt->data.evt_gatt_server_characteristic_status.characteristic == gattdb_temperature) { //evenement sur la caractéristique température
           app_log_info("Status flag: 0x%X\n",evt->data.evt_gatt_server_characteristic_status.status_flags);
           if(evt->data.evt_gatt_server_characteristic_status.status_flags == 0x1){ //descripteur modifier
               app_log_info("%s Temp Notify toggle\n", __FUNCTION__);
               app_log_info("Client config flag: 0x%X\n",evt->data.evt_gatt_server_characteristic_status.client_config_flags);
           if(
               (evt->data.evt_gatt_server_characteristic_status.client_config_flags == 0x01)
               ||
               (evt->data.evt_gatt_server_characteristic_status.client_config_flags == 0x03 )
               ){ //si notif ou indicate actif
               app_log_info("Timer on\n");
               connection_notify=evt->data.evt_gatt_server_characteristic_status.connection;
               sl_simple_timer_start(&timer, 1000, callback, NULL, 1);
           }
           else if(//si notif ou indicate inactif
               (evt->data.evt_gatt_server_characteristic_status.client_config_flags == 0x00)
               ||
               (evt->data.evt_gatt_server_characteristic_status.client_config_flags == 0x02)){
               app_log_info("Timer off\n");
               sl_simple_timer_stop(&timer);
           }
           }
       }
       break;

     case sl_bt_evt_gatt_server_user_write_request_id:
       if (evt->data.evt_gatt_server_user_write_request.characteristic == gattdb_digital) {

           int ledWrite = evt->data.evt_gatt_server_user_write_request.value.data[evt->data.evt_gatt_server_user_write_request.value.len-1];
           app_log_info("Data:%x\n",ledWrite);

           if(ledState != ledWrite){
               sl_simple_led_toggle(sl_led_led0.context);
               ledState=!ledState;
           }

         sl_bt_gatt_server_send_user_prepare_write_response(evt->data.evt_gatt_server_user_write_request.connection, gattdb_digital, SL_STATUS_OK, 0, 1, (uint8_t*)ledState);
         sl_bt_gatt_server_send_user_write_response(evt->data.evt_gatt_server_user_write_request.connection,
                                                  gattdb_digital,
                                                  SL_STATUS_OK);
         app_log_info("Write responce send\n");
       }

       break;

    ///////////////////////////////////////////////////////////////////////////
    // Add additional event handlers here as your application requires!      //
    ///////////////////////////////////////////////////////////////////////////

    // -------------------------------
    // Default event handler.
    default:
      break;
  }
}
