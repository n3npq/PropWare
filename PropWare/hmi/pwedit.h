/**
 * @file    pwedit.h
 *
 * @author  David Zemon
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

#include <PropWare/hmi/output/printer.h>
#include <PropWare/hmi/input/scanner.h>
#include <PropWare/string/stringbuilder.h>
#include <PropWare/c++allocate.h>
#include <PropWare/filesystem/filereader.h>
#include <PropWare/filesystem/filewriter.h>
#include <list>

namespace PropWare {

/**
 * @brief   Basic terminal-style text editor
 *
 * Capable of running on any `Printer` which supports the following escape sequences and ASCII characters:
 *
 *   - 0x07: Bell (Can be no-op)
 *   - 0x08: Backspace
 *   - `\n`: Newline
 *   - `CSI n ; m H`: Move the cursor to column `n` and row `m`, where `n` and `m` are 1-indexed.
 */
class PWEdit {
    public:
        typedef enum {
                                  NO_ERROR,
            /** First error */    BEG_ERROR = 128
        } ErrorCode;

        typedef enum {
            UP,
            DOWN,
            LEFT,
            RIGHT
        } Direction;

    public:
        static const char CURSOR        = '#';
        static const char SAVE_CHAR     = 'w';
        static const char EXIT_CHAR     = 'q';
        static const char EXIT_NO_SAVE  = '!';
        static const char COMMAND_START = ':';
        static const char TO_LINE_START = '0';
        static const char TO_LINE_END   = '$';
        static const char TO_FILE_START = 'g';
        static const char TO_FILE_END   = 'G';

        static const unsigned int PADDING = 3;

    public:
        /**
         * @brief   Constructor
         *
         * @param[in]   inFile      Unopened file to be displayed/edited
         * @param[in]   outFile     Unopened file used for saving any updated content
         * @param[in]   scanner     Human input will be read from this scanner. `pwIn` can not be used because it is set
         *                          for echo mode on, which can not be used in an editor
         * @param[in]   printer     Where the contents of the editor should be printed
         * @param[in]   *debugger   Generally unused, but (sparse) debugging output can be displayed on this Printer if
         *                          provided
         */
        PWEdit (FileReader &inFile, FileWriter &outFile, Scanner &scanner, const Printer &printer = pwOut,
                Printer *debugger = NULL)
                : m_inFile(&inFile),
                  m_outFile(&outFile),
                  m_printer(&printer),
                  m_scanner(&scanner),
                  m_debugger(debugger),
                  m_columns(0),
                  m_rows(1),
                  m_modified(false) {
        }

        ~PWEdit () {
            for (auto line : this->m_lines)
                delete line;

            if (this->m_inFile->open())
                this->m_inFile->close();
            if (this->m_outFile->open())
                this->m_outFile->close();
        }

        PropWare::ErrorCode run () {
            PropWare::ErrorCode err;

            this->calibrate();
            check_errors(this->read_in_file());
            check_errors(this->m_inFile->close());

            this->m_firstLineDisplayed   = (unsigned int) -1;
            this->m_firstColumnDisplayed = (unsigned int) -1;
            this->to_file_start();

            bool exit = false;
            char c;
            do {
                c = this->m_scanner->get_char();
                switch (c) {
                    case 'a':
                    case 'h':
                        this->move_selection(LEFT);
                        break;
                    case 's':
                    case 'j':
                        this->move_selection(DOWN);
                        break;
                    case 'd':
                    case 'l':
                        this->move_selection(RIGHT);
                        break;
                    case 'w':
                    case 'k':
                        this->move_selection(UP);
                        break;
                    case TO_FILE_START:
                        this->to_file_start();
                        break;
                    case TO_FILE_END:
                        this->to_file_end();
                        break;
                    case TO_LINE_START:
                        this->to_line_start();
                        break;
                    case TO_LINE_END:
                        this->to_line_end();
                        break;
                    case COMMAND_START:
                        check_errors(this->command(exit));
                        break;
                }
            } while (!exit);

            this->clear(true);
            return NO_ERROR;
        }

    protected:
        void calibrate () {
            this->hide_cursor();
            static const char calibrationString[]     = "Calibration...#";
            const size_t      calibrationStringLength = strlen(calibrationString);

            this->clear(false);
            *this->m_printer << calibrationString;
            this->m_columns = calibrationStringLength;
            this->m_rows    = 1;

            char input;
            do {
                input = this->m_scanner->get_char();
                switch (input) {
                    case 'a':
                    case 'h':
                        // Move left
                        if (1 < this->m_columns) {
                            --this->m_columns;
                            *this->m_printer << BACKSPACE << ' ' << BACKSPACE << BACKSPACE << CURSOR;
                        }
                        break;
                    case 'w':
                    case 'k':
                        // Move up
                        if (this->m_debugger) {
                            *this->m_debugger << "Moving up\n";
                            *this->m_debugger << "Cur. Rows: " << this->m_rows << '\n';
                        }

                        if (1 < this->m_rows) {
                            --this->m_rows;
                            *this->m_printer << BACKSPACE << ' ';
                            this->clear(false);
                            *this->m_printer << calibrationString;
                            *this->m_printer << BACKSPACE << ' ';

                            // Handle columns
                            if (calibrationStringLength >= this->m_columns) {
                                const unsigned int charactersToDelete = calibrationStringLength - this->m_columns + 1;

                                for (unsigned int i = 0; i < charactersToDelete; ++i)
                                    *this->m_printer << BACKSPACE << ' ' << BACKSPACE;
                                *this->m_printer << CURSOR;
                            } else {
                                for (unsigned int i = calibrationStringLength; i < this->m_columns; ++i)
                                    *this->m_printer << ' ';
                                *this->m_printer << CURSOR;
                            }

                            // Handle rows
                            for (unsigned int i = 1; i < this->m_rows; ++i) {
                                *this->m_printer << BACKSPACE << " \n";
                                for (unsigned int j = 0; j < (this->m_columns - 1); ++j)
                                    *this->m_printer << ' ';
                                *this->m_printer << CURSOR;
                            }
                        }
                        break;
                    case 's':
                    case 'j':
                        // Move down
                        ++this->m_rows;
                        *this->m_printer << BACKSPACE << " \n";
                        for (unsigned int i = 0; i < (this->m_columns - 1); ++i)
                            *this->m_printer << ' ';
                        *this->m_printer << CURSOR;
                        break;
                    case 'd':
                    case 'l':
                        // Move right
                        ++this->m_columns;
                        *this->m_printer << BACKSPACE << " #";
                        break;
                }
            } while (not_enter_key(input));

            this->show_cursor();
            this->clear();
            *this->m_printer << this->m_columns << 'x' << this->m_rows << " ";
        }

        PropWare::ErrorCode read_in_file () {
            PropWare::ErrorCode err;

            check_errors(this->m_inFile->open());
            while (!this->m_inFile->eof()) {
                // Read a line
                StringBuilder *line = new StringBuilder();
                char          c;
                do {
                    check_errors(this->m_inFile->safe_get_char(c));
                    if (32 <= c && c <= 127)
                        // Only accept printable characters
                        line->put_char(c);
                } while ('\r' != c && '\n' != c);

                // Munch the \n following a \r
                if ('\n' == this->m_inFile->peek()) {
                    check_errors(this->m_inFile->safe_get_char(c));
                }

                this->m_lines.push_back(line);

                this->move_cursor(2, 1);
                *this->m_printer << "Line: " << this->m_lines.size();
            }
            return NO_ERROR;
        }

        void display_file_from (const unsigned int startingLineNumber, const unsigned int startingColumnNumber) {
            auto lineIterator = this->m_lines.cbegin();

            unsigned int i = startingLineNumber;
            while (i--)
                ++lineIterator;

            for (unsigned int row = 1; row <= this->m_rows; ++row) {
                this->print_line_at_row(startingColumnNumber, lineIterator, row);
                ++lineIterator;
            }

            this->m_firstLineDisplayed = startingLineNumber;
        }

        void print_line_at_row (const unsigned int startingColumnNumber,
                                const std::list<PropWare::StringBuilder *>::const_iterator &lineIterator,
                                unsigned int row) const {
            const uint16_t charactersInLine = (*lineIterator)->get_size();

            move_cursor(row, 1);
            unsigned int column;
            for (column = 0; column < this->m_columns && (column + startingColumnNumber) < charactersInLine; ++column)
                *this->m_printer << (*lineIterator)->to_string()[column + startingColumnNumber];

            while (column++ < this->m_columns)
                *this->m_printer << ' ';
        }

        void clear (const bool writeSpaces = true) const {
            if (writeSpaces)
                for (unsigned int row = 1; row <= this->m_rows; ++row)
                    clear_row(row);
            this->move_cursor(1, 1);
        }

        void clear_row (const unsigned int row) const {
            move_cursor(row, 1);
            for (unsigned int col = 0; col <= m_columns; ++col)
                *m_printer << ' ';
        }

        void move_cursor (const unsigned int row, const unsigned int column) const {
            *this->m_printer << ESCAPE << '[' << row << ';' << column << 'H';
        }

        void hide_cursor () {
            *this->m_printer << ESCAPE << '[' << "?25l";
        }

        void show_cursor () {
            *this->m_printer << ESCAPE << '[' << "?25h";
        }

        void move_selection (const Direction direction) {
            switch (direction) {
                case DOWN:
                    this->move_down();
                    break;
                case UP:
                    this->move_up();
                    break;
                case RIGHT:
                    this->move_right();
                    break;
                case LEFT:
                    this->move_left();
                    break;
            }
        }

        void move_down () {
            if ((this->m_lines.size() - 1) <= this->m_selectedLineNumber) {
                if (((*this->m_selectedLine)->get_size() - 1) == this->m_selectedColumnInLine)
                    *this->m_printer << BELL;
                else
                    this->to_file_end();
            } else {
                const unsigned int startingColumnSelection = this->m_selectedColumnInLine;
                bool               redrawNecessary         = this->trim_column_selection_to_fit(DOWN);
                redrawNecessary |= this->expand_column_selection_to_desired(DOWN, startingColumnSelection);

                const unsigned int lastLineDisplayed = this->m_firstLineDisplayed + this->m_rows;
                if (PADDING > (this->m_rows - this->m_termRow)
                        && this->m_lines.size() > lastLineDisplayed) {
                    this->display_file_from(++this->m_firstLineDisplayed, this->m_firstColumnDisplayed);
                    this->move_cursor(this->m_termRow, this->m_termColumn);
                } else {
                    if (redrawNecessary)
                        this->display_file_from(this->m_firstLineDisplayed, this->m_firstColumnDisplayed);
                    this->move_cursor(++this->m_termRow, this->m_termColumn);
                }
                ++this->m_selectedLineNumber;
                ++this->m_selectedLine;
            }
        }

        void move_up () {
            if (!this->m_selectedLineNumber) {
                if (this->m_selectedColumnInLine)
                    this->to_file_start();
                else
                    *this->m_printer << BELL;
            } else {
                const unsigned int startingColumnSelection = this->m_selectedColumnInLine;
                bool               redrawNecessary         = this->trim_column_selection_to_fit(UP);
                redrawNecessary |= this->expand_column_selection_to_desired(UP, startingColumnSelection);

                if (PADDING >= this->m_termRow && this->m_firstLineDisplayed) {
                    this->display_file_from(--this->m_firstLineDisplayed, this->m_firstColumnDisplayed);
                    this->move_cursor(this->m_termRow, this->m_termColumn);
                } else {
                    if (redrawNecessary)
                        this->display_file_from(this->m_firstLineDisplayed, this->m_firstColumnDisplayed);
                    this->move_cursor(--this->m_termRow, this->m_termColumn);
                }
                --this->m_selectedLineNumber;
                --this->m_selectedLine;
            }
        }

        void move_right () {
            if (this->cursor_at_end()) {
                *this->m_printer << BELL;
            } else {
                const unsigned int lastVisibleColumnOfLine = this->m_firstColumnDisplayed + this->m_columns;
                if (PADDING > (this->m_columns - this->m_termColumn)
                        && (*this->m_selectedLine)->get_size() > lastVisibleColumnOfLine) {
                    this->display_file_from(this->m_firstLineDisplayed, ++this->m_firstColumnDisplayed);
                    this->move_cursor(this->m_termRow, this->m_termColumn);
                } else
                    this->move_cursor(this->m_termRow, ++this->m_termColumn);
                ++this->m_selectedColumnInLine;
            }
            this->m_desiredColumnInLine = this->m_selectedColumnInLine;
        }

        void move_left () {
            if (!this->m_selectedColumnInLine)
                *this->m_printer << BELL;
            else {
                if (PADDING >= this->m_termColumn && this->m_firstColumnDisplayed) {
                    this->display_file_from(this->m_firstLineDisplayed, --this->m_firstColumnDisplayed);
                    this->move_cursor(this->m_termRow, this->m_termColumn);
                } else
                    this->move_cursor(this->m_termRow, --this->m_termColumn);
                --this->m_selectedColumnInLine;
            }
            this->m_desiredColumnInLine = this->m_selectedColumnInLine;
        }

        bool trim_column_selection_to_fit (const Direction direction) {
            auto tempIterator = m_selectedLine;
            switch (direction) {
                case UP:
                    --tempIterator;
                    break;
                case DOWN:
                    ++tempIterator;
                    break;
                default:
                    return false;
            }

            bool               redrawNecessary = false;
            const unsigned int lineLength      = (*tempIterator)->get_size();

            const bool lineShorterThanSelection = lineLength <= this->m_selectedColumnInLine;
            if (lineShorterThanSelection) {
                this->m_selectedColumnInLine = lineLength - 1;
                if (this->m_firstColumnDisplayed > this->m_selectedColumnInLine) {
                    this->m_firstColumnDisplayed = this->m_selectedColumnInLine;
                    this->m_termColumn           = 1;
                    redrawNecessary = true;
                } else
                    this->m_termColumn = this->m_selectedColumnInLine - this->m_firstColumnDisplayed + 1;
            }

            return redrawNecessary;
        }

        bool expand_column_selection_to_desired (const Direction direction, const unsigned int previousColumnSelected) {
            auto tempIterator = m_selectedLine;
            switch (direction) {
                case UP:
                    --tempIterator;
                    break;
                case DOWN:
                    ++tempIterator;
                    break;
                default:
                    return false;
            }

            bool               redrawNecessary     = false;
            const unsigned int lineLength          = (*tempIterator)->get_size();
            const bool         expansionIsPossible = (lineLength - 1) > previousColumnSelected;
            if (expansionIsPossible) {
                if (this->m_debugger) {
                    this->m_debugger->printf("Len %3u\n", lineLength);
                    this->m_debugger->printf("Old %3u\n", previousColumnSelected);
                    this->m_debugger->printf("New %3u\n", this->m_selectedColumnInLine);
                    this->m_debugger->printf("Des %3u\n", this->m_desiredColumnInLine);
                }

                const bool expansionIsDesired = this->m_desiredColumnInLine != previousColumnSelected;
                if (expansionIsDesired) {
                    if (lineLength >= this->m_desiredColumnInLine)
                        this->m_selectedColumnInLine = this->m_desiredColumnInLine;
                    else
                        this->m_selectedColumnInLine = lineLength - 1;

                    const unsigned int lastColumnDisplayed = this->m_firstColumnDisplayed + this->m_columns;
                    if (lastColumnDisplayed < this->m_selectedColumnInLine) {
                        this->m_firstColumnDisplayed = this->m_selectedColumnInLine - this->m_columns;
                        this->m_termColumn           = this->m_columns;
                        redrawNecessary = true;
                    } else
                        this->m_termColumn = this->m_selectedColumnInLine - this->m_firstColumnDisplayed + 1;
                }
            }
            return redrawNecessary;
        }

        void to_file_start () {
            const bool redrawNecessary = this->m_firstLineDisplayed || this->m_firstColumnDisplayed;
            this->m_firstLineDisplayed   = 0;
            this->m_firstColumnDisplayed = 0;
            this->m_selectedLineNumber   = 0;
            this->m_selectedLine         = this->m_lines.cbegin();
            this->m_desiredColumnInLine  = this->m_selectedColumnInLine = 0;
            this->m_termRow              = 1;
            this->m_termColumn           = 1;

            if (redrawNecessary)
                this->display_file_from(0, 0);
            this->move_cursor(this->m_termRow, this->m_termColumn);
        }

        void to_file_end () {
            const bool lastLineNotShown   = this->m_firstLineDisplayed != (this->m_lines.size() - this->m_rows);
            const bool lastColumnNotShown = this->m_firstColumnDisplayed > (this->m_lines.back()->get_size() - 1);

            if (this->m_debugger) {
                this->m_debugger->printf("Last ln: %s\n", Utility::to_string(lastLineNotShown));
                this->m_debugger->printf("Last cl: %s\n", Utility::to_string(lastColumnNotShown));
            }

            const bool redrawNecessary = lastLineNotShown || lastColumnNotShown;

            this->m_firstLineDisplayed = this->m_lines.size() - this->m_rows;
            unsigned int lastColumn;
            if (this->m_lines.back()->get_size() > this->m_columns) {
                this->m_firstColumnDisplayed = this->m_lines.back()->get_size() - this->m_columns;
                lastColumn = this->m_columns - 1;
            } else {
                this->m_firstColumnDisplayed = 0;
                lastColumn = this->m_lines.back()->get_size();
            }
            this->m_selectedLine = this->m_lines.cend();
            --this->m_selectedLine; // Need to point to the last line, not the "end" which is one after the last
            this->m_selectedLineNumber   = this->m_lines.size() - 1;
            this->m_selectedColumnInLine = (unsigned int) ((*this->m_selectedLine)->get_size() - 1);
            this->m_desiredColumnInLine  = this->m_selectedColumnInLine;
            this->m_termRow              = this->m_rows;
            this->m_termColumn           = lastColumn;

            if (redrawNecessary)
                this->display_file_from(this->m_firstLineDisplayed, this->m_firstColumnDisplayed);
            this->move_cursor(this->m_termRow, this->m_termColumn);
        }

        void to_line_start () {
            if (this->m_selectedColumnInLine) {
                if (this->m_firstColumnDisplayed) {
                    this->m_firstColumnDisplayed = 0;
                    this->display_file_from(this->m_firstLineDisplayed, this->m_firstColumnDisplayed);
                }
                this->m_termColumn = 1;
                this->move_cursor(this->m_termRow, this->m_termColumn);
                this->m_selectedColumnInLine = 0;
            }
            this->m_desiredColumnInLine = this->m_selectedColumnInLine;
        }

        void to_line_end () {
            if (!this->cursor_at_end()) {
                const unsigned int lastColumnOfLineVisibleOnScreen = this->m_firstColumnDisplayed + this->m_columns;
                const uint16_t     lineLength                      = (*this->m_selectedLine)->get_size();
                const bool         lineScrollsPastVisibleColumns   = lineLength > lastColumnOfLineVisibleOnScreen;

                if (lineScrollsPastVisibleColumns) {
                    this->m_firstColumnDisplayed = lineLength - this->m_columns;
                    this->m_termColumn           = this->m_columns;

                    this->display_file_from(this->m_firstLineDisplayed, this->m_firstColumnDisplayed);
                } else
                    this->m_termColumn = lineLength - this->m_firstColumnDisplayed;
                this->move_cursor(this->m_termRow, this->m_termColumn);
                this->m_selectedColumnInLine = (unsigned int) (lineLength - 1);
            }
            this->m_desiredColumnInLine = this->m_selectedColumnInLine;
        }

        bool cursor_at_end () const {
            return ((*this->m_selectedLine)->get_size() - 1) <= this->m_selectedColumnInLine;
        }

        PropWare::ErrorCode command (bool &exit) {
            PropWare::ErrorCode err;

            this->clear_row(this->m_rows);
            this->move_cursor(this->m_rows, 1);
            *this->m_printer << COMMAND_START;

            char _buffer[64];
            this->read_command_input(_buffer);

            // `_buffer` can't be modified - no good for parsing char-by-char
            char *nextCharacter = _buffer;
            if (NULL != this->m_debugger)
                *this->m_debugger << "CMD: " << nextCharacter << '\n';

            if (SAVE_CHAR == nextCharacter[0]) {
                if (NULL != this->m_debugger)
                    *this->m_debugger << "Save cmd...\n";
                check_errors(this->save());
                ++nextCharacter;
            }

            if (EXIT_CHAR == nextCharacter[0]) {
                if (NULL != this->m_debugger)
                    *this->m_debugger << "Exit cmd...\n";

                if (this->m_modified) {
                    if (EXIT_NO_SAVE == nextCharacter[1]) {
                        if (NULL != this->m_debugger)
                            *this->m_debugger << "Exit (discard)\n";
                        exit = true;
                    } else {
                        if (NULL != this->m_debugger)
                            *this->m_debugger << "BAD EXIT\n";

                        this->clear_row(this->m_rows);
                        this->move_cursor(this->m_rows, 1);
                        *this->m_printer << "UNSAVED CHANGES";
                        char c;
                        do {
                            c = this->m_scanner->get_char();
                        } while (not_enter_key(c));
                    }
                } else {
                    if (NULL != this->m_debugger)
                        *this->m_debugger << "Exit (no-mod)\n";
                    exit = true;
                }
            }

            this->rewrite_last_line();
            this->move_cursor(this->m_termRow, this->m_termColumn);
            return NO_ERROR;
        }

        void read_command_input (char _buffer[]) const {
            char *string = _buffer - 1;
            do {
                ++string;
                string[0] = this->m_scanner->get_char();
                if (not_enter_key(string[0]))
                    *this->m_printer << string[0];
            } while (not_enter_key(string[0]));
        }

        void rewrite_last_line () const {
            auto               iterator                = this->m_selectedLine;
            const unsigned int lastLineNumberDisplayed = this->m_firstLineDisplayed + this->m_rows - 1;
            for (unsigned int  i                       = this->m_selectedLineNumber; i < lastLineNumberDisplayed; ++i)
                ++iterator;
            this->print_line_at_row(this->m_firstColumnDisplayed, iterator, this->m_rows);
        }

    protected:
        // Write-only functions

        /**
         * @brief   Save the file if it has changed
         *
         * @return  Zero upon success, error code otherwise
         */
        PropWare::ErrorCode save () {
            static const char trimmingMessage[] = "Trimming...";
            static const char savingMessage[]   = "Saving...  ";

            PropWare::ErrorCode err;
            if (this->m_modified) {
                if (NULL != this->m_debugger)
                    *this->m_debugger << "Saving now\n";

                check_errors(this->m_outFile->open(0, File::BEG));

                this->move_cursor(this->m_rows, 1);
                *this->m_printer << trimmingMessage;
                check_errors(this->m_outFile->trim());

                this->move_cursor(this->m_rows, 1);
                *this->m_printer << savingMessage;
                unsigned int lineNumber = 1;
                for (auto    line : this->m_lines) {
                    this->move_cursor(this->m_rows, sizeof(savingMessage + 1));
                    *this->m_printer << lineNumber++;
                    check_errors(this->m_outFile->safe_puts(line->to_string()));
                }
                check_errors(this->m_outFile->close());
                this->m_modified = false;
            } else {
                if (NULL != this->m_debugger)
                    *this->m_debugger << "No mod. No Save\n";
            }
            return NO_ERROR;
        }

    protected:
        static bool not_enter_key (const char c) {
            return '\r' != c && '\n' != c && '\0' != c;
        }

    protected:
        FileReader                 *m_inFile;
        FileWriter                 *m_outFile;
        const Printer              *m_printer;
        Scanner                    *m_scanner;
        Printer                    *m_debugger;
        std::list<StringBuilder *> m_lines;

        /** Total columns on screen */
        unsigned int m_columns;
        /** Total rows on screen */
        unsigned int m_rows;

        /** Current cursor row (1-indexed) */
        unsigned int m_termRow;
        /** Current cursor column (1-indexed) */
        unsigned int m_termColumn;

        std::list<StringBuilder *>::const_iterator m_selectedLine;

        /** Index of currently selected line in the file (0-indexed) */
        unsigned int m_selectedLineNumber;
        /** Index of currently selected column in the line (0-indexed) */
        unsigned int m_selectedColumnInLine;
        /**
         * Index of the column that the user actually wants. When moving from a long to a short line, this column
         * may not exist. It is those cases that `m_selectedColumnInLine` may differ from this. The desired column
         * will remain the larger value, that way `m_selectedColumnInLine` can be restored the next time a longer
         * line is selected
         */
        unsigned int m_desiredColumnInLine;

        /** First visible line of the file (0-indexed) */
        unsigned int m_firstLineDisplayed;
        /** First visible column of the line displayed (0-indexed) */
        unsigned int m_firstColumnDisplayed;

        /** Has the file content been modified */
        bool m_modified;
};

}
