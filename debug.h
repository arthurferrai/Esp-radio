#ifndef __DEBUG_H_DEFINED__
#define __DEBUG_H_DEFINED__

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <Arduino.h>

// Debug buffer size
#define DEBUG_BUFFER_SIZE 100

//******************************************************************************************
//                                  D B G P R I N T                                        *
//******************************************************************************************
// Send a line of info to serial output.  Works like vsprintf(), but checks the BEDUg flag.*
// Print only if DEBUG flag is true.  Always returns the the formatted string.             *
//******************************************************************************************
char* dbgprint ( const char* format, ... )
{
  static char sbuf[DEBUG_BUFFER_SIZE] ;                // For debug lines
  va_list varArgs ;                                    // For variable number of params

  va_start ( varArgs, format ) ;                       // Prepare parameters
  vsnprintf ( sbuf, sizeof(sbuf), format, varArgs ) ;  // Format the message
  va_end ( varArgs ) ;                                 // End of using parameters

#ifdef DEBUG                                           // DEBUG on?
  {
    Serial.print ( "D: " ) ;                           // Yes, print prefix
    Serial.println ( sbuf ) ;                          // and the info
  }
#endif

  return sbuf ;                                        // Return stored string
}
#endif