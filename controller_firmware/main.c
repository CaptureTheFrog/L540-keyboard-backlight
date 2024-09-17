#define CONFIG_POWERSAVE

#include <avr/io.h>
#include <util/delay.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <string.h>

#ifdef CONFIG_POWERSAVE
#include <avr/power.h>
#include <avr/sleep.h>
#endif

#include "v-usb/usbdrv/usbdrv.h"
#include "../kbd_bl.h"

#define LED_PIN PB1

#define PRESCALE_DIV    (1 << ((TCCR1 & (1<<CS13 | 1<<CS12 | 1<<CS11 | 1<<CS10)) - 1))
#define PLL_PCK         (64000000 >> ((PLLCSR >> LSM) & 1))

volatile uint32_t usecs_interrupts_counter = 0;
volatile double time_per_cycle_us = 0.0;  // Approximate time per PWM cycle in us

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

double usecs() {
    return usecs_interrupts_counter * time_per_cycle_us;
}

void reset_usecs() {
    cli();
    usecs_interrupts_counter = 0;
    sei();
}

void setupPWM() {
    // Set up Timer 1 for PWM on PB1
    PLLCSR |= (1 << PLLE);  // Enable PLL
    while (!(PLLCSR & (1 << PLOCK)));  // Wait for PLL to lock
    PLLCSR |= (1 << PCKE);  // Enable clock source

    DDRB |= _BV(LED_PIN);  // Set LED_PIN as output
    TCCR1 = (1 << CS12) | (1 << PWM1A) | (1 << COM1A1);  // Clock/8, PWM on OC1A (PB1)

    OCR1C = 0xFF;  // max frequency

    // Calculate time per PWM cycle (in us)
    time_per_cycle_us = ((uint32_t)(OCR1C + 1) * PRESCALE_DIV * 1000000L) / PLL_PCK;  // Result in us

    TIMSK |= (1 << TOIE1);  // Enable Timer 1 overflow interrupt
}

ISR(TIMER1_OVF_vect) {
    usecs_interrupts_counter++;
}

uint32_t usecs() {
    uint32_t u;
    cli();
    u = usecs_counter;
    sei();
    return u;
}

void reset_usecs() {
    cli();
    usecs_counter = 0;
    sei();
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
                reset_usecs();
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

#define IS_FLAG_CURRENTLY_SET(flag) (KBD_BL_IS_FLAG_SET(CURRENT_STATUS, flag))
void updateLED(){
    if(IS_FLAG_CURRENTLY_SET(KBD_BL_FLAGS_HARDWARE_BLINK)){
        if((CURRENT_INTERNAL_STATUS.blink_state && usecs() >= CURRENT_STATUS.delay_on) ||
            (!CURRENT_INTERNAL_STATUS.blink_state && usecs() >= CURRENT_STATUS.delay_off)){
            CURRENT_INTERNAL_STATUS.blink_state = !CURRENT_INTERNAL_STATUS.blink_state;
            reset_usecs();
        }
        OCR1A = CURRENT_INTERNAL_STATUS.blink_state ? CURRENT_STATUS.target_brightness : CURRENT_STATUS.low_blink_target_brightness;
        return;
    }
    OCR1A = CURRENT_STATUS.target_brightness;
}

int main(void) {
    // init status buffer
    memset(status_buffer, 0, sizeof(status_buffer));




#ifdef CONFIG_POWERSAVE// Disable ADC
    ADCSRA &= ~(1 << ADEN); // disable the ADC

    DDRB = 0x00;  // set all IO as inputs
    PORTB = ~(_BV(USB_CFG_DPLUS_BIT) | _BV(USB_CFG_DMINUS_BIT)); // enable pullup on all IO except USB
    DIDR0 = ~(_BV(USB_CFG_DPLUS_BIT) | _BV(USB_CFG_DMINUS_BIT)); // turn off all digital inputs except USB

    GIMSK |= (1 << PCIE);      // Enable PCINT interrupt
    PCMSK |= _BV(USB_CFG_DPLUS_BIT) | _BV(USB_CFG_DMINUS_BIT);    // Enable PCINT for usb data lines

    // Set the sleep mode to Power-down
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_enable();

    // Disable unnecessary features
    power_adc_disable();
    power_usi_disable();
#else
    DDRB &= ~(_BV(USB_CFG_DPLUS_BIT) | _BV(USB_CFG_DMINUS_BIT)); // USB data pins are set as inputs
    PORTB &= ~(_BV(USB_CFG_DPLUS_BIT) | _BV(USB_CFG_DMINUS_BIT)); // Make sure USB data pins have pull-ups turned off
#endif



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

    reset_usecs();

    uint32_t loops_without_sof = 0;

    for (;;) {
        wdt_reset(); // Reset watchdog timer
        usbPoll();   // Poll USB events
        updateLED(); // Update LED value

        if(usbSofCount != 0) {
            usbSofCount = 0;
            loops_without_sof = 0;
        }else{
            if (loops_without_sof++ >= 100000) {

                // disconnect the LED from the PWM because PWM will freeze state during sleep
                TCCR1 &= ~(1 << COM1A1);
                PORTB &= ~(1 << LED_PIN);

                wdt_disable();
                sleep_cpu();
                wdt_enable(WDTO_1S);

                loops_without_sof = 0; // reset sof counter - a wakeup counts as activity
                setupPWM(); // reenable pwm
            }
        }
    }

    return 0;
}

