#include "../include/parser.h"
#include "../include/util.h"
#include "../include/uart.h"
#include <stdint.h>
#include <string.h>


/*
    Supported commands:
    - RPM=(9-digit integer)
    - DIR=(CW or CCW)
    - {optional} SPR=(9-digit integer)
    - INFO

    Note: supports numbers up to 9 digits
*/


static void error_reset(void) {
    RPM = 0;
    SPR = 200;
    DIR = 0;
}


static void tokenize(void) {
    for (uint8_t i = 0; i < INDEX; i++) {
        // null-terminate commands
        if (commands[i] == '\n' || commands[i] == '\r' || commands[i] == '=') commands[i] = '\0';
    }
}


static uint32_t string_to_int(char* string) {
    uint8_t digits = strlen(string);
    if (digits > 9) {
        parseErr = 1;
        return 0;
    }

    uint32_t num = 0;

    for (uint32_t i = 0; i < digits; i++) {
        int16_t temp = string[i] - '0';

        if (temp < 0 || temp > 9) {
            parseErr = 1;
            return 0;
        }

        num = (num * 10) + temp;
    }

    return num;
}


void parse(void) {
    /* 
        parser does not allow multiple instances of the same command 
        RPM must be set by the user
    */

    tokenize();
    parseErr = 0;

    uint8_t RPM_detect = 0;
    uint8_t SPR_detect = 0;
    uint8_t DIR_detect = 0;
    uint8_t INFO_detect = 0;

    for (uint8_t i = 0; i < INDEX; i++) {
        // skip null characters
        if (commands[i] == '\0') continue;

        if (!strcmp((const char*)&commands[i], "INFO")) {
            /* INFO command must be entered individually */
            if (RPM_detect || DIR_detect || SPR_detect) goto Exit;

            INFO_detect = 1;

            // skip to next command
            while (i < INDEX && commands[i]) i++;
            while (i < INDEX && !commands[i+1]) i++;

        } else if (!strcmp((const char*)&commands[i], "RPM")) {
            // detect multiple RPM commands
            if (RPM_detect || INFO_detect) goto Exit;

            i += 4;
            if (i >= INDEX) goto Exit;

            // detect syntax errors
            if (!commands[i]) goto Exit;

            RPM = string_to_int((char *)&commands[i]);               // read value

            // detect any errors
            if (parseErr) goto Exit;

            // define max limit for RPM
            if (RPM > RPM_MAX) goto Exit;

            RPM_detect = 1;

            // skip to next command
            while (i < INDEX && commands[i]) i++;
            while (i < INDEX && !commands[i+1]) i++;

        } else if (!strcmp((const char*)&commands[i], "DIR")) {
            // detect multiple DIR commands
            if (DIR_detect || INFO_detect) goto Exit;

            i += 4;
            if (i >= INDEX) goto Exit;

            // detect syntax errors
            if (!commands[i]) goto Exit;

            if (!strcmp((const char*)&commands[i], "CW")) DIR = 0;
            else if (!strcmp((const char*)&commands[i], "CCW")) DIR = 1;
            else goto Exit;

            DIR_detect = 1;

            // skip to next command
            while (i < INDEX && commands[i]) i++;
            while (i < INDEX && !commands[i+1]) i++;

        } else if (!strcmp((const char*)&commands[i], "SPR")) {
            // detect multiple SPR commands
            if (SPR_detect || INFO_detect) goto Exit;

            i += 4;
            if (i >= INDEX) goto Exit;

            // detect syntax errors
            if (!commands[i]) goto Exit;

            SPR = string_to_int((char *)&commands[i]);               // read value

            // detect any errors
            if (parseErr) goto Exit;

            // define max/min limit for SPR
            if (SPR > SPR_MAX || !SPR) goto Exit;

            SPR_detect = 1;

            // skip to next command
            while (i < INDEX && commands[i]) i++;
            while (i < INDEX && !commands[i+1]) i++;

        // detect commands errors
        } else goto Exit;
    }

    if (INFO_detect) {
        /* handle sending data over UART */
        
        UART0_send(RPM);
        UART0_send(DIR);
        UART0_send(SPR);
    }

    // use default values if commands not entered
    if (!RPM) parseErr = 1;

    // allow overwrite of parsed commands
    INDEX = 0;
    if (!parseErr) BLINKER = 0x04;
    return;

Exit:
    error_reset();
    parseErr = 1;
    INDEX = 0;
}
