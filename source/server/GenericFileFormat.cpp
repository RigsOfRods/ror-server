/*
This file is part of "Rigs of Rods Server" (Relay mode)

Copyright 2007   Pierre-Michel Ricordel
Copyright 2014+  Rigs of Rods Community

"Rigs of Rods Server" is free software: you can redistribute it
and/or modify it under the terms of the GNU General Public License
as published by the Free Software Foundation, either version 3
of the License, or (at your option) any later version.

"Rigs of Rods Server" is distributed in the hope that it will
be useful, but WITHOUT ANY WARRANTY; without even the implied
warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar. If not, see <http://www.gnu.org/licenses/>.
*/

// Ported from Rigs of Rods (/source/main/utils/) at revision a95d75df8c7c376e36ff9e24df62e7006d6246df by ohlidalp, 2026

#include "GenericFileFormat.h"
#include "logger.h"

#include <algorithm>
#include <cstdio>
#include <locale.h>
#include <iterator>

// < snatched from OgreStringConverter.cpp >
// A quick define to overcome different names for the same function
// A quick define to overcome different names for the same function
#if defined(_WIN32)
#   define strtod_l _strtod_l
#   define strtoul_l _strtoul_l
#   define strtol_l _strtol_l
#   define strtoull_l _strtoull_l
#   define strtoll_l _strtoll_l
#else
#   define strtod_l(ptr, end, l) strtod(ptr, end)
#   define strtoul_l(ptr, end, base, l) strtoul(ptr, end, base)
#   define strtol_l(ptr, end, base, l) strtol(ptr, end, base)
#   define strtoull_l(ptr, end, base, l) strtoull(ptr, end, base)
#   define strtoll_l(ptr, end, base, l) strtoll(ptr, end, base)
#endif

#if (defined(_WIN32)) && !defined(__MINGW32__)
#   define LC_NUMERIC_MASK LC_NUMERIC
#   define newlocale(cat, loc, base) _create_locale(cat, loc)
#else
#   define newlocale(cat, loc, base) 0
#endif

#ifdef __MINGW32__
#define _strtoull_l _strtoul_l
#define _strtoll_l _strtol_l
#endif

// < snatched from OgreStringConverter.h >
#ifdef _WIN32
#   define locale_t _locale_t
#else
#   define locale_t int
#endif

locale_t _numLocale = newlocale(LC_NUMERIC_MASK, "C", NULL);

enum class PartialToken
{
    NONE,
    COMMENT_SEMICOLON,             // Comment starting with ';'
    COMMENT_SLASH,                 // Comment starting with '//'
    COMMENT_HASH,
    STRING_QUOTED,                 // String starting/ending with '"'
    STRING_NAKED,                  // String without '"' on either end
    STRING_NAKED_CAPTURING_SPACES, // Only for OPTION_PARENTHESES_CAPTURE_SPACES - A naked string seeking the closing ')'.
    TITLE_STRING,                  // A whole-line string, with spaces
    NUMBER_STUB_MINUS,             // Sole '-' character, may start a number or a naked string.
    NUMBER_INTEGER,                // Just digits and optionally leading '-'
    NUMBER_DECIMAL,                // Like INTEGER but already containing '.'
    NUMBER_SCIENTIFIC_STUB,        // Like DECIMAL, already containing 'e' or 'E' but not the exponent value.
    NUMBER_SCIENTIFIC_STUB_MINUS,  // Like SCIENTIFIC_STUB but with only '-' in exponent. 
    NUMBER_SCIENTIFIC,             // Valid decimal number in scientific notation.
    KEYWORD,                       // Unqoted string at the start of line. Accepted characters: alphanumeric and underscore
    KEYWORD_BRACED,                // Like KEYWORD but starting with '[' and ending with ']'
    BOOL_TRUE,                     // Partial 'true'
    BOOL_FALSE,                    // Partial 'false'
    GARBAGE,                       // Text not fitting any above category, will be discarded
};

struct DocumentParser
{
    DocumentParser(GenericDocument& d, const BitMask_t opt, FILE* f)
        : doc(d), options(opt), file(f) {}

    // Config
    GenericDocument& doc;
    const BitMask_t options;
    FILE* file;

    // State
    std::vector<char> tok;
    size_t line_num = 0;
    size_t line_pos = 0;
    PartialToken partial_tok_type = PartialToken::NONE;
    bool title_found = false; // Only for OPTION_FIRST_LINE_IS_TITLE

    void ProcessChar(const char c);
    void ProcessEOF();
    void ProcessSeparatorWithinBool();

    void BeginToken(const char c);
    void UpdateComment(const char c);
    void UpdateString(const char c);
    void UpdateNumber(const char c);
    void UpdateBool(const char c);
    void UpdateKeyword(const char c);
    void UpdateTitle(const char c); // Only for OPTION_FIRST_LINE_IS_TITLE
    void UpdateGarbage(const char c);

    void DiscontinueBool();
    void DiscontinueNumber();
    void DiscontinueKeyword();
    void FlushStringishToken(GDocTokenType type);
    void FlushNumericToken();
};

void DocumentParser::BeginToken(const char c)
{
    switch (c)
    {
    case '\r':
        break;

    case ' ':
    case ',':
    case '\t':
        line_pos++;
        break;

    case ':':
        if (options & GenericDocument::OPTION_ALLOW_SEPARATOR_COLON)
        {
            line_pos++;
        }
        else
        {
            if (options & GenericDocument::OPTION_ALLOW_NAKED_STRINGS)
                partial_tok_type = PartialToken::STRING_NAKED;
            else
                partial_tok_type = PartialToken::GARBAGE;
            tok.push_back(c);
            line_pos++;
        }
        break;

    case '=':
        if (options & GenericDocument::OPTION_ALLOW_SEPARATOR_EQUALS)
        {
            line_pos++;
        }
        else
        {
            if (options & GenericDocument::OPTION_ALLOW_NAKED_STRINGS)
                partial_tok_type = PartialToken::STRING_NAKED;
            else
                partial_tok_type = PartialToken::GARBAGE;
            tok.push_back(c);
            line_pos++;
        }
        break;

    case '\n':
        doc.tokens.push_back({ GDocTokenType::LINEBREAK, 0.f });
        line_num++;
        line_pos = 0;
        break;

    case ';':
        partial_tok_type = PartialToken::COMMENT_SEMICOLON;
        line_pos++;
        break;

    case '/':
        if (options & GenericDocument::OPTION_ALLOW_SLASH_COMMENTS)
        {
            partial_tok_type = PartialToken::COMMENT_SLASH;
        }
        else
        {
            if (options & GenericDocument::OPTION_ALLOW_NAKED_STRINGS)
                partial_tok_type = PartialToken::STRING_NAKED;
            else
                partial_tok_type = PartialToken::GARBAGE;
            tok.push_back(c);
        }
        line_pos++;
        break;

    case '#':
        if (options & GenericDocument::OPTION_ALLOW_HASH_COMMENTS)
        {
            partial_tok_type = PartialToken::COMMENT_HASH;
        }
        else
        {
            if (options & GenericDocument::OPTION_ALLOW_NAKED_STRINGS)
                partial_tok_type = PartialToken::STRING_NAKED;
            else
                partial_tok_type = PartialToken::GARBAGE;
            tok.push_back(c);
        }
        line_pos++;
        break;

    case '[':
        if (options & GenericDocument::OPTION_ALLOW_BRACED_KEYWORDS)
        {
            partial_tok_type = PartialToken::KEYWORD_BRACED;
        }
        else
        {
            if (options & GenericDocument::OPTION_ALLOW_NAKED_STRINGS)
                partial_tok_type = PartialToken::STRING_NAKED;
            else
                partial_tok_type = PartialToken::GARBAGE;
        }
        tok.push_back(c);
        line_pos++;
        break;

    case '"':
        partial_tok_type = PartialToken::STRING_QUOTED;
        line_pos++;
        break;

    case '.':
        tok.push_back(c);
        partial_tok_type = PartialToken::NUMBER_DECIMAL;
        line_pos++;
        break;

    case 't':
        tok.push_back(c);
        partial_tok_type = PartialToken::BOOL_TRUE;
        line_pos++;
        break;

    case 'f':
        tok.push_back(c);
        partial_tok_type = PartialToken::BOOL_FALSE;
        line_pos++;
        break;

    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        partial_tok_type = PartialToken::NUMBER_INTEGER;
        tok.push_back(c);
        line_pos++;
        break;

    case '-':
        partial_tok_type = PartialToken::NUMBER_STUB_MINUS;
        tok.push_back(c);
        line_pos++;
        break;

    default:
        if (isalpha(c) &&
            (doc.tokens.size() == 0 || doc.tokens.back().type == GDocTokenType::LINEBREAK)) // on line start?
        {
            tok.push_back(c);
            partial_tok_type = PartialToken::KEYWORD;
        }
        else if (options & GenericDocument::OPTION_ALLOW_NAKED_STRINGS)
        {
            tok.push_back(c);
            partial_tok_type = PartialToken::STRING_NAKED;
        }
        else
        {
            partial_tok_type = PartialToken::GARBAGE;
            tok.push_back(c);
        }
        line_pos++;
        break;
    }

    if (options & GenericDocument::OPTION_FIRST_LINE_IS_TITLE
        && !title_found
        && (doc.tokens.size() == 0 || doc.tokens.back().type == GDocTokenType::LINEBREAK)
        && partial_tok_type != PartialToken::NONE
        && partial_tok_type != PartialToken::COMMENT_SEMICOLON
        && partial_tok_type != PartialToken::COMMENT_SLASH)
    {
        title_found = true;
        partial_tok_type = PartialToken::TITLE_STRING;
    }

    if (partial_tok_type == PartialToken::GARBAGE)
    {
        Logger::Log(LOG_WARN, "GenericFileFormat: line %zd, pos %zd: stray character '%c'", line_num, line_pos, c);
    }
}

void DocumentParser::UpdateComment(const char c)
{
    switch (c)
    {
    case '\r':
        break;

    case '\n':
        this->FlushStringishToken(GDocTokenType::COMMENT);
        // Break line
        doc.tokens.push_back({ GDocTokenType::LINEBREAK, 0.f });
        line_num++;
        line_pos = 0;
        break;

    case '/':
        if (partial_tok_type != PartialToken::COMMENT_SLASH || tok.size() > 0) // With COMMENT_SLASH, skip any number of leading '/'
        {
            tok.push_back(c);
        }
        line_pos++;
        break;

    default:
        tok.push_back(c);
        line_pos++;
        break;
    }
}

void DocumentParser::UpdateString(const char c)
{
    switch (c)
    {
    case '\r':
        break;

    case ' ':
        if (partial_tok_type == PartialToken::STRING_QUOTED
            || partial_tok_type == PartialToken::STRING_NAKED_CAPTURING_SPACES)
        {
            tok.push_back(c);
        }
        else // (partial_tok_type == PartialToken::STRING_NAKED)
        {
            this->FlushStringishToken(GDocTokenType::STRING);
        }
        line_pos++;
        break;

    case ',':
    case '\t':
        if (partial_tok_type == PartialToken::STRING_QUOTED)
        {
            tok.push_back(c);
        }
        else // (partial_tok_type == PartialToken::STRING_NAKED)
        {
            this->FlushStringishToken(GDocTokenType::STRING);
        }
        line_pos++;
        break;

    case '\n':
        if (partial_tok_type == PartialToken::STRING_QUOTED)
        {
            Logger::Log(LOG_WARN, "GenericFileFormat: line %zd, pos %zd: quoted string interrupted by newline", line_num, line_pos);
        }
        this->FlushStringishToken(GDocTokenType::STRING);
        // Break line
        doc.tokens.push_back({ GDocTokenType::LINEBREAK, 0.f });
        line_num++;
        line_pos = 0;
        break;

    case ':':
        if (options & GenericDocument::OPTION_ALLOW_SEPARATOR_COLON
            && (partial_tok_type == PartialToken::STRING_NAKED || partial_tok_type == PartialToken::STRING_NAKED_CAPTURING_SPACES))
        {
            this->FlushStringishToken(GDocTokenType::STRING);
        }
        else
        {
            tok.push_back(c);
        }
        line_pos++;
        break;

    case '=':
        if (options & GenericDocument::OPTION_ALLOW_SEPARATOR_EQUALS
            && (partial_tok_type == PartialToken::STRING_NAKED || partial_tok_type == PartialToken::STRING_NAKED_CAPTURING_SPACES))
        {
            this->FlushStringishToken(GDocTokenType::STRING);
        }
        else
        {
            tok.push_back(c);
        }
        line_pos++;
        break;

    case '"':
        if (partial_tok_type == PartialToken::STRING_QUOTED)
        {
            this->FlushStringishToken(GDocTokenType::STRING);
        }
        else // (partial_tok_type == PartialToken::STRING_NAKED)
        {
            partial_tok_type = PartialToken::GARBAGE;
            tok.push_back(c);
        }
        line_pos++;
        break;

    case '(':
        if (partial_tok_type == PartialToken::STRING_NAKED
            && options & GenericDocument::OPTION_PARENTHESES_CAPTURE_SPACES)
        {
            partial_tok_type = PartialToken::STRING_NAKED_CAPTURING_SPACES;
        }
        tok.push_back(c);
        line_pos++;
        break;

    case ')':
        if (partial_tok_type == PartialToken::STRING_NAKED_CAPTURING_SPACES)
        {
            partial_tok_type = PartialToken::STRING_NAKED;
        }
        tok.push_back(c);
        line_pos++;
        break;

    default:
        tok.push_back(c);
        line_pos++;
        break;
    }

    if (partial_tok_type == PartialToken::GARBAGE)
    {
        Logger::Log(LOG_WARN, "GenericFileFormat: line %zd, pos %zd: stray character '%c'", line_num, line_pos, c);
    }
}

void DocumentParser::UpdateNumber(const char c)
{
    switch (c)
    {
    case '\r':
        break;

    case ' ':
    case ',':
    case '\t':
        if (partial_tok_type == PartialToken::NUMBER_STUB_MINUS 
            && options & GenericDocument::OPTION_ALLOW_NAKED_STRINGS)
        {
            this->FlushStringishToken(GDocTokenType::STRING);
        }
        else
        {
            this->FlushNumericToken();
        }
        line_pos++;
        break;

    case '\n':
        if (partial_tok_type == PartialToken::NUMBER_STUB_MINUS 
            && options & GenericDocument::OPTION_ALLOW_NAKED_STRINGS)
        {
            this->FlushStringishToken(GDocTokenType::STRING);
        }
        else
        {
            this->FlushNumericToken();
        }
        // Break line
        doc.tokens.push_back({ GDocTokenType::LINEBREAK, 0.f });
        line_num++;
        line_pos = 0;
        break;

    case ':':
        if (options & GenericDocument::OPTION_ALLOW_SEPARATOR_COLON)
        {
            if (partial_tok_type == PartialToken::NUMBER_STUB_MINUS 
                && options & GenericDocument::OPTION_ALLOW_NAKED_STRINGS)
            {
                this->FlushStringishToken(GDocTokenType::STRING);
            }
            else
            {
                this->FlushNumericToken();
            }
        }
        else
        {
            this->DiscontinueNumber();
            tok.push_back(c);
        }
        line_pos++;
        break;

    case '=':
        if (options & GenericDocument::OPTION_ALLOW_SEPARATOR_EQUALS)
        {
            if (partial_tok_type == PartialToken::NUMBER_STUB_MINUS 
                && options & GenericDocument::OPTION_ALLOW_NAKED_STRINGS)
            {
                this->FlushStringishToken(GDocTokenType::STRING);
            }
            else
            {
                this->FlushNumericToken();
            }
        }
        else
        {
            this->DiscontinueNumber();
            tok.push_back(c);
        }
        line_pos++;
        break;

    case '.':
        if (partial_tok_type == PartialToken::NUMBER_INTEGER
            || partial_tok_type == PartialToken::NUMBER_STUB_MINUS)
        {
            partial_tok_type = PartialToken::NUMBER_DECIMAL;
        }
        else
        {
            this->DiscontinueNumber();
        }
        tok.push_back(c);
        line_pos++;
        break;

    case 'e':
    case 'E':
        if (partial_tok_type == PartialToken::NUMBER_DECIMAL
            || partial_tok_type == PartialToken::NUMBER_INTEGER)
        {
            partial_tok_type = PartialToken::NUMBER_SCIENTIFIC_STUB;
        }
        else
        {
            this->DiscontinueNumber();
        }
        tok.push_back(c);
        line_pos++;
        break;

    case '-':
        if (partial_tok_type == PartialToken::NUMBER_SCIENTIFIC_STUB)
        {
            partial_tok_type = PartialToken::NUMBER_SCIENTIFIC_STUB_MINUS;
        }
        else
        {
            this->DiscontinueNumber();
        }
        tok.push_back(c);
        line_pos++;
        break;

    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        if (partial_tok_type == PartialToken::NUMBER_SCIENTIFIC_STUB
            || partial_tok_type == PartialToken::NUMBER_SCIENTIFIC_STUB_MINUS)
        {
            partial_tok_type = PartialToken::NUMBER_SCIENTIFIC;
        }
        else if (partial_tok_type == PartialToken::NUMBER_STUB_MINUS)
        {
            partial_tok_type = PartialToken::NUMBER_INTEGER;
        }
        tok.push_back(c);
        line_pos++;
        break;

    default:
        this->DiscontinueNumber();
        tok.push_back(c);
        line_pos++;
        break;

    }

    if (partial_tok_type == PartialToken::GARBAGE)
    {
        Logger::Log(LOG_WARN, "GenericFileFormat: line %zd, pos %zd: stray character '%c' in number", line_num, line_pos, c);
    }
}

void DocumentParser::ProcessSeparatorWithinBool()
{
    this->DiscontinueBool();
    switch (partial_tok_type)
    {
        case PartialToken::KEYWORD:
            this->FlushStringishToken(GDocTokenType::KEYWORD);
            break;
        case PartialToken::STRING_NAKED:
            this->FlushStringishToken(GDocTokenType::STRING);
            break;
        default:
            // Discard token
            tok.push_back('\0');
            Logger::Log(LOG_WARN, "GenericFileFormat: line %zd, pos %zd: discarding incomplete boolean token '%s'", line_num, line_pos, tok.data());
            tok.clear();
            partial_tok_type = PartialToken::NONE;
            break;
    }
}

void DocumentParser::UpdateBool(const char c)
{
    switch (c)
    {
    case '\r':
        break;

    case ' ':
    case ',':
    case '\t':
        this->ProcessSeparatorWithinBool();
        line_pos++;
        break;

    case '\n':
        this->ProcessSeparatorWithinBool();
        // Break line
        doc.tokens.push_back({ GDocTokenType::LINEBREAK, 0.f });
        line_num++;
        line_pos = 0;
        break;

    case ':':
        if (options & GenericDocument::OPTION_ALLOW_SEPARATOR_COLON)
        {
            this->ProcessSeparatorWithinBool();
        }
        else
        {
            this->DiscontinueBool();
            tok.push_back(c);
        }
        line_pos++;
        break;

    case '=':
        if (options & GenericDocument::OPTION_ALLOW_SEPARATOR_EQUALS)
        {
            this->ProcessSeparatorWithinBool();
        }
        else
        {
            this->DiscontinueBool();
            tok.push_back(c);
        }
        line_pos++;
        break;

    case 'r':
        if (partial_tok_type != PartialToken::BOOL_TRUE || tok.size() != 1)
        {
            this->DiscontinueBool();
        }
        tok.push_back(c);
        line_pos++;
        break;

    case 'u':
        if (partial_tok_type != PartialToken::BOOL_TRUE || tok.size() != 2)
        {
            this->DiscontinueBool();
        }
        tok.push_back(c);
        line_pos++;
        break;

    case 'a':
        if (partial_tok_type != PartialToken::BOOL_FALSE || tok.size() != 1)
        {
            this->DiscontinueBool();
        }
        tok.push_back(c);
        line_pos++;
        break;

    case 'l':
        if (partial_tok_type != PartialToken::BOOL_FALSE || tok.size() != 2)
        {
            this->DiscontinueBool();
        }
        tok.push_back(c);
        line_pos++;
        break;

    case 's':
        if (partial_tok_type != PartialToken::BOOL_FALSE || tok.size() != 3)
        {
            this->DiscontinueBool();
        }
        tok.push_back(c);
        line_pos++;
        break;

    case 'e':
        if (partial_tok_type == PartialToken::BOOL_TRUE && tok.size() == 3)
        {
            doc.tokens.push_back({ GDocTokenType::BOOL, 1.f });
            tok.clear();
            partial_tok_type = PartialToken::NONE;
        }
        else if (partial_tok_type == PartialToken::BOOL_FALSE && tok.size() == 4)
        {
            doc.tokens.push_back({ GDocTokenType::BOOL, 0.f });
            tok.clear();
            partial_tok_type = PartialToken::NONE;
        }
        else
        {
            this->DiscontinueBool();
            tok.push_back(c);
        }
        line_pos++;
        break;

    default:
        this->DiscontinueBool();
        tok.push_back(c);
        line_pos++;
        break;
    }

    if (partial_tok_type == PartialToken::GARBAGE)
    {
        Logger::Log(LOG_WARN, "GenericFileFormat: line %zd, pos %zd: stray character '%c' in boolean", line_num, line_pos, c);
    }
}

void DocumentParser::DiscontinueBool()
{
    if (doc.tokens.size() == 0 || doc.tokens.back().type == GDocTokenType::LINEBREAK)
        partial_tok_type = PartialToken::KEYWORD;
    else if (options & GenericDocument::OPTION_ALLOW_NAKED_STRINGS)
        partial_tok_type = PartialToken::STRING_NAKED;
    else
        partial_tok_type = PartialToken::GARBAGE;
}

void DocumentParser::DiscontinueNumber()
{
    if (options & GenericDocument::OPTION_ALLOW_NAKED_STRINGS)
        partial_tok_type = PartialToken::STRING_NAKED;
    else
        partial_tok_type = PartialToken::GARBAGE;
}

void DocumentParser::DiscontinueKeyword()
{
    if (options & GenericDocument::OPTION_ALLOW_NAKED_STRINGS)
        partial_tok_type = PartialToken::STRING_NAKED;
    else
        partial_tok_type = PartialToken::GARBAGE;
}

void DocumentParser::UpdateKeyword(const char c)
{
    switch (c)
    {
    case '\r':
        break;

    case ' ':
    case ',':
    case '\t':
        this->FlushStringishToken(GDocTokenType::KEYWORD);
        line_pos++;
        break;

    case '\n':
        this->FlushStringishToken(GDocTokenType::KEYWORD);
        // Break line
        doc.tokens.push_back({ GDocTokenType::LINEBREAK, 0.f });
        line_num++;
        line_pos = 0;
        break;

    case ':':
        if (options & GenericDocument::OPTION_ALLOW_SEPARATOR_COLON)
        {
            this->FlushStringishToken(GDocTokenType::KEYWORD);
        }
        else
        {
            this->DiscontinueKeyword();
            tok.push_back(c);
        }
        line_pos++;
        break;

    case '=':
        if (options & GenericDocument::OPTION_ALLOW_SEPARATOR_EQUALS)
        {
            this->FlushStringishToken(GDocTokenType::KEYWORD);
        }
        else
        {
            this->DiscontinueKeyword();
            tok.push_back(c);
        }
        line_pos++;
        break;

    case '_':
        tok.push_back(c);
        line_pos++;
        break;

    case '(':
        if (options & GenericDocument::OPTION_ALLOW_NAKED_STRINGS)
        {
            if (options & GenericDocument::OPTION_PARENTHESES_CAPTURE_SPACES)
                partial_tok_type = PartialToken::STRING_NAKED_CAPTURING_SPACES;
            else
                partial_tok_type = PartialToken::STRING_NAKED;
        }
        else
        {
            partial_tok_type = PartialToken::GARBAGE;
        }
        tok.push_back(c);
        line_pos++;
        break;

    case ']':
        if (partial_tok_type == PartialToken::KEYWORD_BRACED)
        {
            partial_tok_type = PartialToken::KEYWORD; // Do not allow any more ']'.
        }
        else
        {
            this->DiscontinueKeyword();
        }
        tok.push_back(c);
        line_pos++;
        break;

    default:
        if (!isalnum(c))
        {
            this->DiscontinueKeyword();
        }
        tok.push_back(c);
        line_pos++;
        break;
    }

    if (partial_tok_type == PartialToken::GARBAGE)
    {
        Logger::Log(LOG_WARN, "GenericFileFormat: line %zd, pos %zd: stray character '%c' in keyword", line_num, line_pos, c);
    }
}

void DocumentParser::UpdateTitle(const char c)
{
    switch (c)
    {
    case '\r':
        break;

    case '\n':
        this->FlushStringishToken(GDocTokenType::STRING);
        // Break line
        doc.tokens.push_back({ GDocTokenType::LINEBREAK, 0.f });
        line_num++;
        line_pos = 0;
        break;

    default:
        tok.push_back(c);
        line_pos++;
        break;
    }
}

void DocumentParser::UpdateGarbage(const char c)
{
    switch (c)
    {
    case '\r':
        break;

    case ' ':
    case ',':
    case '\t':
    case '\n':
        tok.push_back('\0');
        Logger::Log(LOG_WARN, "GenericFileFormat: line %zd, pos %zd: discarding garbage token '%s'", line_num, line_pos, tok.data());
        tok.clear();
        partial_tok_type = PartialToken::NONE;
        line_pos++;
        break;

    default:
        tok.push_back(c);
        line_pos++;
        break;
    }
}

void DocumentParser::FlushStringishToken(GDocTokenType type)
{
    doc.tokens.push_back({ type, (float)doc.string_pool.size() });
    tok.push_back('\0');
    std::copy(tok.begin(), tok.end(), std::back_inserter(doc.string_pool));
    tok.clear();
    partial_tok_type = PartialToken::NONE;
}

void DocumentParser::FlushNumericToken()
{
    tok.push_back('\0');
    if (partial_tok_type == PartialToken::NUMBER_INTEGER)
    {
        char* end;
        long val = strtol_l(tok.data(), &end, 0, _numLocale);
        if (tok.data() != end)
        {
            doc.tokens.push_back({ GDocTokenType::INT, (float)val });
        }
        else
        {
            Logger::Log(LOG_ERROR, "GenericFileFormat: could not parse '%s' as INTEGER", tok.data());
        }
    }
    else
    {
        char* end;
        double val = strtod_l(tok.data(), &end, _numLocale);
        if (tok.data() != end)
        {
            doc.tokens.push_back({ GDocTokenType::FLOAT, (float)val });
        }
        else
        {
            Logger::Log(LOG_ERROR, "GenericFileFormat: could not parse '%s' as FLOAT", tok.data());
        }
    }
    tok.clear();
    partial_tok_type = PartialToken::NONE;
}

void DocumentParser::ProcessChar(const char c)
{
    switch (partial_tok_type)
    {
    case PartialToken::NONE:
        this->BeginToken(c);
        break;

    case PartialToken::COMMENT_SEMICOLON:
    case PartialToken::COMMENT_SLASH:
    case PartialToken::COMMENT_HASH:
        this->UpdateComment(c);
        break;

    case PartialToken::STRING_QUOTED:
    case PartialToken::STRING_NAKED:
    case PartialToken::STRING_NAKED_CAPTURING_SPACES:
        this->UpdateString(c);
        break;

    case PartialToken::NUMBER_INTEGER:
    case PartialToken::NUMBER_STUB_MINUS:
    case PartialToken::NUMBER_DECIMAL:
    case PartialToken::NUMBER_SCIENTIFIC:
    case PartialToken::NUMBER_SCIENTIFIC_STUB:
    case PartialToken::NUMBER_SCIENTIFIC_STUB_MINUS:
        this->UpdateNumber(c);
        break;

    case PartialToken::BOOL_TRUE:
    case PartialToken::BOOL_FALSE:
        this->UpdateBool(c);
        break;

    case PartialToken::KEYWORD:
    case PartialToken::KEYWORD_BRACED:
        this->UpdateKeyword(c);
        break;

    case PartialToken::TITLE_STRING:
        this->UpdateTitle(c);
        break;

    case PartialToken::GARBAGE:
        this->UpdateGarbage(c);
        break;
    }
}

void DocumentParser::ProcessEOF()
{
    // Flush any partial token
    switch (partial_tok_type)
    {
    case PartialToken::STRING_QUOTED:
    case PartialToken::STRING_NAKED_CAPTURING_SPACES:
    case PartialToken::TITLE_STRING:
        this->FlushStringishToken(GDocTokenType::STRING);
        break;

    case PartialToken::KEYWORD_BRACED:
        this->FlushStringishToken(GDocTokenType::KEYWORD);
        break;

    default:
        this->ProcessChar(' '); // Pretend processing a separator to flush any partial whitespace-incompatible token.
        break;
    }

    // Ensure newline at end of file
    if (doc.tokens.size() == 0 || doc.tokens.back().type != GDocTokenType::LINEBREAK)
    {
        doc.tokens.push_back({ GDocTokenType::LINEBREAK, 0.f });
    }
}

bool GenericDocument::loadFromFile(const std::string& filename, const BitMask_t options)
{
    FILE* f = fopen(filename.c_str(), "r");
    if (!f)
    {
        Logger::Log(LOG_ERROR, "GenericDocument::loadFromFile(): Could not open '%s'", filename.c_str());
        return false;
    }

    // Reset the document
    tokens.clear();
    string_pool.clear();

    // Prepare context
    DocumentParser parser(*this, options, f);

    // Parse the text
    while (!feof(f))
    {
        const char c = (const char)fgetc(f);
        parser.ProcessChar(c);
    }
    parser.ProcessEOF();
    return true;
}

bool GenericDocument::saveToFile(const std::string& filename)
{
    FILE* f = fopen(filename.c_str(), "w");
    if (!f)
    {
        Logger::Log(LOG_ERROR, "GenericDocument::saveToFile(): Could not open '%s'", filename.c_str());
        return false;
    }

    std::string separator;
    const char* pool_str = nullptr;

    for (Token& tok : tokens)
    {
        switch (tok.type)
        {
        case GDocTokenType::LINEBREAK:
            fprintf(f, "\n");
            separator = "";
            break;

        case GDocTokenType::COMMENT:
            pool_str = string_pool.data() + (size_t)tok.data;
            fprintf(f, ";%s", pool_str);
            break;

        case GDocTokenType::STRING:
            pool_str = string_pool.data() + (size_t)tok.data;
            fprintf(f, "%s%s", separator.c_str(), pool_str);
            separator = ", ";
            break;

        case GDocTokenType::FLOAT:
            fprintf(f, "%s%g", separator.c_str(), tok.data);
            separator = ", ";
            break;

        case GDocTokenType::INT:
            fprintf(f, "%s%d", separator.c_str(), (int)tok.data);
            separator = ", ";
            break;

        case GDocTokenType::BOOL:
            fprintf(f, "%s%s", separator.c_str(), tok.data == 1.f ? "true" : "false");
            separator = ", ";
            break;

        case GDocTokenType::KEYWORD:
            pool_str = string_pool.data() + (size_t)tok.data;
            fprintf(f, "%s", pool_str);
            separator = " ";
            break;
        }
    }
    return true;
}

bool GenericDocContext::seekNextLine()
{
    // Skip current line
    while (!this->endOfFile() && this->tokenType() != GDocTokenType::LINEBREAK)
    {
        this->moveNext();
    }
    this->moveNext();

    // Skip comments and empty lines
    while (!this->endOfFile() && (this->isTokComment(0) || this->isTokLineBreak(0)))
    {
        this->moveNext();
    }

    return this->endOfFile();
}

int GenericDocContext::countLineArgs()
{
    int count = 0;
    while (!endOfFile(count) && this->tokenType(count) != GDocTokenType::LINEBREAK)
        count++;
    return count;
}

// -----------------
// Editing functions

void GenericDocContext::appendTokens(int count)
{
    if (count <= 0)
        return;

    token_pos = (int)doc->tokens.size();
    for (int i = 0; i < count; i++)
    {
        doc->tokens.push_back({ GDocTokenType::NONE, 0.f });
    }
}

bool GenericDocContext::insertToken(int offset)
{
    if (endOfFile(offset))
       return false;

    doc->tokens.insert(doc->tokens.begin() + token_pos + offset, { GDocTokenType::NONE, 0.f });
    return true;
}

bool GenericDocContext::eraseToken(int offset)
{
    if (endOfFile(offset))
       return false;

    // Just erase the token.
    // We don't care about garbage in `string_pool` - the strings are usually just 1-6 characters long anyway.

    doc->tokens.erase(doc->tokens.begin() + token_pos + offset);
    return true;
}

bool GenericDocContext::setStringData(int offset, GDocTokenType type, const std::string& data)
{
    if (endOfFile(offset))
       return false;

    // Insert the string at the end of the string_pool
    // We don't care about order - updating string offsets in tokens would be complicated and unlikely helpful.
    
    doc->tokens[token_pos + offset] = { type, (float)doc->string_pool.size() };
    std::copy(data.begin(), data.end(), std::back_inserter(doc->string_pool));
    doc->string_pool.push_back('\0');
    return true;
}

bool GenericDocContext::setFloatData(int offset, GDocTokenType type, float data)
{
    if (endOfFile(offset))
       return false;

    doc->tokens[token_pos + offset] = { type, data };
    return true;
}

 // ----------------------------- registering AngelScript bindings --------------------------------- //

// Factories
static GenericDocument* GenericDocumentFactory()
{
    return new GenericDocument();
}

static GenericDocContext* GenericDocContextFactory(GenericDocument* doc)
{
    return new GenericDocContext(doc);
}

void RegisterGenericFileFormat(asIScriptEngine* engine)
{
    // enum TokenType
    engine->RegisterEnum("TokenType");
    engine->RegisterEnumValue("TokenType", "TOKEN_TYPE_NONE", (int)GDocTokenType::NONE);
    engine->RegisterEnumValue("TokenType", "TOKEN_TYPE_LINEBREAK", (int)GDocTokenType::LINEBREAK);
    engine->RegisterEnumValue("TokenType", "TOKEN_TYPE_COMMENT", (int)GDocTokenType::COMMENT);
    engine->RegisterEnumValue("TokenType", "TOKEN_TYPE_STRING", (int)GDocTokenType::STRING);
    engine->RegisterEnumValue("TokenType", "TOKEN_TYPE_FLOAT", (int)GDocTokenType::FLOAT);
    engine->RegisterEnumValue("TokenType", "TOKEN_TYPE_INT", (int)GDocTokenType::INT);
    engine->RegisterEnumValue("TokenType", "TOKEN_TYPE_BOOL", (int)GDocTokenType::BOOL);
    engine->RegisterEnumValue("TokenType", "TOKEN_TYPE_KEYWORD", (int)GDocTokenType::KEYWORD);


    // GenericDocument constants
    engine->RegisterEnum("GenericDocumentOptions");
    engine->RegisterEnumValue("GenericDocumentOptions", "GENERIC_DOCUMENT_OPTION_ALLOW_NAKED_STRINGS", GenericDocument::OPTION_ALLOW_NAKED_STRINGS);
    engine->RegisterEnumValue("GenericDocumentOptions", "GENERIC_DOCUMENT_OPTION_ALLOW_SLASH_COMMENTS", GenericDocument::OPTION_ALLOW_SLASH_COMMENTS);
    engine->RegisterEnumValue("GenericDocumentOptions", "GENERIC_DOCUMENT_OPTION_FIRST_LINE_IS_TITLE", GenericDocument::OPTION_FIRST_LINE_IS_TITLE);
    engine->RegisterEnumValue("GenericDocumentOptions", "GENERIC_DOCUMENT_OPTION_ALLOW_SEPARATOR_COLON", GenericDocument::OPTION_ALLOW_SEPARATOR_COLON);
    engine->RegisterEnumValue("GenericDocumentOptions", "GENERIC_DOCUMENT_OPTION_PARENTHESES_CAPTURE_SPACES", GenericDocument::OPTION_PARENTHESES_CAPTURE_SPACES);
    engine->RegisterEnumValue("GenericDocumentOptions", "GENERIC_DOCUMENT_OPTION_ALLOW_BRACED_KEYWORDS", GenericDocument::OPTION_ALLOW_BRACED_KEYWORDS);
    engine->RegisterEnumValue("GenericDocumentOptions", "GENERIC_DOCUMENT_OPTION_ALLOW_SEPARATOR_EQUALS", GenericDocument::OPTION_ALLOW_SEPARATOR_EQUALS);
    engine->RegisterEnumValue("GenericDocumentOptions", "GENERIC_DOCUMENT_OPTION_ALLOW_HASH_COMMENTS", GenericDocument::OPTION_ALLOW_HASH_COMMENTS);


    // class GenericDocument
    engine->RegisterObjectType("GenericDocumentClass", sizeof(GenericDocument), asOBJ_REF);
    engine->RegisterObjectBehaviour("GenericDocumentClass", asBEHAVE_FACTORY, "GenericDocumentClass@ f()", asFUNCTION(GenericDocumentFactory), asCALL_CDECL);
    // Registering the addref/release behaviours
    engine->RegisterObjectBehaviour("GenericDocumentClass", asBEHAVE_ADDREF, "void f()", asMETHOD(GenericDocument, AddRef), asCALL_THISCALL);
    engine->RegisterObjectBehaviour("GenericDocumentClass", asBEHAVE_RELEASE, "void f()", asMETHOD(GenericDocument, Release), asCALL_THISCALL);

    // RORSERVER: This is the only difference from client's `GenericDocumentClass`
    engine->RegisterObjectMethod("GenericDocumentClass", "bool loadFromFile(string const&in,int)", asMETHOD(GenericDocument, loadFromFile), asCALL_THISCALL);
    engine->RegisterObjectMethod("GenericDocumentClass", "bool saveToFile(string const&in)", asMETHOD(GenericDocument, saveToFile), asCALL_THISCALL);


    // class GenericDocContext
    // (Please maintain the same order as in 'GenericFileFormat.h' and 'doc/*/GenericDocContextClass.h')
    engine->RegisterObjectType("GenericDocContextClass", sizeof(GenericDocument), asOBJ_REF);
    engine->RegisterObjectBehaviour("GenericDocContextClass", asBEHAVE_FACTORY, "GenericDocContextClass@ f()", asFUNCTION(GenericDocContextFactory), asCALL_CDECL);
    // Registering the addref/release behaviours
    engine->RegisterObjectBehaviour("GenericDocContextClass", asBEHAVE_ADDREF, "void f()", asMETHOD(GenericDocument, AddRef), asCALL_THISCALL);
    engine->RegisterObjectBehaviour("GenericDocContextClass", asBEHAVE_RELEASE, "void f()", asMETHOD(GenericDocument, Release), asCALL_THISCALL);

    engine->RegisterObjectMethod("GenericDocContextClass", "bool moveNext()", asMETHOD(GenericDocContext, moveNext), asCALL_THISCALL);
    engine->RegisterObjectMethod("GenericDocContextClass", "uint getPos()", asMETHOD(GenericDocContext, getPos), asCALL_THISCALL);
    engine->RegisterObjectMethod("GenericDocContextClass", "bool seekNextLine()", asMETHOD(GenericDocContext, seekNextLine), asCALL_THISCALL);
    engine->RegisterObjectMethod("GenericDocContextClass", "uint countLineArgs()", asMETHOD(GenericDocContext, countLineArgs), asCALL_THISCALL);
    engine->RegisterObjectMethod("GenericDocContextClass", "bool endOfFile(int offset = 0)", asMETHOD(GenericDocContext, endOfFile), asCALL_THISCALL);
    engine->RegisterObjectMethod("GenericDocContextClass", "GDocTokenType tokenType(int offset = 0)", asMETHOD(GenericDocContext, tokenType), asCALL_THISCALL);

    engine->RegisterObjectMethod("GenericDocContextClass", "string getTokString(int offset = 0)", asMETHOD(GenericDocContext, getTokString), asCALL_THISCALL);
    engine->RegisterObjectMethod("GenericDocContextClass", "float getTokFloat(int offset = 0)", asMETHOD(GenericDocContext, getTokFloat), asCALL_THISCALL);
    engine->RegisterObjectMethod("GenericDocContextClass", "int getTokInt(int offset = 0)", asMETHOD(GenericDocContext, getTokInt), asCALL_THISCALL);
    engine->RegisterObjectMethod("GenericDocContextClass", "bool getTokBool(int offset = 0)", asMETHOD(GenericDocContext, getTokBool), asCALL_THISCALL);
    engine->RegisterObjectMethod("GenericDocContextClass", "string getTokKeyword(int offset = 0)", asMETHOD(GenericDocContext, getTokKeyword), asCALL_THISCALL);
    engine->RegisterObjectMethod("GenericDocContextClass", "string getTokComment(int offset = 0)", asMETHOD(GenericDocContext, getTokComment), asCALL_THISCALL);
    
    engine->RegisterObjectMethod("GenericDocContextClass", "bool isTokString(int offset = 0)", asMETHOD(GenericDocContext, isTokString), asCALL_THISCALL);
    engine->RegisterObjectMethod("GenericDocContextClass", "bool isTokFloat(int offset = 0)", asMETHOD(GenericDocContext, isTokFloat), asCALL_THISCALL);
    engine->RegisterObjectMethod("GenericDocContextClass", "bool isTokInt(int offset = 0)", asMETHOD(GenericDocContext, isTokInt), asCALL_THISCALL);
    engine->RegisterObjectMethod("GenericDocContextClass", "bool isTokBool(int offset = 0)", asMETHOD(GenericDocContext, isTokBool), asCALL_THISCALL);
    engine->RegisterObjectMethod("GenericDocContextClass", "bool isTokKeyword(int offset = 0)", asMETHOD(GenericDocContext, isTokKeyword), asCALL_THISCALL);
    engine->RegisterObjectMethod("GenericDocContextClass", "bool isTokComment(int offset = 0)", asMETHOD(GenericDocContext, isTokComment), asCALL_THISCALL);
    engine->RegisterObjectMethod("GenericDocContextClass", "bool isTokLineBreak(int offset = 0)", asMETHOD(GenericDocContext, isTokLineBreak), asCALL_THISCALL);

    // > Editing functions:
    engine->RegisterObjectMethod("GenericDocContextClass", "void appendTokens(int count)", asMETHOD(GenericDocContext, appendTokens), asCALL_THISCALL);
    engine->RegisterObjectMethod("GenericDocContextClass", "bool insertToken(int offset = 0)", asMETHOD(GenericDocContext, insertToken), asCALL_THISCALL);
    engine->RegisterObjectMethod("GenericDocContextClass", "bool eraseToken(int offset = 0)", asMETHOD(GenericDocContext, eraseToken), asCALL_THISCALL);

    engine->RegisterObjectMethod("GenericDocContextClass", "void appendTokString(const string &in)", asMETHOD(GenericDocContext, appendTokString), asCALL_THISCALL);
    engine->RegisterObjectMethod("GenericDocContextClass", "void appendTokFloat(float)", asMETHOD(GenericDocContext, appendTokFloat), asCALL_THISCALL);
    engine->RegisterObjectMethod("GenericDocContextClass", "void appendTokInt(int)", asMETHOD(GenericDocContext, appendTokInt), asCALL_THISCALL);
    engine->RegisterObjectMethod("GenericDocContextClass", "void appendTokBool(bool)", asMETHOD(GenericDocContext, appendTokBool), asCALL_THISCALL);
    engine->RegisterObjectMethod("GenericDocContextClass", "void appendTokKeyword(const string &in)", asMETHOD(GenericDocContext, appendTokKeyword), asCALL_THISCALL);
    engine->RegisterObjectMethod("GenericDocContextClass", "void appendTokComment(const string &in)", asMETHOD(GenericDocContext, appendTokComment), asCALL_THISCALL);
    engine->RegisterObjectMethod("GenericDocContextClass", "void appendTokLineBreak()", asMETHOD(GenericDocContext, appendTokLineBreak), asCALL_THISCALL);

    engine->RegisterObjectMethod("GenericDocContextClass", "bool setTokString(int offset, const string &in)", asMETHOD(GenericDocContext, setTokString), asCALL_THISCALL);
    engine->RegisterObjectMethod("GenericDocContextClass", "bool setTokFloat(int offset, float)", asMETHOD(GenericDocContext, setTokFloat), asCALL_THISCALL);
    engine->RegisterObjectMethod("GenericDocContextClass", "bool setTokInt(int offset, int)", asMETHOD(GenericDocContext, setTokInt), asCALL_THISCALL);
    engine->RegisterObjectMethod("GenericDocContextClass", "bool setTokBool(int offset, bool)", asMETHOD(GenericDocContext, setTokBool), asCALL_THISCALL);
    engine->RegisterObjectMethod("GenericDocContextClass", "bool setTokKeyword(int offset, const string &in)", asMETHOD(GenericDocContext, setTokKeyword), asCALL_THISCALL);
    engine->RegisterObjectMethod("GenericDocContextClass", "bool setTokComment(int offset, const string &in)", asMETHOD(GenericDocContext, setTokComment), asCALL_THISCALL);
    engine->RegisterObjectMethod("GenericDocContextClass", "bool setTokLineBreak(int offset)", asMETHOD(GenericDocContext, setTokLineBreak), asCALL_THISCALL);

}
