#include <msp430.h>
#include "msprf24.h"
#include "nrf_userconfig.h"
#include "stdint.h"
#include "I2C.h"
#include "MPU6050.h"
#include <stdlib.h>
#include <stdio.h>

volatile unsigned int user;
float temp_C;
char temp[sizeof(float)];
int main()
{
    uint8_t addr[5];
    uint8_t buf[32];

    WDTCTL = WDTHOLD | WDTPW;
    DCOCTL = CALDCO_16MHZ;
    BCSCTL1 = CALBC1_16MHZ;
    BCSCTL2 = DIVS_1;  // SMCLK = DCOCLK/2
    // SPI (USCI) uses SMCLK, prefer SMCLK < 10MHz (SPI speed limit for nRF24 = 10MHz)

    // Red, Green LED used for status
    P1DIR |= 0x01;              // P1.6 changed used for I2C SCK
    P1OUT &= ~0x01;             // P1.6 changed used for I2C SCK


    i2c_init();
    mpu_init();

    user = 0xFE;

    /* Initial values for nRF24L01+ library config variables */
    rf_crc = RF24_EN_CRC | RF24_CRCO; // CRC enabled, 16-bit
    rf_addr_width      = 5;
    rf_speed_power     = RF24_SPEED_1MBPS | RF24_POWER_0DBM;
    rf_channel         = 120;

    msprf24_init();  // All RX pipes closed by default
    msprf24_set_pipe_packetsize(0, 32);
    msprf24_open_pipe(0, 1);  // Open pipe#0 with Enhanced ShockBurst enabled for receiving Auto-ACKs
        // Note: Pipe#0 is hardcoded in the transceiver hardware as the designated "pipe" for a TX node to receive
        // auto-ACKs.  This does not have to match the pipe# used on the RX side.

    // Transmit to 'rad01' (0x72 0x61 0x64 0x30 0x31)
    msprf24_standby();
    user = msprf24_current_state();
    addr[0] = 0xDE; addr[1] = 0xAD; addr[2] = 0xBE; addr[3] = 0xEF; addr[4] = 0x00;
    w_tx_addr(addr);
    w_rx_addr(0, addr);  // Pipe 0 receives auto-ack's, autoacks are sent back to the TX addr so the PTX node
                         // needs to listen to the TX addr on pipe#0 to receive them.

    while(1){
        __delay_cycles(800000);

        if(buf[0]=='0'){buf[0] = '1';buf[1] = '0';}
        else {buf[0] = '0';buf[1] = '1';}
        temp_C = get_temp();

        memcpy(temp, &temp_C, sizeof(float));

        buf[2] = temp[0];
        buf[3] = temp[1];
        buf[4] = temp[2];
        buf[5] = temp[3];

        w_tx_payload(32, buf);
        msprf24_activate_tx();
        LPM4;

        if (rf_irq & RF24_IRQ_FLAGGED) {
            rf_irq &= ~RF24_IRQ_FLAGGED;

            msprf24_get_irq_reason();
            if (rf_irq & RF24_IRQ_TX){
                P1OUT &= ~BIT0; // Red LED off
                P1OUT |= 0x40;  // Green LED on
            }
            if (rf_irq & RF24_IRQ_TXFAILED){
                P1OUT &= ~BIT6; // Green LED off
                P1OUT |= BIT0;  // Red LED on
            }

            msprf24_irq_clear(rf_irq);
            user = msprf24_get_last_retransmits();
        }
    }
    return 0;
}
