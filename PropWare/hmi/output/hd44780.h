/**
 * @file        PropWare/hmi/output/hd44780.h
 *
 * @author      David Zemon
 * @author      Collin Winans
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

#include <PropWare/PropWare.h>
#include <PropWare/gpio/pin.h>
#include <PropWare/gpio/simpleport.h>
#include <PropWare/hmi/output/printcapable.h>
#include <PropWare/hmi/output/printer.h>

namespace PropWare {

/**
 * @brief       Support for the common "character LCD" modules using the HD44780
 *              controller for the Parallax Propeller
 *
 * @note        Does not natively support 40x4 or 24x4 character displays
 */
class HD44780: public PrintCapable {
    public:
        /**
         * @brief   LCD databus width
         */
        enum class BusWidth {
                /** 4-bit mode */WIDTH4 = 4,
                /** 8-bit mode */WIDTH8 = 8,
        };

        /**
         * @brief   Supported LCD dimensions; Used for determining cursor placement
         *
         * @note    There are two variations of 16x1 character LCDs; if you're
         *          unsure which version you have, try 16x1_1 first, it is more
         *          common. 16x1_1 uses both DDRAM lines of the controller,
         *          8-characters on each line; 16x1_2 places all 16 characters
         *          are a single line of DDRAM.
         */
        enum class Dimensions {
                /** 8x1 */        DIM_8x1,
                /** 8x2 */        DIM_8x2,
                /** 8x2 */        DIM_8x4,
                /** 16x1 mode 1 */DIM_16x1_1,
                /** 16x1 mode 2 */DIM_16x1_2,
                /** 16x2 */       DIM_16x2,
                /** 16x2 */       DIM_16x4,
                /** 20x1 */       DIM_20x1,
                /** 20x2 */       DIM_20x2,
                /** 20x2 */       DIM_20x4,
                /** 24x1 */       DIM_24x1,
                /** 24x2 */       DIM_24x2,
                /** 40x1 */       DIM_40x1,
                /** 40x2 */       DIM_40x2,
        };

        /** Number of allocated error codes for HD44780 */
#define HD44780_ERRORS_LIMIT            16
        /** First HD44780 error code */
#define HD44780_ERRORS_BASE             48

        /**
         * Error codes - Proceeded by SD, SPI
         */
        typedef enum {
            /** No error */          NO_ERROR          = 0,
            /** First HD44780 error */BEG_ERROR = HD44780_ERRORS_BASE,
            /** HD44780 Error 0 */   INVALID_CTRL_SGNL = HD44780::BEG_ERROR,
            /** HD44780 Error 1 */   INVALID_DIMENSIONS,
            /** Last HD44780 error */END_ERROR         = HD44780::INVALID_DIMENSIONS
        } ErrorCode;

    protected:
        /**
         * Store metadata on the LCD device to determine when line-wraps should
         * and shouldn't occur
         */
        typedef struct {
            /** How many characters can be displayed on a single row */
            uint8_t charRows;
            /** How many characters can be displayed in a single column */
            uint8_t charColumns;
            /**
             * How many contiguous bytes of memory per visible character row
             */
            uint8_t ddramCharRowBreak;
            /** Last byte of memory used in each DDRAM line */
            uint8_t ddramLineEnd;
        } MemMap;

        static const uint_fast8_t ESCAPE_SEQUENCE_BUFFER_LENGTH = 32;

    public:
        /** Number of spaces inserted for '\\t' */
        static const uint8_t TAB_WIDTH = 4;

        /**
         * @name    Commands
         * @note    Must be combined with arguments below to create a parameter
         *          for the HD44780
         */
        static const uint8_t CLEAR          = BIT_0;
        static const uint8_t RET_HOME       = BIT_1;
        static const uint8_t ENTRY_MODE_SET = BIT_2;
        static const uint8_t DISPLAY_CTRL   = BIT_3;
        static const uint8_t SHIFT          = BIT_4;
        static const uint8_t FUNCTION_SET   = BIT_5;
        static const uint8_t SET_CGRAM_ADDR = BIT_6;
        static const uint8_t SET_DDRAM_ADDR = BIT_7;
        /**@}*/

        /**
         * @name    Entry mode arguments
         * @{
         */
        static const uint8_t SHIFT_INC = BIT_1;
        static const uint8_t SHIFT_EN  = BIT_0;
        /**@}*/

        /**
         * @name    Display control arguments
         * @{
         */
        static const uint8_t DISPLAY_PWR = BIT_2;
        static const uint8_t CURSOR      = BIT_1;
        static const uint8_t BLINK       = BIT_0;
        /**@}*/

        /**
         * @name    Cursor/display shift arguments
         * @{
         */
        static const uint8_t SHIFT_DISPLAY = BIT_3;  // 0 = shift cursor
        static const uint8_t SHIFT_RIGHT   = BIT_2;  // 0 = shift left
        /**@}*/

        /**
         * @name Function set arguments
         * @{
         */
        /**
         * 0 = 4-bit mode
         */
        static const uint8_t FUNC_8BIT_MODE  = BIT_4;
        /**
         * 0 = "1-line" mode - use 2-line mode for 2- and 4-line displays
         */
        static const uint8_t FUNC_2LINE_MODE = BIT_3;
        /**
         * 0 = 5x8 dot mode
         */
        static const uint8_t FUNC_5x10_CHAR  = BIT_2;
        /**@}*/

    public:
        /**
         * @brief       Construct an LCD object
         *
         * @param[in]   lsbDataPin  Pin mask for the least significant pin of the data port
         * @param[in]   rs          Pin mask connected to the `register select` control pin of the LCD driver
         * @param[in]   rw          Pin mask connected to the `read/write` control pin of the LCD driver
         * @param[in]   en          Pin mask connected to the `enable` control pin of the LCD driver
         * @param[in]   bitMode     Select between whether the parallel bus is using 4 or 8 pins
         * @param[in]   dimensions  Dimensions of your LCD device. Most common is HD44780::DIM_16x2
         * @param[in]   showCursor  Determines if the cursor on the display device will be visible
         */
        HD44780 (const Pin::Mask lsbDataPin, const Pin::Mask rs, const Pin::Mask rw, const Pin::Mask en,
                 const HD44780::BusWidth bitMode, const HD44780::Dimensions dimensions,
                 const bool showCursor = false)
            : m_dataPort(lsbDataPin, static_cast<uint8_t>(bitMode), Port::Dir::OUT),
              m_rs(rs, Pin::Dir::OUT),
              m_rw(rw, Pin::Dir::OUT),
              m_en(en, Pin::Dir::OUT),
              m_bitMode(bitMode),
              m_dimensions(dimensions),
              m_inEscapeSequence(false),
              m_showCursor(showCursor) {
            this->m_curPos.row = 0;
            this->m_curPos.col = 0;

            // Save all control signal pin masks
            this->m_rs.clear();
            this->m_rw.clear();
            this->m_en.clear();

            // Save the modes
            this->generate_mem_map();
        }

        /**
         * @brief       Initialize an HD44780 LCD display
         *
         * @note        A 250 ms delay is called while the LCD does internal
         *              initialization
         *
         * @return      Returns 0 upon success, otherwise error code
         */
        void start () {
            uint8_t arg;

            // Wait for a couple years until the LCD has finished internal initialization
            waitcnt(250 * MILLISECOND + CNT);

            // Begin init routine:
            if (BusWidth::WIDTH8 == this->m_bitMode)
                arg = 0x30;
            else
                /* Implied: "if (HD44780::WIDTH4 == this->m_bitMode)" */
                arg = 0x3;

            this->m_dataPort.write(arg);
            this->clock_pulse();
            waitcnt(100 * MILLISECOND + CNT);

            this->clock_pulse();
            waitcnt(100 * MILLISECOND + CNT);

            this->clock_pulse();
            waitcnt(10 * MILLISECOND + CNT);

            if (BusWidth::WIDTH4 == this->m_bitMode) {
                this->m_dataPort.write(0x2);
                this->clock_pulse();
            }

            // Default functions during initialization
            arg = PropWare::HD44780::FUNCTION_SET;
            if (BusWidth::WIDTH8 == this->m_bitMode)
                arg |= PropWare::HD44780::FUNC_8BIT_MODE;
            arg |= PropWare::HD44780::FUNC_2LINE_MODE;
            this->cmd(arg);

            // Turn off display shift (set cursor shift) and leave default of
            // shift-left
            arg = PropWare::HD44780::SHIFT;
            this->cmd(arg);

            if (this->m_showCursor)
                this->show_cursor();
            else
                this->hide_cursor();

            // Set cursor to auto-increment upon writing a character
            arg = PropWare::HD44780::ENTRY_MODE_SET | PropWare::HD44780::SHIFT_INC;
            this->cmd(arg);

            this->clear();
        }

        /**
         * @brief   Clear the LCD display and return cursor to home
         */
        void clear (void) {
            this->cmd(PropWare::HD44780::CLEAR);
            this->m_curPos.row = 0;
            this->m_curPos.col = 0;
            waitcnt(1530 * MICROSECOND + CNT);
        }

        /**
         * @brief       Move the cursor to a specified column and row
         *
         * @param[in]   row     Zero-indexed row to place the cursor
         * @param[in]   col     Zero indexed column to place the cursor
         */
        void move (const uint8_t row, const uint8_t col) {
            uint8_t ddramLine, addr = 0;

            // Handle weird special case where a single row LCD is split across
            // multiple DDRAM lines (i.e., 16x1 type 1)
            if (this->m_memMap.ddramCharRowBreak > this->m_memMap.ddramLineEnd) {
                ddramLine = col / this->m_memMap.ddramLineEnd;
                if (ddramLine)
                    addr  = 0x40;
                addr |= col % this->m_memMap.ddramLineEnd;
            } else if (4 == this->m_memMap.charRows) {
                // Determine DDRAM line
                if (row % 2)
                    addr = 0x40;
                if (row / 2)
                    addr += this->m_memMap.ddramCharRowBreak;
                addr += col % this->m_memMap.ddramCharRowBreak;

            } else /* implied: "if (2 == memMap.charRows)" */{
                if (row)
                    addr = 0x40;
                addr |= col;
            }

            this->cmd(addr | PropWare::HD44780::SET_DDRAM_ADDR);
            this->m_curPos.row = row;
            this->m_curPos.col = col;
        }

        void puts (const char string[]) {
            const char *s = (char *) string;

            while (*s) {
                this->put_char(*s);
                ++s;
            }
        }

        void put_char (const char c) {
            if (this->m_inEscapeSequence)
                this->handle_escape_sequence_character(c);
            else {
                switch (c) {
                    case ESCAPE:
                        this->start_escape_sequence();
                        break;
                    case NEWLINE:
                        this->newline();
                        break;
                    case TAB:
                        this->tab();
                        break;
                    case CARRIAGE_RETURN:
                        this->carriage_return();
                        break;
                    case BACKSPACE:
                        this->backspace();
                        break;
                    case BELL:
                        break;
                    default:
                        //set RS to data and RW to write
                        this->m_rs.set();
                        this->write((const uint8_t) c);
                        ++this->m_curPos.col;

                        // Insert a line wrap if necessary
                        if (this->m_memMap.charColumns == this->m_curPos.col)
                            this->put_char('\n');

                        // Handle weird special case where a single row LCD is split
                        // across multiple DDRAM lines (i.e., 16x1 type 1)
                        if (this->m_memMap.ddramCharRowBreak > this->m_memMap.ddramLineEnd)
                            this->move(this->m_curPos.row, this->m_curPos.col);
                }
            }
        }

        void newline () {
            this->m_curPos.row++;
            if (this->m_curPos.row == this->m_memMap.charRows)
                this->m_curPos.row = 0;
            this->m_curPos.col     = 0;
            this->move(this->m_curPos.row, this->m_curPos.col);
        }

        void carriage_return () {
            this->move(this->m_curPos.row, 0);
        }

        void backspace () {
            uint8_t nextRow = this->m_curPos.row;
            uint8_t nextColumn;
            if (this->m_curPos.col)
                nextColumn = (uint8_t) (this->m_curPos.col - 1);
            else {
                nextColumn = this->m_memMap.charColumns;

                if (this->m_curPos.row)
                    nextRow = (uint8_t) (this->m_curPos.row - 1);
                else
                    nextRow = this->m_memMap.charRows;
            }

            this->move(nextRow, nextColumn);
        }

        void tab () {
            do {
                this->put_char(' ');
            } while (this->m_curPos.col % PropWare::HD44780::TAB_WIDTH);
        }

        void show_cursor () {
            this->cmd(DISPLAY_CTRL | DISPLAY_PWR | BLINK);
        }

        void hide_cursor () {
            this->cmd(DISPLAY_CTRL | DISPLAY_PWR);
        }

        /**
         * @brief      Send a control command to the LCD module
         *
         * @param[in]  command  8-bit command to send to the LCD
         */
        void cmd (const uint8_t command) const {
            //set RS to command mode and RW to write
            this->m_rs.clear();
            this->write(command);
        }

        static void print_error_str (const Printer &printer, const HD44780::ErrorCode err) {
            static const char str[] = "HD44780 Error ";

            printer << str << err - PropWare::HD44780::BEG_ERROR << ": ";
            switch (err) {
                case PropWare::HD44780::INVALID_CTRL_SGNL:
                    printer << "invalid control signal\n";
                    break;
                case PropWare::HD44780::INVALID_DIMENSIONS:
                    printer << "invalid LCD dimension; please choose from the HD44780::Dimensions type\n";
                    break;
                default:
                    printer << "unknown error code\n";
                    break;
            }
        }

    protected:
        void start_escape_sequence () {
            this->m_inEscapeSequence = true;
            this->m_escapeSequence[0] = ESCAPE;
            this->m_escapeSequence[1] = NULL_TERMINATOR;
        }

        void handle_escape_sequence_character (const char c) {
            size_t sequenceLength = 0;
            while (this->m_escapeSequence[sequenceLength])
                ++sequenceLength;

            if ((ESCAPE_SEQUENCE_BUFFER_LENGTH - 1) > sequenceLength) {
                this->m_escapeSequence[sequenceLength]     = c;
                this->m_escapeSequence[sequenceLength + 1] = NULL_TERMINATOR;

                if (1 != sequenceLength && this->final_escape_sequence_character(c)) {
                    this->m_inEscapeSequence = false;
                    this->perform_escape_sequence_command(sequenceLength + 1);
                }
            } else
                this->m_inEscapeSequence = false;
        }

        void perform_escape_sequence_command (const size_t length) {
            const char command = this->m_escapeSequence[length - 1];

            switch (command) {
                case 'H':
                    this->move_via_command_sequence();
                    break;
                case 'h':
                    this->show_cursor();
                    break;
                case 'l':
                    this->hide_cursor();
                    break;
            }

            this->m_inEscapeSequence = false;
        }

        void move_via_command_sequence () {
            const uint8_t row = (uint8_t) (atoi(&this->m_escapeSequence[2]) - 1);

            // Find start of second number
            size_t index = 3;
            while (this->m_escapeSequence[index++] != ';');
            const uint8_t column = (uint8_t) (atoi(&this->m_escapeSequence[index]) - 1);

            this->move(row, column);
        }

        static bool final_escape_sequence_character (const char c) {
            return '@' <= c && c <= '~';
        }

        /**
         * @brief       Write a single byte to the LCD - instruction or data
         *
         * @param[in]   val     Value to be written
         */
        void write (const uint8_t val) const {
            // Clear RW to signal write value
            this->m_rw.clear();

            if (BusWidth::WIDTH4 == this->m_bitMode) {
                // shift out the high nibble
                this->m_dataPort.write(val >> 4);
                this->clock_pulse();

                // Shift out low nibble
                this->m_dataPort.write(val);
            }
                // Shift remaining four bits out
            else /* Implied: if (HD44780::8BIT == this->m_bitMode) */{
                this->m_dataPort.write(val);
            }
            this->clock_pulse();
        }

        /**
         * @brief   Toggle the enable pin, inducing a write to the LCD's register
         */
        void clock_pulse (void) const {
            this->m_en.set();
            waitcnt(MILLISECOND + CNT);
            this->m_en.clear();
        }

        /**
         * @brief   The memory map is used to determine where line wraps should and shouldn't occur
         */
        void generate_mem_map () {
            // TODO: Make this a look-up table instead of a switch-case
            switch (this->m_dimensions) {
                case Dimensions::DIM_8x1:
                    this->m_memMap.charRows          = 1;
                    this->m_memMap.charColumns       = 8;
                    this->m_memMap.ddramCharRowBreak = 8;
                    this->m_memMap.ddramLineEnd      = 8;
                    break;
                case Dimensions::DIM_8x2:
                    this->m_memMap.charRows          = 2;
                    this->m_memMap.charColumns       = 8;
                    this->m_memMap.ddramCharRowBreak = 8;
                    this->m_memMap.ddramLineEnd      = 8;
                    break;
                case Dimensions::DIM_8x4:
                    this->m_memMap.charRows          = 4;
                    this->m_memMap.charColumns       = 8;
                    this->m_memMap.ddramCharRowBreak = 8;
                    this->m_memMap.ddramLineEnd      = 16;
                    break;
                case Dimensions::DIM_16x1_1:
                    this->m_memMap.charRows          = 1;
                    this->m_memMap.charColumns       = 16;
                    this->m_memMap.ddramCharRowBreak = 8;
                    this->m_memMap.ddramLineEnd      = 8;
                    break;
                case Dimensions::DIM_16x1_2:
                    this->m_memMap.charRows          = 1;
                    this->m_memMap.charColumns       = 16;
                    this->m_memMap.ddramCharRowBreak = 16;
                    this->m_memMap.ddramLineEnd      = 16;
                    break;
                case Dimensions::DIM_16x2:
                    this->m_memMap.charRows          = 2;
                    this->m_memMap.charColumns       = 16;
                    this->m_memMap.ddramCharRowBreak = 16;
                    this->m_memMap.ddramLineEnd      = 16;
                    break;
                case Dimensions::DIM_16x4:
                    this->m_memMap.charRows          = 4;
                    this->m_memMap.charColumns       = 16;
                    this->m_memMap.ddramCharRowBreak = 16;
                    this->m_memMap.ddramLineEnd      = 32;
                    break;
                case Dimensions::DIM_20x1:
                    this->m_memMap.charRows          = 1;
                    this->m_memMap.charColumns       = 20;
                    this->m_memMap.ddramCharRowBreak = 20;
                    this->m_memMap.ddramLineEnd      = 20;
                    break;
                case Dimensions::DIM_20x2:
                    this->m_memMap.charRows          = 2;
                    this->m_memMap.charColumns       = 20;
                    this->m_memMap.ddramCharRowBreak = 20;
                    this->m_memMap.ddramLineEnd      = 20;
                    break;
                case Dimensions::DIM_20x4:
                    this->m_memMap.charRows          = 4;
                    this->m_memMap.charColumns       = 20;
                    this->m_memMap.ddramCharRowBreak = 20;
                    this->m_memMap.ddramLineEnd      = 40;
                    break;
                case Dimensions::DIM_24x1:
                    this->m_memMap.charRows          = 1;
                    this->m_memMap.charColumns       = 24;
                    this->m_memMap.ddramCharRowBreak = 24;
                    this->m_memMap.ddramLineEnd      = 24;
                    break;
                case Dimensions::DIM_24x2:
                    this->m_memMap.charRows          = 2;
                    this->m_memMap.charColumns       = 24;
                    this->m_memMap.ddramCharRowBreak = 24;
                    this->m_memMap.ddramLineEnd      = 24;
                    break;
                case Dimensions::DIM_40x1:
                    this->m_memMap.charRows          = 1;
                    this->m_memMap.charColumns       = 40;
                    this->m_memMap.ddramCharRowBreak = 40;
                    this->m_memMap.ddramLineEnd      = 40;
                    break;
                case Dimensions::DIM_40x2:
                    this->m_memMap.charRows          = 2;
                    this->m_memMap.charColumns       = 40;
                    this->m_memMap.ddramCharRowBreak = 40;
                    this->m_memMap.ddramLineEnd      = 40;
                    break;
            }
        }

    private:
        typedef struct {
            uint8_t row;
            uint8_t col;
        }                    Position;

    protected:
        HD44780::MemMap m_memMap;

    private:
        const SimplePort m_dataPort;
        const Pin        m_rs;
        const Pin        m_rw;
        const Pin        m_en;
        const BusWidth   m_bitMode;
        const Dimensions m_dimensions;
        Position         m_curPos;
        bool             m_inEscapeSequence;
        char             m_escapeSequence[ESCAPE_SEQUENCE_BUFFER_LENGTH];
        bool             m_showCursor;
};

}
