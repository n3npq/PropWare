/**
 * @file        uart.h
 *
 * @project     PropWare
 *
 * @author      David Zemon
 *
 * @copyright
 * The MIT License (MIT)<br>
 * <br>Copyright (c) 2013 David Zemon<br>
 * <br>Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:<br>
 * <br>The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.<br>
 * <br>THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef PROPWARE_UART_H_
#define PROPWARE_UART_H_

#include <sys/thread.h>
#include <PropWare/PropWare.h>
#include <PropWare/pin.h>
#include <PropWare/port.h>

namespace PropWare {

/**
 * @brief   Abstract base class for all UART devices
 *
 * Configurable with the following options:
 * <ul><li>Data width: 1-16 bits</li>
 * <li>Parity: No parity, odd parity, even parity</li>
 * <li>Stop bits: Any number of stop bits between 1 and 14
 * </li></ul>
 *
 * @note    Total number of bits within start, data, parity, and stop cannot
 *          exceed 32. For instance, a configuration of 16 data bits, even or
 *          odd parity, and 2 stop bits would be 1 + 16 + 1 + 2 = 20 (this is
 *          allowed). A configuration of 30 data bits, no parity, and 2 stop
 *          bits would be 1 + 30 + 2 = 33 (not allowed).
 *
 * @note    No independent cog is needed for execution and therefore all
 *          communication methods are blocking (cog execution will not return
 *          from the method until the relevant data has been received/sent)
 */
class UART {
    public:
        typedef enum {
            /** No parity */NO_PARITY,
            /** Even parity */EVEN_PARITY,
            /** Odd parity */ODD_PARITY
        } Parity;

        /** Number of allocated error codes for UART */
#define UART_ERRORS_LIMIT            16
        /** First UART error code */
#define UART_ERRORS_BASE             64

        /**
         * Error codes - Proceeded by SD, SPI, and HD44780
         */
        typedef enum {
            /** No errors; Successful completion of the function */
            NO_ERROR = 0,
            /** First error code for PropWare::UART */
            BEG_ERROR = UART_ERRORS_BASE,
            /** The requested baud rate is too high */
            BAUD_TOO_HIGH = BEG_ERROR,
            /** The requested data width is not between 1 and 16 (inclusive) */
            INVALID_DATA_WIDTH,
            /** Valid stop bit width can not be 0 */
            INVALID_STOP_BIT_WIDTH,
            /** Last error code used by PropWare::UART */
            END_ERROR = PropWare::UART::INVALID_STOP_BIT_WIDTH
        } ErrorCode;

    public:
        static const uint8_t DEFAULT_DATA_WIDTH = 8;
        static const PropWare::UART::Parity DEFAULT_PARITY = NO_PARITY;
        static const uint8_t DEFAULT_STOP_BIT_WIDTH = 1;
        static const uint32_t DEFAULT_BAUD = 115200;

        static const uint32_t MAX_BAUD = 122000;

    public:
        /**
         * @brief       Set the pin mask for TX pin
         *
         * @param[in]   Pin mask for the transmit (TX) pin
         */
        void set_tx_mask (const PropWare::Port::Mask tx) {
            this->m_tx.set_mask(tx);
            this->m_tx.set();
            this->m_tx.set_dir(PropWare::Port::OUT);
        }

        /**
         * @brief   Retrieve the currently configured transmit (TX) pin mask
         *
         * @return  Pin mask of the transmit (TX) pin
         */
        PropWare::Port::Mask get_tx_mask () const {
            return this->m_tx.get_mask();
        }

        /**
         * @brief       Set the number of bits for each word of data
         *
         * @param[in]   dataWidth   Typical values are between 5 and 9, but any
         *                          value between 1 and 16 is valid
         *
         * @return      Generally 0; PropWare::UART::INVALID_DATA_WIDTH will be
         *              returned if dataWidth is not between 1 and 16
         */
        virtual PropWare::ErrorCode set_data_width (const uint8_t dataWidth) {
            if (1 > dataWidth || dataWidth > 16)
                return PropWare::UART::INVALID_DATA_WIDTH;

            this->m_dataWidth = dataWidth;

            this->m_dataMask = 0;
            for (uint8_t i = 0; i < this->m_dataWidth; ++i)
                this->m_dataMask |= 1 << i;

            this->set_parity_mask();
            this->set_total_bits();

            return PropWare::UART::NO_ERROR;
        }

        /**
         * @brief   Retrieve the currently configured data width
         *
         * @return  Returns a numbers between 1 and 16, inclusive
         */
        uint8_t get_data_width () const {
            return this->m_dataWidth;
        }

        /**
         * @brief       Set the parity configuration
         *
         * @param[in]   No parity, even or odd parity can be selected
         */
        virtual void set_parity (const PropWare::UART::Parity parity) {
            this->m_parity = parity;
            this->set_parity_mask();
            this->set_stop_bit_mask();
            this->set_total_bits();
        }

        /**
         * @brief   Retrieve the current parity configuration
         *
         * @return  Current parity configuration
         */
        PropWare::UART::Parity get_parity () const {
            return this->m_parity;
        }

        /**
         * @brief       Set the number of stop bits used
         *
         * @param[in]   Typically either 1 or 2, but can be any number between 1
         *              and 14
         *
         * @return      Returns 0 upon success; Failure can occur when an
         *              invalid value is passed into stopBitWidth
         */
        PropWare::ErrorCode set_stop_bit_width (const uint8_t stopBitWidth) {
            // Error checking
            if (0 == stopBitWidth || stopBitWidth > 14)
                return PropWare::UART::INVALID_STOP_BIT_WIDTH;

            this->m_stopBitWidth = stopBitWidth;

            this->set_stop_bit_mask();

            this->set_total_bits();

            return NO_ERROR;
        }

        /**
         * @brief   Retrieve the current number of stop bits in use
         *
         * @return  Returns a number between 1 and 14 representing the number of
         *          stop bits
         */
        uint8_t get_stop_bit_width () const {
            return this->m_stopBitWidth;
        }

        /**
         * @brief       Set the baud rate
         *
         * @note        Actual baud rate will be approximate due to integer math
         *
         * @param[in]   baudRate    A value between 1 and
         *                          PropWare::UART::MAX_BAUD representing the
         *                          desired baud rate
         *
         * @return      Returns 0 upon success; PropWare::UART::BAUD_TOO_HIGH
         *              when baudRate is set too high for the Propeller's clock
         *              frequency
         */
        PropWare::ErrorCode set_baud_rate (const uint32_t baudRate) {
            if (PropWare::UART::MAX_BAUD < baudRate)
                return PropWare::UART::BAUD_TOO_HIGH;

            this->m_bitCycles = CLKFREQ / baudRate;
            return NO_ERROR;
        }

        /**
         * @brief   Retrieve the current buad rate
         *
         * @return  Returns an approximation  of the current baud rate; Value is
         *          not exact due to integer math
         */
        uint32_t get_baud_rate () const {
            return CLKFREQ / this->m_bitCycles;
        }

        /**
         * @brief       Send a word of data out the serial port
         *
         * @pre         Note to UART developers, not users: this->m_tx must be
         *              already configured as output
         *
         * @note        The core loop is taken directly from PropGCC's putchar()
         *              function in tinyio; A big thanks to the PropGCC team for
         *              the simple and elegant algorithm!
         *
         * @param[in]   originalData    Data word to send out the serial port
         */
        HUBTEXT virtual void send (uint16_t originalData) const {
            uint32_t wideData = originalData;

            // Add parity bit
            if (PropWare::UART::EVEN_PARITY == this->m_parity) {
                __asm__ volatile("test %[_data], %[_dataMask] wc \n\t"
                        "muxc %[_data], %[_parityMask]"
                        : [_data] "+r" (wideData)
                        : [_dataMask] "r" (this->m_dataMask),
                        [_parityMask] "r" (this->m_parityMask));
            } else if (PropWare::UART::ODD_PARITY == this->m_parity) {
                __asm__ volatile("test %[_data], %[_dataMask] wc \n\t"
                        "muxnc %[_data], %[_parityMask]"
                        : [_data] "+r" (wideData)
                        : [_dataMask] "r" (this->m_dataMask),
                        [_parityMask] "r" (this->m_parityMask));
            }

            // Add stop bits
            wideData |= this->m_stopBitMask;

            // Add start bit
            wideData <<= 1;

            uint32_t waitCycles = CNT + this->m_bitCycles;
            for (uint8_t i = 0; i < this->m_totalBits; i++) {
                waitCycles = waitcnt2(waitCycles, this->m_bitCycles);

                // if (value & 1) OUTA |= this->m_tx else OUTA &= ~this->m_tx; value = value >> 1;
                __asm__ volatile("shr %[_data],#1 wc \n\t"
                        "muxc outa, %[_mask]"
                        : [_data] "+r" (wideData)
                        : [_mask] "r" (this->m_tx.get_mask()));
            }
        }

    protected:
        /**
         * @brief   Set default values for all configuration parameters; TX mask
         *          must still be set before it can be used
         */
        UART () {
            this->set_data_width(UART::DEFAULT_DATA_WIDTH);
            this->set_parity(UART::DEFAULT_PARITY);
            this->set_stop_bit_width(UART::DEFAULT_STOP_BIT_WIDTH);
            this->set_baud_rate(UART::DEFAULT_BAUD);
        }

        /**
         * @brief   Create a stop bit mask and adjust it shift it based on the
         *          current value of parity
         */
        void set_stop_bit_mask () {
            // Create the mask to the far right
            this->m_stopBitMask = 1;
            for (uint8_t i = 0; i < this->m_stopBitWidth - 1; ++i)
                this->m_stopBitMask |= this->m_stopBitMask << 1;

            // Shift the mask into position (taking into account the current
            // parity settings)
            this->m_stopBitMask <<= this->m_dataWidth;
            if (PropWare::UART::NO_PARITY != this->m_parity)
                this->m_stopBitMask <<= 1;
        }

        /**
         * @brief   Create the parity mask; Takes into account the width of the
         *          data
         */
        void set_parity_mask () {
            this->m_parityMask = 1 << this->m_dataWidth;
        }

        /**
         * @brief       Determine the total number of bits shifted out or in
         *
         * @detailed    Takes into account the start bit, the width of the data,
         *              if there is a parity bit and the number of stop bits
         */
        void set_total_bits () {
            // Total bits = start + data + parity + stop bits
            this->m_totalBits = 1 + this->m_dataWidth + this->m_stopBitWidth;
            if (PropWare::UART::NO_PARITY != this->m_parity)
                ++this->m_totalBits;
        }

    protected:
        PropWare::Pin m_tx;
        uint8_t m_dataWidth;
        uint16_t m_dataMask;
        PropWare::UART::Parity m_parity;
        uint16_t m_parityMask;
        uint8_t m_stopBitWidth;
        uint32_t m_stopBitMask;  // Does not take into account parity bit!
        uint32_t m_bitCycles;
        uint8_t m_totalBits;
};

/**
 * @brief   An easy-to-use class for simplex (transmit only) UART communication
 */
class SimplexUART: public PropWare::UART {
    public:
        /**
         * @brief   No-arg constructors are helpful when avoiding dynamic
         *          allocation
         */
        SimplexUART () :
                PropWare::UART() {
        }

        /**
         * @brief       Construct a UART instance capable of simplex serial
         *              communications
         *
         * @param[in]   Bit mask used for the TX (transmit) pin
         */
        SimplexUART (const PropWare::Port::Mask tx) :
                PropWare::UART() {
            this->set_tx_mask(tx);
        }
};

/**
 * @brief   Full duplex UART communication module
 *
 * Because this class does not use an independent cog for receiving,
 * "Full duplex" may be an exaggeration. Though two separate pins can be
 * used for communication, transmitting and receiving can not happen
 * simultaneously, all receiving calls are indefinitely blocking and there is no
 * receive buffer (data sent to the Propeller will be ignored if execution is
 * not in the receive() method) PropWare::FullDuplexUART::receive() will not
 * return until after the RX pin is low and all data, parity (if applicable) and
 * stop bits have been read.
 */
class FullDuplexUART: public PropWare::SimplexUART {
    public:
        /**
         * @see PropWare::SimplexUART::SimplexUART()
         */
        FullDuplexUART () :
                PropWare::SimplexUART() {
            this->set_data_width(this->m_dataWidth);
        }

        /**
         * @brief       Initialize a UART module with both pin masks
         *
         * @param[in]   tx  Pin mask for TX (transmit) pin
         * @param[in]   rx  Pin mask for RX (receive) pin
         */
        FullDuplexUART (const PropWare::Port::Mask tx,
                const PropWare::Port::Mask rx) :
                PropWare::SimplexUART(tx) {
            this->set_data_width(this->m_dataWidth);

            // Set rx direction second first so that, in the case of
            // half-duplex, the pin floats high
            this->set_rx_mask(rx);
        }

        /**
         * @brief       Set the pin mask for RX pin
         *
         * @param[in]   Pin mask for the receive (RX) pin
         */
        void set_rx_mask (const PropWare::Port::Mask rx) {
            this->m_rx.set_mask(rx);
            this->m_rx.set_dir(PropWare::Port::IN);
        }

        /**
         * @brief   Retrieve the currently configured receive (RX) pin mask
         *
         * @return  Pin mask of the receive (RX) pin
         */
        PropWare::Port::Mask get_rx_mask () const {
            return this->m_rx.get_mask();
        }

        /**
         * @see PropWare::UART::set_data_width()
         */
        virtual PropWare::ErrorCode set_data_width (const uint8_t dataWidth) {
            PropWare::ErrorCode err = this->UART::set_data_width(dataWidth);
            if (err)
                return err;

            this->set_msb_mask();
            this->set_receivable_bits();

            return 0;
        }

        /**
         * @see PropWare::UART::set_parity()
         */
        virtual void set_parity (const PropWare::UART::Parity parity) {
            this->UART::set_parity(parity);
            this->set_msb_mask();
            this->set_receivable_bits();
        }

        /**
         * @brief   Receive one word of data; Will block until word is received
         *
         * Cog execution will be blocked by this call and there is no timeout;
         * Execution will not resume until the RX pin is driven low
         *
         * @pre     RX pin mask must be set
         *
         * @return  Data word will be returned unless parity is value is
         *          incorrect; An invalid parity bit will result in -1 being
         *          returned
         */
        HUBTEXT virtual uint32_t receive () const {
            uint32_t rxVal;
            uint32_t evenParityResult;
            uint32_t wideParityMask = this->m_parityMask;
            uint32_t wideDataMask = this->m_dataMask;

            /* wait for a start bit */
            this->m_rx.wait_until_low();

            /* sync for one half bit */
            uint32_t waitCycles = CNT + (this->m_bitCycles >> 1)
                    + this->m_bitCycles;

            for (uint8_t i = 0; i < this->m_receivableBits; i++) {
                waitCycles = waitcnt2(waitCycles, this->m_bitCycles);

                // value = ( (0 != (_INA & rxmask)) << 7) | (value >> 1);
                __asm__ volatile("shr %[_value],# 1\n\t"
                        "test %[_mask],ina wz \n\t"
                        "muxnz %[_value], %[_msbMask]"
                        : [_value] "+r" (rxVal)
                        : [_mask] "r" (this->m_rx.get_mask()),
                        [_msbMask] "r" (this->m_msbMask));
            }

            // Wait for the stop bit
            this->m_rx.wait_until_high();

            // Check parity bit
            if (this->m_parity) {
                evenParityResult = 0;
                __asm__ volatile("test %[_data], %[_dataMask] wc \n\t"
                        "muxc %[_parityResult], %[_parityMask]"
                        : [_parityResult] "+r" (evenParityResult)
                        : [_data] "r" (rxVal),
                        [_dataMask] "r" (wideDataMask),
                        [_parityMask] "r" (wideParityMask));

                if (PropWare::UART::EVEN_PARITY == this->m_parity) {
                    if (evenParityResult != (rxVal & this->m_parityMask))
                        return -1;
                } else if (evenParityResult == (rxVal & this->m_parityMask))
                    return -1;
            }

            return rxVal & wideDataMask;
        }

    protected:
        /**
         * @brief   Set a bit-mask for the data word's MSB (assuming LSB is bit
         *          0 - the start bit is not taken into account)
         */
        void set_msb_mask () {
            if (this->m_parity)
                this->m_msbMask =
                        (PropWare::Port::Mask) (1 << this->m_dataWidth);
            else
                this->m_msbMask = (PropWare::Port::Mask) (1
                        << (this->m_dataWidth - 1));
        }

        /**
         * @brief   Set the number of receivable bits - based on data width and
         *          parity selection
         */
        void set_receivable_bits () {
            if (this->m_parity)
                this->m_receivableBits = this->m_dataWidth + 1;
            else
                this->m_receivableBits = this->m_dataWidth;
        }

    protected:
        PropWare::Pin m_rx;
        PropWare::Port::Mask m_msbMask;
        uint8_t m_receivableBits;
};

/**
 * @brief   Simple wrapper for full duplex that provides half duplex capability
 *
 * It is important to note that, just like PropWare::FullDuplexUART, receiving
 * data is an indefinitely blocking call
 */
class HalfDuplexUART: public PropWare::FullDuplexUART {
    public:
        /**
         * @see PropWare::SimplexUART::SimplexUART()
         */
        HalfDuplexUART () :
                FullDuplexUART() {
        }

        /**
         * @see PropWare::FullDuplexUART::FullDuplexUART()
         */
        HalfDuplexUART (const PropWare::Port::Mask pinMask) :
                FullDuplexUART(pinMask, pinMask) {
        }

        /**
         * @see PropWare::UART::send()
         */
        HUBTEXT virtual void send (uint16_t originalData) {
            this->m_tx.set_dir(PropWare::Port::OUT);
            this->FullDuplexUART::send(originalData);
            this->m_tx.set_dir(PropWare::Port::IN);
        }

        /**
         * @see PropWare::FullDuplexUART::receive()
         */
        HUBTEXT virtual uint32_t receive () {
            this->m_rx.set_dir(PropWare::Port::IN);
            return this->FullDuplexUART::receive();
        }
};

}

#endif /* PROPWARE_UART_H_ */