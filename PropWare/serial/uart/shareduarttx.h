/**
* @file        PropWare/serial/uart/shareduarttx.h
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

#pragma once

#include <PropWare/serial/uart/uarttx.h>

namespace PropWare {

/**
 * @brief   An easy-to-use, thread-safe class for simplex (transmit only) UART
 *          communication
 */
class SharedUARTTX : public UARTTX {
    public:
        /**
         * @brief   No-arg constructors are helpful when avoiding dynamic
         *          allocation
         */
        SharedUARTTX () :
                UARTTX() {
        }

        /**
         * @brief       Construct a UART instance capable of simplex serial
         *              communications
         *
         * @param[in]   tx  Bit mask used for the TX (transmit) pin
         */
        SharedUARTTX (const Port::Mask tx) :
                UARTTX(tx) {
        }

        virtual void set_tx_mask (const Port::Mask tx) {
            // Reset the old pin
            this->m_pin.set_dir_in();
            this->m_pin.clear();

            this->m_pin.set_mask(tx);
            this->m_pin.set();
        }

        virtual void send (uint16_t originalData) const {
            // Set pin as output
            this->m_pin.set();
            this->m_pin.set_dir_out();

            UARTTX::send(originalData);

            this->m_pin.set_dir_in();
        }

        virtual void send_array (char const array[], uint32_t words) const {
            // Set pin as output
            this->m_pin.set();
            this->m_pin.set_dir_out();

            UARTTX::send_array(array, words);

            this->m_pin.set_dir_in();
        }
};

}
