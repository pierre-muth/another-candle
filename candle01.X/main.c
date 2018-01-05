/*
 * File:   main.c
 * Author: muth
 * Device: PIC10F320
 * Created on 12 December 2017, 11:38
 */

// CONFIG 
#pragma config FOSC = INTOSC    // Oscillator Selection bits (INTOSC oscillator: CLKIN function disabled)
#pragma config BOREN = ON       // Brown-out Reset Enable (Brown-out Reset enabled)
//#pragma config WDTE = SWDTEN    // Watchdog Timer Enable (WDT controlled by the SWDTEN bit in the WDTCON register)
#pragma config WDTE = ON        // Watchdog Timer Enable (WDT enabled)
#pragma config PWRTE = OFF      // Power-up Timer Enable bit (PWRT disabled)
#pragma config MCLRE = OFF      // MCLR Pin Function Select bit (MCLR pin function is digital input, MCLR internally tied to VDD)
#pragma config CP = OFF         // Code Protection bit (Program memory code protection is disabled)
#pragma config LVP = OFF        // Low-Voltage Programming Enable (High-voltage on MCLR/VPP must be used for programming)
#pragma config LPBOR = ON       // Brown-out Reset Selection bits (BOR enabled)
#pragma config BORV = LO        // Brown-out Reset Voltage Selection (Brown-out Reset Voltage (Vbor), low trip point selected.)
#pragma config WRT = OFF        // Flash Memory Self-Write Protection (Write protection off)

#include <xc.h>
#include <pic10f320.h>

// CPU freq
#define _XTAL_FREQ  (8000000UL)

unsigned short lfsr = 0x5AD3;
unsigned short fb = 0;
unsigned char left = 0;
unsigned char right = 0;
unsigned char length = 127;
unsigned char target = 127;
unsigned char oscillate = 2;
unsigned char quietCycles = 0;
unsigned char recover = 0;

void interrupt isr(void) {
    if (left == target) {   // we reached the target
        if (oscillate) {   // are we in oscillating mode
            oscillate--;
            recover = 0;
            if (oscillate < 2) {
                target = 128;   // to recover normal brightness
            } else {
                if (oscillate % 2) {
                    target = 96;   // low value
                } else {
                    target = 224 - ((32-oscillate)*2);  // high value, decreasing  
                    length++;   // compensate intensity decreasing (not perfect)
                }
            }
        } else {
            fb = ((lfsr >> 1) ^ (lfsr >> 2)) & 1;   // randomize, x^15 + x^14 + 1 
            lfsr =  (lfsr >> 1) | (fb << 15);     // https://en.wikipedia.org/wiki/Linear-feedback_shift_register
            target = lfsr & 0x00FF;   // target between 0 to 255
            if (target < 16 && quietCycles > 8) {   // 16 chances over 255 to oscillate
                oscillate = ((lfsr & 0x1F00) >> 8) | 0x08;   //oscillate between 8 to 31 times
                length = 5;
                target = 128;
                quietCycles = 0;
                recover = 1;
            } else {
                length = ((lfsr & 0x7F00) >> 8) | 0x20;    //length between 31 and 127
                quietCycles++;
            }
        }
    }  
    
    // we have to increase or decrease to reach  the target
    if (left < target) left++;
    else left--;
    if (oscillate && !recover) right = left;   // oscillate the total intensity
    else right = ~left;   // or the flame position
    
    // set to periferals
    PWM1DCH = left;
    PWM2DCH = right;
    TMR0 = ~length;

    INTCONbits.TMR0IF = 0;  // clear timer0 flag
}

void main(void) {
    // clock conf, default is 8Mhz
//    OSCCONbits.IRCF = 0b111;  // osc 16Mhz
    
    // IO conf
    TRISAbits.TRISA0 = 0b0;      // output for PWM / 1 default
    TRISAbits.TRISA1 = 0b0;      // output for PWM / 1 default
//    TRISAbits.TRISA2 = 0b1;      // input / 1 default
    
    ANSELAbits.ANSA0 = 0b0;       // digital
    ANSELAbits.ANSA1 = 0b0;       // digital
//    ANSELAbits.ANSA2 = 0b1;       // analog / 1 default
    
    //ADC conf
    ADCONbits.CHS = 0b010;   // AN2 selected
    ADCONbits.ADON = 1;      // adc on
    ADCONbits.ADCS = 0b010;  // fosc/32 (2us /conv))
    
    // voltage ref conf, cannot be used as ADC reference on 10F320
//    FVRCONbits.ADFVR0 = 0b0; // 2.048 volt ref
//    FVRCONbits.ADFVR1 = 0b1; 
//    FVRCONbits.FVREN = 0b1;  // enable fvr
    
    // PWM conf
    PWM1CONbits.PWM1EN = 1; // enable
    PWM1CONbits.PWM1OE = 1; // output enable
    PWM2CONbits.PWM2EN = 1; // ''
    PWM2CONbits.PWM2OE = 1; // ''
    
    // Timer2 (pwm) conf
//    T2CONbits.T2CKPS = 0b00; // Prescaler 0 /default
//    PR2 = 0xFF;     // trm2 period / 0xFF default
    T2CONbits.TMR2ON = 1;
    
    // watchdog conf
    WDTCONbits.WDTPS = 0b01101;  // 8s whatchdog / 2s default
//    WDTCONbits.SWDTEN = 1;  // enable watchdog 
    
    // Timer0 conf
    OPTION_REGbits.T0CS = 0; // clock is Fosc/4
    OPTION_REGbits.PSA = 0;  // use the prescaler
    OPTION_REGbits.PS = 0b111;  // prescaler 1:256
    
    //interrupt conf
//    INTCONbits.TMR0IF = 0;  // clear timer0 flag
    INTCONbits.TMR0IE = 1;  // enable timer0 interrupt
    INTCONbits.GIE = 1;     // general enable interrupt
    
    // checking outsite brightness
    while(1) {
        ADCONbits.GO_nDONE = 1;   // ADC start convertion
        while(ADCONbits.GO_nDONE){  // wait it's done
//            NOP();   // not needed
        }
        if(ADRES > 0xB0){   // if it's too bright outside, off and sleep
            TRISAbits.TRISA0 = 0b1;      // input, high-Z 
            TRISAbits.TRISA1 = 0b1;      // input, high-Z 
            SLEEP();
        } else {
            TRISAbits.TRISA0 = 0b0;      // output 
            TRISAbits.TRISA1 = 0b0;      // output 
        }
        CLRWDT();   // reset watchdog
//        __delay_ms(20);
    }
}