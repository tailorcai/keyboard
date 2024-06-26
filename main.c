/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <pico/printf.h>
#include <string.h>
#include "class/cdc/cdc_device.h"
#include "class/hid/hid.h"
#include "pico/stdio.h"
#include "pico/stdlib.h"

#include "bsp/board.h"
#include "pico/time.h"
#include "pio_usb.h"
#include "tusb.h"

#include "usb_descriptors.h"
#include "pico/multicore.h"
#include "led.h"

#include "queue.h"
#include "persist.h"
#include "kb.h"


//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+
// WebUSB stuff
// #define URL "kb003.config.interface.systems"
// const tusb_desc_webusb_url_t desc_url =
// {
//   .bLength         = 3 + sizeof(URL) - 1,
//   .bDescriptorType = 3, // WEBUSB URL type
//   .bScheme         = 1, // 0: http, 1: https
//   .url             = URL
// };
// static bool web_serial_connected = false;

//------------- prototypes -------------//
// void webserial_task(void);
// void send_webusb_message(char type, uint8_t * data, uint8_t data_size);
void hid_task(void);

static usb_device_t *usb_device = NULL;
KEY_STRING* str_tbl = NULL;
QUEUE q_send;

// core1: handle host events
void core1_main() {
  // sleep_ms(10);

  // To run USB SOF interrupt in core1, create alarm pool in core1.
  static pio_usb_configuration_t config = PIO_USB_DEFAULT_CONFIG;
  config.alarm_pool = (void*)alarm_pool_create(2, 1);
  usb_device = pio_usb_host_init(&config);

  //// Call pio_usb_host_add_port to use multi port
  // const uint8_t pin_dp2 = 8;
  // pio_usb_host_add_port(pin_dp2, PIO_USB_PINOUT_DPDM);

  // while (true) {
  //   pio_usb_host_task();
  // }
}

void my_host_task();
void control_task();
void cdc_task(void);
void process_keycode(uint8_t keycode);
int cdc__printf (const char *format, ...);
/*------------- MAIN -------------*/
int main(void)
{
  board_init();
  tusb_init();
  // delay
  str_tbl = persist_readBackMyData();

  // multicore_reset_core1();
  // // all USB task run in core1
  // multicore_launch_core1(core1_main);
  core1_main(); // multicore will cause flash program lock

  //set_sys_clock_khz(200000, true);
  keyboard_init();
  led_init();
  led_solid(false);
    
  CreateQueue(&q_send, 64);
  int n = 0;
  while (1)
  {
    my_host_task();
    tud_task(); // tinyusb device task
    hid_task();
    led_task();
    control_task();
    pio_usb_host_task();
    if( keyboard_update()) {
      uint8_t* keycodes = get_keycode_report();
      for (int i = 0; i < KEYBOARD_REPORT_SIZE; i++) {
        cdc__printf( "%02x ", keycodes[i]);
      }
      cdc__printf("\n");

      process_keycode( keycodes[0]);
    }
    // cdc_task();
  }

  return 0;
}


int cdc__printf (const char *format, ...)
{
   va_list arg;
   int done;

   va_start (arg, format);
   char buf[1024];
   done = vsnprintf (buf, sizeof(buf), format, arg);
   va_end (arg);

   int i=0; 
   while(i<done) {
    int n = done-i>64?64:done-i;
    tud_cdc_write( &buf[i], n );
    tud_cdc_write_flush();
    sleep_ms(10);
    i+= n;
   }
   return done;
}

void dbg_dump(const char* buf, int len) {
  for( int i=0;;i++) {
    if( len!=-1 && i>=len || len==-1 && buf[i]==0 )
      break;
    cdc__printf("0x%02x ", buf[i]);
  }
  cdc__printf("\n");
}

void control_task() {
  static char buf[1024] = {0};
  static int len = 0;
  if( len > 1000) {
    // error, reset
    // printf("line overflow, reset!\n");
    len = 0;
  }

  if (!tud_cdc_available()) return;
  char c;
  uint32_t count = tud_cdc_read(&c, 1);
  if( !count ) return;
  // int c=getchar_timeout_us(0);
  // if ( c == PICO_ERROR_TIMEOUT ) return;
  
  buf[len++] = c;
  buf[len] = 0;
  if( c != '\n' && c != '\r') return;
  if( len == 1 ) {
    len = 0;
    return;
  }

  // dbg_dump("DBG: stdin read ",-1);dbg_dump(buf, len);
  // byte: id    0x59 -> 0x5e
  // byte: 
  KEY_STRING *p = str_tbl;
  bool saved = false;
  switch(buf[0]) {
    case '?':
      cdc__printf("input:[CMD] [ID] [DATA...]\n");
      cdc__printf("CMD: a=add,c=clear all,s=show\n");
      cdc__printf("ID:1-9\nDATA:text\n");
      break;
    case 's':
    case 'S': 
      for( int i=0;i<6;i++, p++) {
        if( p->ch )
          cdc__printf("%x,%s\n", p->ch, p->data);
        else
          cdc__printf("empty\n");
      }
      break;

    case 'c':
    case 'C': 
      for( int i=0;i<6;i++, p++) {
        p->ch = 0;
      }
      saved = true;
      break;

    case 'a': 
    case 'A': {
      int ch = buf[2] - '1' + 0x59;   // my code START from 0x59
      for( int i=0;i<6;i++, p++) {        
        
        if( p->ch == ch || p->ch == 0) {
          p->ch = ch;
          uint8_t* ptr = buf+4;
          uint8_t* dst = p->data;
          int j=len-5;
          while( j ) {
            uint8_t c = *ptr;
            cdc__printf("%d-%02x ", j, c);
            if( c == '\\') {
              ptr++;j--;
              c = *ptr;
              if( c == 'n')
                c = '\n';
            }
            *dst = c;
            dst++;
            ptr++;
            j--;
          }
          *dst = 0;
          // strncpy(p->data,buf+4, len-5);
          p++;
          for( int j=i+1;j<6;j++,p++)
            if(p->ch == ch) p->ch = 0;
          break;
        }
      }
      saved = true;
      break;
    }
    default:
      cdc__printf("unknown cmd %c\n", buf[0]);
      return;
  }
  
  len = 0;
  if( saved ) {
    // printf("table overflow, reset!\n");
    // cdc__printf(" persist save\n");
    persist_saveMyData();
    cdc__printf(" persist done\n");
  }
}

uint8_t const conv_table[128][2] =  { HID_ASCII_TO_KEYCODE };

MY_KEY translate_ascii(uint8_t in) {

  if( in >= 0 && in <= 127) {
    MY_KEY key = { KEY_TYPE_NORMAL ,
          conv_table[in][0]?KEYBOARD_MODIFIER_LEFTSHIFT:0 ,
          conv_table[in][1] };
    return key;
  }
  MY_KEY empty = {KEY_TYPE_NONE};
  return empty;
}

void process_keycode(uint8_t keycode) {
  switch(keycode) {
    case 0x59:
    case 0x5a:
    case 0x5b:
    case 0x5d:
    case 0x5e: {
        KEY_STRING *keystr = str_tbl;
        for( int i=0;i<6;i++,keystr++) {
          // for ascii 
          if( keycode == keystr->ch) {
            cdc__printf("found %s\n", keystr->data);
            for( uint8_t* p = keystr->data;*p!=0;p++) {
              MY_KEY keycode = translate_ascii(*p);
              if( keycode.type != KEY_TYPE_NONE ) {
                cdc__printf("%02x,%02x,%02x\n", keycode.type, keycode.modifier, keycode.code );
                Enqueue(&q_send, keycode);
                MY_KEY key_type_release = {KEY_TYPE_RELEASE};
                Enqueue(&q_send, key_type_release);
              }
            }
            cdc__printf("\n");
          }
        }
        break;
    }
    case 0x5c: {
      cdc__printf("0x5c found\n");
      MY_KEY keys[] = {
            { KEY_TYPE_NORMAL, 3, 0 },  // CTRL+SHIFT DOWN
            { KEY_TYPE_MEDIA, 0, 0xb8}, // EJECT
            { KEY_TYPE_MEDIA, 0, 0x0},
            { KEY_TYPE_RELEASE},   
            { KEY_TYPE_NONE}                     
      };
      for( int i=0;keys[i].type != KEY_TYPE_NONE;i++) {
        Enqueue( &q_send, keys[i]);
      }
      break;
    }
    case 0x5f:
      Enqueue( &q_send, (MY_KEY) {KEY_TYPE_MEDIA, 0, HID_USAGE_CONSUMER_VOLUME_DECREMENT});
      Enqueue( &q_send, (MY_KEY){KEY_TYPE_MEDIA, 0, 0});
      break;
    case 0x60:
      Enqueue( &q_send, (MY_KEY){KEY_TYPE_MEDIA, 0, HID_USAGE_CONSUMER_VOLUME_INCREMENT});
      Enqueue( &q_send, (MY_KEY){KEY_TYPE_MEDIA, 0, 0});
      break;
    case 0x61:
      Enqueue( &q_send, (MY_KEY){KEY_TYPE_MEDIA, 0, HID_USAGE_CONSUMER_PLAY_PAUSE});
      Enqueue( &q_send, (MY_KEY){KEY_TYPE_MEDIA, 0, 0});
      break;
  }  
}

void my_host_task() {
  if( FullQueue(&q_send))
    return;

    if (usb_device != NULL) {
      for (int dev_idx = 0; dev_idx < PIO_USB_DEVICE_CNT; dev_idx++) {
        usb_device_t *device = &usb_device[dev_idx];
        if (!device->connected) {
          continue;
        }

        // Print received packet to EPs
        for (int ep_idx = 0; ep_idx < PIO_USB_DEV_EP_CNT; ep_idx++) {
          endpoint_t *ep = pio_usb_get_endpoint(device, ep_idx);

          if (ep == NULL) {
            break;
          }

          uint8_t temp[64];
          int len = pio_usb_get_in_data(ep, temp, sizeof(temp));
          if(  (ep->ep_num == 0x81 && len == 9 && temp[0] == 1 && temp[3]))          // normal key
          {
            process_keycode(temp[3]);
          }
          else if( (ep->ep_num == 0x83 && len == 3 && temp[0] == 2 && temp[1]) ) {      // media key
            switch( temp[1]) {
              case 0xea:    // left
                Enqueue( &q_send, (MY_KEY) {KEY_TYPE_MEDIA, 0, HID_USAGE_CONSUMER_VOLUME_DECREMENT});
                Enqueue( &q_send, (MY_KEY){KEY_TYPE_MEDIA, 0, 0});
                break;
              case 0xe9:    // right
                Enqueue( &q_send, (MY_KEY){KEY_TYPE_MEDIA, 0, HID_USAGE_CONSUMER_VOLUME_INCREMENT});
                Enqueue( &q_send, (MY_KEY){KEY_TYPE_MEDIA, 0, 0});
                break;
              case 0xcd:  // press
                Enqueue( &q_send, (MY_KEY){KEY_TYPE_MEDIA, 0, HID_USAGE_CONSUMER_PLAY_PAUSE});
                Enqueue( &q_send, (MY_KEY){KEY_TYPE_MEDIA, 0, 0});
                break;
            }
          }
          //   Enqueue(&q_send, temp[1]);
          // }
        #if 1
          if (len > 0) {
            cdc__printf( "%04x:%04x EP 0x%02x:\t", device->vid, device->pid,
                   ep->ep_num);
            for (int i = 0; i < len; i++) {
              cdc__printf( "%02x ", temp[i]);
            }
            cdc__printf("\n");
          }
        #endif 
        }
      }
    }
}
//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
  // led_solid(true);
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
  // led_blink(LED_BLINK_NOT_MOUNTED);
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
  (void) remote_wakeup_en;
  // led_blink(LED_BLINK_SUSPENDED);
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
  // led_solid(true);
}

//--------------------------------------------------------------------+
// HID
//--------------------------------------------------------------------+
bool hid_queued = false;
static void send_hid_report()
{
  if (!tud_hid_ready()) {
    hid_queued = true;
    return;
  }

  // static bool bRelease = false;
  MY_KEY key;

  if( EmptyQueue(&q_send) ) {
    return;
  }
  Dequeue(&q_send, &key);

  switch( key.type ) {
    case KEY_TYPE_NORMAL: {
      uint8_t keycode[6] = { 0 };
      keycode[0] = key.code;
      printf("normal key, %02x %02x\n",  key.modifier, key.code);
      tud_hid_keyboard_report(REPORT_ID_KEYBOARD, key.modifier, keycode);    
      break;
    }
    case KEY_TYPE_RELEASE: {
      printf("release all key\n");
      uint8_t keycode[6] = { 0 };
      tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, keycode);
      break;
    }
    // case KEY_TYPE_OSC: {
    //   uint8_t b = key.code & 0xFF;
    //   printf("OSC, %02x\n", b);
    //   tud_hid_report(REPORT_ID_KEYBOARD, &b, 1);  // OSC is 1 byte
    //   break;
    // }
    case KEY_TYPE_DELAY: {
      printf("delay...");
      sleep_ms(100);
      break;
    }
    // case KEY_TYPE_OSC_RELEASE: {
    //   uint8_t c = 0;
    //   tud_hid_report(REPORT_ID_KEYBOARD, &c, 1);
    // }
    case KEY_TYPE_MEDIA: {
      uint16_t media_key_held = key.code;
      tud_hid_report(REPORT_ID_CONSUMER_CONTROL, &media_key_held, 2);
    }

  }
}

// static void send_media_report()
// {
//   static uint16_t media_key_held = 0;
//   uint16_t media_key = 0;
//   uint8_t * report = get_keycode_report();

//   for (int i = 0; i < KEYBOARD_REPORT_SIZE; i++) {
//     if (report[i] == HID_KEY_VOLUME_UP)
//       media_key = HID_USAGE_CONSUMER_VOLUME_INCREMENT;
//     else if (report[i] == HID_KEY_VOLUME_DOWN)
//       media_key = HID_USAGE_CONSUMER_VOLUME_DECREMENT; 
//     else if (report[i] == HID_KEY_MUTE)
//       media_key = HID_USAGE_CONSUMER_MUTE;
//   }
  
//   if (media_key != 0 && media_key != media_key_held) {
//     board_delay(2); // space from previous report .. because
//     media_key_held = media_key;
//     tud_hid_report(REPORT_ID_CONSUMER_CONTROL, &media_key, 2);
//   } else if (media_key == 0 && media_key_held != 0){
//     board_delay(2); // space from previous report .. because
//     media_key_held = 0;
//     uint16_t empty_key = 0;
//     tud_hid_report(REPORT_ID_CONSUMER_CONTROL, &empty_key, 2);
//   }
// }

// void send_webusb_report() {
//   if (!web_serial_connected)
//     return;
  
//   uint8_t * report = get_raw_report(); // need to replace this with a pins-down map

//   send_webusb_message('r', report, KEYS);
// }

// tud_hid_report_complete_cb() is used to send the next report after previous one is complete
void hid_task(void)
{
  // Poll very quickly - faster than our USB polling rate so we always have fresh data
  // available (see TUD_HID_DESCRIPTOR in usb_descriptors.c)
  #define KEYBOARD_SCAN_RATE_US 125
  const uint64_t interval_us = KEYBOARD_SCAN_RATE_US;
  static uint64_t start_us = 0;

  if (time_us_64() - start_us < interval_us) return; // not enough time
  start_us += interval_us;

  bool changed = !EmptyQueue(&q_send);
  if (!hid_queued && !changed) return;

  // Remote wakeup
  if (tud_suspended()) {
    // Wake up host if we are in suspend mode
    // and REMOTE_WAKEUP feature is enabled by host
    tud_remote_wakeup();
  } else {
    // Send the 1st of report chain, the rest will be sent by tud_hid_report_complete_cb()
    send_hid_report();
    // send_media_report();
    // send_webusb_report();
  }
}

// Invoked when sent REPORT successfully to host
// Application can use this to send the next report
// Note: For composite reports, report[0] is report ID
void tud_hid_report_complete_cb(uint8_t instance, uint8_t const* report, uint16_t len)
{
  (void) instance;
  (void) len;

  return;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
  (void) instance;

  if (report_type == HID_REPORT_TYPE_OUTPUT)
  {
    // Set keyboard LED e.g Capslock, Numlock etc...
    if (report_id == REPORT_ID_KEYBOARD)
    {
      // bufsize should be (at least) 1
      if ( bufsize < 1 ) return;

      uint8_t const kbd_leds = buffer[0];

      if (kbd_leds & KEYBOARD_LED_CAPSLOCK) {
        // Capslock On: disable blink, turn led on
        // led_solid(true);
      } else {
        // Caplocks Off: back to normal blink
        // led_blink(LED_BLINK_MOUNTED);
      }
    }
  }
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
  // TODO not Implemented
  (void) instance;
  (void) report_id;
  (void) report_type;
  (void) buffer;
  (void) reqlen;

  return 0;
}
/*
// Stub so we can implement queuing to prevent message
// concatenation - the problem is we don't know the
// window of the concating.
//
// Need to look at https://github.com/hathach/tinyusb/blob/src/class/vendor/vendor_device.c
// and tud_vendor_write_available() to see if those provide clues
void send_webusb_message(char type, uint8_t * data, uint8_t data_size) {
  if (!web_serial_connected)
    return;

  // Need to check that this does what we think it does
  //if (!tud_vendor_available())
  //  return;

  int buf_size = data_size + 2;
  uint8_t buf[buf_size];

  memset(buf, 0, buf_size);
  buf[0] = buf_size;
  buf[1] = type;
  memcpy(buf + 2, data, data_size);

  tud_vendor_write(buf, buf_size);
}
*/
//--------------------------------------------------------------------+
// WebUSB use vendor class
//--------------------------------------------------------------------+

// Invoked when a control transfer occurred on an interface of this class
// Driver response accordingly to the request and the transfer stage (setup/data/ack)
// return false to stall control endpoint (e.g unsupported request)
bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request)
{
  // nothing to with DATA & ACK stage
  if (stage != CONTROL_STAGE_SETUP) return true;
  /*
  switch (request->bmRequestType_bit.type)
  {
    case TUSB_REQ_TYPE_VENDOR:
      switch (request->bRequest)
      {
        case VENDOR_REQUEST_WEBUSB:
          // match vendor request in BOS descriptor
          // Get landing page url
          return tud_control_xfer(rhport, request, (void*) &desc_url, desc_url.bLength);

        case VENDOR_REQUEST_MICROSOFT:
          if ( request->wIndex == 7 )
          {
            // Get Microsoft OS 2.0 compatible descriptor
            uint16_t total_len;
            memcpy(&total_len, desc_ms_os_20+8, 2);

            return tud_control_xfer(rhport, request, (void*) desc_ms_os_20, total_len);
          } else {
            return false;
          }

        default: break;
      }
    break;

    case TUSB_REQ_TYPE_CLASS:
      if (request->bRequest == 0x22)
      {
        // Webserial simulate the CDC_REQUEST_SET_CONTROL_LINE_STATE (0x22) to connect and disconnect.
        web_serial_connected = (request->wValue != 0);

        // Always lit LED if connected
        if ( web_serial_connected ) {
          led_solid(true);
          tud_vendor_write_str("\r\nTinyUSB WebUSB device example\r\n");
        } else {
          led_blink(LED_BLINK_MOUNTED);
        }

        // response with status OK
        return tud_control_status(rhport, request);
      }
    break;

    default: break;
  } */
  // stall unknown request
  return false;
}

// void send_webusb_keyboard_config() {
//   uint8_t data[KEYS * KEY_CONFIG_SIZE];
//   uint8_t size = keyboard_config_read(data, sizeof(data));
//   send_webusb_message('c', data, size);
// }
/*
void webserial_task(void)
{
  if (!web_serial_connected)
    return;

  uint8_t buf[128]; // need to check this
  uint32_t count = tud_vendor_read(buf, sizeof(buf));
  printf("read: %d", count);

  if (count == 0)
    return;

  if(buf[0] == 'c') {
    // Read the config and send
    send_webusb_keyboard_config();
  } else if (buf[0] == 's') {
    // Set the keymap
    keyboard_config_set(buf + 1, count - 1);
    keyboard_config_flash_save();
    send_webusb_keyboard_config();
  } else if (buf[0] == 'd') {
    keyboard_config_reset();
    send_webusb_keyboard_config();
  }
}
*/

//--------------------------------------------------------------------+
// USB CDC
//--------------------------------------------------------------------+
void cdc_task(void) {
  // connected() check for DTR bit
  // Most but not all terminal client set this when making connection
  // if ( tud_cdc_connected() )
  {
    // connected and there are data available
    if (tud_cdc_available()) {
      // read data
      char buf[64];
      uint32_t count = tud_cdc_read(buf, sizeof(buf));
      (void) count;

      // Echo back
      // Note: Skip echo by commenting out write() and write_flush()
      // for throughput test e.g
      //    $ dd if=/dev/zero of=/dev/ttyACM0 count=10000
      tud_cdc_write(buf, count);
      tud_cdc_write_flush();
    }
  }
}

// Invoked when cdc when line state changed e.g connected/disconnected
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts) {
  (void) itf;
  (void) rts;

  // TODO set some indicator
  if (dtr) {
    // Terminal connected
  } else {
    // Terminal disconnected
  }
}

// Invoked when CDC interface received data from host
void tud_cdc_rx_cb(uint8_t itf) {
  (void) itf;
}

