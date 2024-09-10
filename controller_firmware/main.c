#include <avr/io.h>
#include <util/delay.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <string.h>

#include "v-usb/usbdrv/usbdrv.h"
#include "../kbd_bl.h"

#define LED_PIN PB1

internal_status_t status_buffer[2];
unsigned char current_status_buffer_index = 0;

#define CURRENT_INTERNAL_STATUS (status_buffer[current_status_buffer_index])
#define CURRENT_STATUS (CURRENT_INTERNAL_STATUS.external_status)

#define INACTIVE_INTERNAL_STATUS (status_buffer[1 - current_status_buffer_index])
#define INACTIVE_STATUS (INACTIVE_INTERNAL_STATUS.external_status)

unsigned char usbMsg[8];

#define INIT_RECVDATA(rq)   req_to_recvdata = rq->bRequest; bytes_remaining = rq->wLength.word; offset = 0
uint16_t req_to_recvdata = KBD_BL_REQUEST_INVALID;
uint16_t bytes_remaining = 0;
uint16_t offset = 0;

void setupPWM() {
    // Set up timer/PWM items
    PLLCSR |= (1 << PLLE); //Enable PLL

    //Wait for PLL to lock
    while ((PLLCSR & (1<<PLOCK)) == 0x00)
    {
        // Do nothing until plock bit is set
    }

    // Enable clock source bit
    PLLCSR |= (1 << PCKE);

    DDRB |= _BV(LED_PIN);
    // put your setup code here, to run once:
    TCCR1 = 0;
    TCCR1 |= (1<<CS12) | (1<<PWM1A) | (1<<COM1A1); //start timer1, devide timer1 clock by 8, enable pwm, clear on match
    OCR1C = 0xFF; // maximum frequency
}

usbMsgLen_t usbFunctionSetup(uchar setupData[8])
{
    usbRequest_t *rq = (usbRequest_t *)setupData;   // cast to structured data for parsing
    switch(rq->bRequest){
        case KBD_BL_REQUEST_SET_TARGET_BRIGHTNESS:
            CURRENT_STATUS.target_brightness = rq->wValue.bytes[0];
            return 0;                           // no data block sent or received
        case KBD_BL_REQUEST_GET_ACTUAL_BRIGHTNESS:
            usbMsgPtr = usbMsg;
            usbMsg[0] = OCR1A;
            return 1;
        case KBD_BL_REQUEST_GET_TARGET_BRIGHTNESS:
            usbMsgPtr = usbMsg;
            usbMsg[0] = CURRENT_STATUS.target_brightness;
            return 1;
        case KBD_BL_REQUEST_GET_STATE_DATA:
            usbMsgPtr = (unsigned char*) &CURRENT_STATUS;
            return sizeof(CURRENT_STATUS);
        case KBD_BL_REQUEST_SET_STATE_DATA:
            // wIndex = offset into state data to start writing from
            if(rq->wLength.word <= sizeof(INACTIVE_STATUS) - rq->wIndex.word) {
                INIT_RECVDATA(rq);
                offset = rq->wIndex.word;
                // init internal status
                memset(&INACTIVE_INTERNAL_STATUS, 0, sizeof(INACTIVE_INTERNAL_STATUS));
                memcpy(&INACTIVE_STATUS, &CURRENT_STATUS, sizeof(INACTIVE_STATUS));
            }else{
                // invalid state data length, make sure recv stalls
                req_to_recvdata = KBD_BL_REQUEST_INVALID;
            }
            return USB_NO_MSG;
    }
    return 0;                               // ignore all unknown requests
}

/*
 * usbFunctionWrite: called when the host is writing data to us, i.e. we are reading
 *                      (this is confusingly named in v-usb)
 */
uchar usbFunctionWrite(uchar *data, uchar len){
    if(req_to_recvdata == KBD_BL_REQUEST_INVALID) // we don't want to receive right now
        return -1;
    if(bytes_remaining == 0)
        return 1;               /* end of transfer */
    if(len > bytes_remaining)
        len = bytes_remaining;

    switch(req_to_recvdata){
        case KBD_BL_REQUEST_SET_STATE_DATA:
            memcpy(((void*)(&INACTIVE_STATUS)) + offset, data, len);
            if(bytes_remaining == len){
                // switch buffers - this is last chunk
                current_status_buffer_index = 1 - current_status_buffer_index;
            }
            break;
        default:
            req_to_recvdata = KBD_BL_REQUEST_INVALID;
            return -1;
    }

    offset += len;
    bytes_remaining -= len;
    if(bytes_remaining == 0){
        req_to_recvdata = KBD_BL_REQUEST_INVALID;
    }
    return bytes_remaining == 0; /* return 1 if this was the last chunk */
}

int main(void) {
    // init status buffer
    memset(status_buffer, 0, sizeof(status_buffer));

    DDRB &= ~(_BV(USB_CFG_DPLUS_BIT) | _BV(USB_CFG_DMINUS_BIT)); // Make sure USB data pins are set as inputs
    PORTB &= ~(_BV(USB_CFG_DPLUS_BIT) | _BV(USB_CFG_DMINUS_BIT)); // Make sure USB data pins have pull-ups turned off

    cli();

    setupPWM();

    usbInit();

    usbDeviceDisconnect();
    uchar   i;
    i = 0;
    while(--i){             /* fake USB disconnect for > 250 ms */
        _delay_ms(10);
    }
    usbDeviceConnect();

    wdt_enable(WDTO_1S);
    sei(); // Enable global interrupts

    for (;;) {
        wdt_reset(); // Reset watchdog timer
        usbPoll();   // Poll USB events
    }

    return 0;
}

