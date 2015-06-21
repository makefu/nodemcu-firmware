#include "lualib.h"
#include "lauxlib.h"
#include "platform.h"
#include "auxmods.h"
#include "lrotable.h"
#include "c_stdlib.h"
#include "c_string.h"

#include "c_stdio.h"
#include "c_stdlib.h"
#include "c_stdarg.h"
#include "c_string.h"

#include "strbuf.h"

/**
 * All this code is mostly from http://www.esp8266.com/viewtopic.php?f=21&t=1143&sid=a620a377672cfe9f666d672398415fcb
 * from user Markus Gritsch.
 * I just put this code into its own module and pushed into a forked repo,
 * to easily create a pull request. Thanks to Markus Gritsch for the code.
 */

// ----------------------------------------------------------------------------
// -- This WS2812 code must be compiled with -O2 to get the timing right.  Read this:
// -- http://wp.josh.com/2014/05/13/ws2812-neopixels-are-not-so-finicky-once-you-get-to-know-them/
// -- The ICACHE_FLASH_ATTR is there to trick the compiler and get the very first pulse width correct.
static void ICACHE_FLASH_ATTR send_ws_0(uint8_t gpio) {
  uint8_t i;
  i = 4; while (i--) GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, 1 << gpio);
  i = 9; while (i--) GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, 1 << gpio);
}

static void ICACHE_FLASH_ATTR send_ws_1(uint8_t gpio) {
  uint8_t i;
  i = 8; while (i--) GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, 1 << gpio);
  i = 6; while (i--) GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, 1 << gpio);
}

// brightness may be between 0 and 1
#define MAX_BRIGHTNESS 1

// Brightness is a floating-point number which will be multiplied before
// setting an LED
static lua_Number brightness = MAX_BRIGHTNESS;

// returns the current brightness
static int ICACHE_FLASH_ATTR set_brightness(lua_State* L){
  // brightness must be between 0 and 1
  brightness = luaL_checknumber(L,1);
  return 1;
}

static int ICACHE_FLASH_ATTR get_brightness(lua_State* L){
  lua_pushnumber(L,brightness);
  return 1;
}

// TODO: add mapping table function
// this will provide a way to tell which LED in the array is at what place in
// the matrix
//
// array[numled] = [ledid1,ledid2,...,ledn]

char* remap_buffer= (char*)0;
size_t remap_size = 0;

static int ICACHE_FLASH_ATTR set_remap(lua_State* L){
  // brightness must be between 0 and 1
  if (remap_buffer) { c_free(remap_buffer); }
  const char *buffer = luaL_checklstring(L, 2, &remap_size);
  remap_buffer = (char*)c_malloc(remap_size);

  size_t i;
  for (i = 0; i < remap_size; i ++) {
    remap_buffer[i] = buffer[i];
  }
  return 1;
}

static int ICACHE_FLASH_ATTR clear_remap(lua_State* L){
  if (remap_buffer){
    c_free(remap_buffer);
    remap_buffer=(char*)0;
    remap_size=0;
    lua_pushboolean(L,1);
  }else{
    lua_pushboolean(L,0);
  }
  return 1;
}

static int ICACHE_FLASH_ATTR get_remap(lua_State* L){
  if (remap_buffer){
    lua_pushlstring(L,remap_buffer,remap_size);
  }else{
    lua_pushboolean(L,1);
    NODE_ERROR("No Remap array");
  }
  return 1;
}


// Lua: ws2812.write(pin, "string")
// Byte triples in the string are interpreted as G R B values.
// This function does not corrupt your buffer.
//
// ws2812.write(4, string.char(0, 255, 0)) uses GPIO2 and sets the first LED red.
// ws2812.write(3, string.char(0, 0, 255):rep(10)) uses GPIO0 and sets ten LEDs blue.
// ws2812.write(4, string.char(255, 0, 0, 255, 255, 255)) first LED green, second LED white.
static int ICACHE_FLASH_ATTR ws2812_writegrb(lua_State* L) {
  const uint8_t pin = luaL_checkinteger(L, 1);
  size_t length;
  const char *buffer = luaL_checklstring(L, 2, &length);
  char *transfer = (char*)c_malloc(length);

  platform_gpio_mode(pin, PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_FLOAT);
  platform_gpio_write(pin, 0);

  // add brightness
  NODE_ERR("add brightness\n");
  size_t i;
  for (i = 0; i < length; i ++) {
    transfer[i] = buffer[i]*brightness;
  }
  NODE_ERR("finish brightness\n");

  os_delay_us(1);
  os_delay_us(1);

  os_intr_lock();
  const char * const end = transfer + length;
  while (transfer != end) {
    uint8_t mask = 0x80;
    while (mask) {
      ( *transfer & mask) ? send_ws_1(pin_num[pin]) : send_ws_0(pin_num[pin]);
      mask >>= 1;
    }
    ++transfer;
  }
  os_intr_unlock();
  // clean up the mess
  //c_free(transfer-length);
  lua_pushlstring(L,transfer-length,length);

  return 0;
}

#define MIN_OPT_LEVEL 2
#include "lrodefs.h"
const LUA_REG_TYPE ws2812_map[] =
{
  { LSTRKEY( "set_brightness" ), LFUNCVAL( set_brightness )},
  { LSTRKEY( "brightness" ), LFUNCVAL( get_brightness )},
  { LSTRKEY( "write" ), LFUNCVAL( ws2812_writegrb )},
  { LNILKEY, LNILVAL}
};

LUALIB_API int luaopen_ws2812(lua_State *L) {
  // TODO: Make sure that the GPIO system is initialized
  LREGISTER(L, "ws2812", ws2812_map);
  return 1;
}
