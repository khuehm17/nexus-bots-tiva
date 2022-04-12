#include "debug_printf.h"

void debug_init()
{
    Serial.begin(115200);
}

/*
 *  Print standard C printf format string over Serial
 */
void debug_printf(const char *format, ...)
{
  static char dbgMsg_buffer[DBG_MSG_SIZE];
  va_list args;

  va_start(args, format);
  vsnprintf(dbgMsg_buffer, DBG_MSG_SIZE, format, args);
  va_end(args);

  Serial.println(dbgMsg_buffer);
}
