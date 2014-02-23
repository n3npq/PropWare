/**
 * @file    mcp300x.c
 *
 * @project PropWare
 *
 * @author  David Zemon
 * @author  Collin Winans
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

// Includes
#include <mcp300x.h>

namespace PropWare {

MCP300x::MCP300x () {
    this->m_cs = -1;
    this->m_alwaysSetMode = 0;
}

int8_t MCP300x::start (const uint32_t mosi, const uint32_t miso,
        const uint32_t sclk, const uint32_t cs) {
    int8_t err;

    this->m_spi = SPI::getSPI();
    this->m_cs = cs;
    gpio_set_dir(cs, GPIO_DIR_OUT);
    gpio_pin_set(cs);

    if (!this->m_spi->is_running()) {
        check_errors(
                this->m_spi->start(mosi, miso, sclk, MCP300x::SPI_DEFAULT_FREQ, MCP300x::SPI_MODE, MCP300x::SPI_BITMODE));
    } else {
        check_errors(this->m_spi->set_mode(MCP300x::SPI_MODE));
        check_errors(this->m_spi->set_bit_mode(MCP300x::SPI_BITMODE));
    }

    return 0;
}

void MCP300x::always_set_spi_mode (const uint8_t alwaysSetMode) {
    this->m_alwaysSetMode = alwaysSetMode;
}

int8_t MCP300x::read (const MCP300x::Channel channel, uint16_t *dat) {
    int8_t err, options;

    options = MCP300x::START | MCP300x::SINGLE_ENDED | channel;
    options <<= 2; // One dead bit between output and input - see page 19 of datasheet

    if (this->m_alwaysSetMode) {
        check_errors(this->m_spi->set_mode(MCP300x::SPI_MODE));
        check_errors(this->m_spi->set_bit_mode(MCP300x::SPI_BITMODE));
    }

    gpio_pin_clear(this->m_cs);
    check_errors(this->m_spi->shift_out(MCP300x::OPTN_WIDTH, options));
    check_errors(this->m_spi->shift_in(MCP300x::DATA_WIDTH, dat, sizeof(*dat)));
    gpio_pin_set(this->m_cs);

    return 0;
}

int8_t MCP300x::read_diff (const MCP300x::ChannelDiff channels, uint16_t *dat) {
    int8_t err, options;

    options = MCP300x::START | MCP300x::DIFFERENTIAL | channels;
    options <<= 2; // One dead bit between output and input - see page 19 of datasheet

    if (this->m_alwaysSetMode) {
        check_errors(this->m_spi->set_mode(MCP300x::SPI_MODE));
        check_errors(this->m_spi->set_bit_mode(MCP300x::SPI_BITMODE));
    }

    gpio_pin_clear(this->m_cs);
    check_errors(this->m_spi->shift_out(MCP300x::OPTN_WIDTH, options));
    check_errors(this->m_spi->shift_in(MCP300x::DATA_WIDTH, dat, sizeof(*dat)));
    gpio_pin_set(this->m_cs);

    return 0;
}

}