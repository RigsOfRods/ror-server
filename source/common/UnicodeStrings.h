
// ############################################# //
//                 PUBLIC DOMAIN                 //
//  This file is not covered by GPLv3 license    //
//   of the host project (Rigs of Rods), but     //
//    separately released to public domain       //
//    under license CC0 by Creative Commons.     //
//                                               //
//      This notice can be freely removed.       //
// ############################################# //

/// @file     UnicodeStrings.h
/// @author   Petr Ohlidal
/// @date     01/2017
/// @brief    String utilities and guidelines

/// STRING HANDLING GUIDELINES
///
/// char,     std::string    = UTF-8 (2003's spec -> max 4 bytes!)
/// char16_t, std::u16string = UTF-16LE (not UCS-2 -> 2 or 4 bytes!)
/// char32_t, std::u32string = UTF-32LE
/// wchar_t,  std::wstring   = Forbidden, except for using wchar APIs.
///
/// Inspired by http://www.utf8everywhere.org
///
/// CRASH COURSE OF TEXT PROCESSING
///
/// It's a 2 step process:
/// 1. Take a glyph, i.e. `A` and consult a table called "Coded character set"[2]
///    (short: "charset") for an unique numeric ID, called "code point"[1][2]
///    Examples: `A` is 0x41 in ASCII and Unicode[3] standard charsets.
/// 2. Take the code point and use one of the charset's "encoding"-s to convert it
///    to a sequence of equally-sized binary values called "code units".
///    Examples: ASCII has single encoding: ANSI, Unicode has many: UCS-2/4(BE/LE),
///    UTF-8, UTF-16/32(BE/LE) and some obscure unused ones like UTF-1.
///
/// About "endianness" a.k.a byte order:
/// Number 15 in 2 bytes would be [00000000 00001111] in a textbook or computer
/// with BE (big endian) architecture. The opposite is LE (little endian)
/// architecture, which uses [00001111 00000000] (the same 2 bytes, but reversed)[4]
///
/// References:
/// [1] https://en.wikipedia.org/wiki/Code_point
/// [2] https://en.wikipedia.org/wiki/Character_encoding#Terminology
/// [3] https://en.wikipedia.org/wiki/Universal_Coded_Character_Set
/// [4] https://en.wikipedia.org/wiki/Endianness
///
/// This file implements only bare essentials of Unicode handling.
/// For more complete solutions, check utf8proc or ICU projects.
/// We get majority of our text from textfiles and online resources where
/// UTF-8 is a respected solution. And we mostly just display the text.
/// We only really need conversions for 3rd party APIs.

#pragma once

#include <string>
#include <string.h>

namespace Str
{
    // UTF-8 byte signatures
    //  Spec: 0/1 are bit values, 'x' are codepoint bits
    //
    // type  | spec     | mask     | value
    // -----------------------------------
    // 1byte | 0xxxxxxx | 10000000 | 00000000
    // 2byte | 110xxxxx | 11100000 | 11000000
    // 3byte | 1110xxxx | 11110000 | 11100000
    // 4byte | 11110xxx | 11111000 | 11110000
    // cont. | 10xxxxxx | 11000000 | 10000000

    static const char UTF8_SIG_LEAD_1b       = '\x00';
    static const char UTF8_SIG_LEAD_2b       = '\xC0';
    static const char UTF8_SIG_LEAD_3b       = '\xE0';
    static const char UTF8_SIG_LEAD_4b       = '\xF0';
    static const char UTF8_SIG_CONT          = '\x80';
    static const char UTF8_MASK_LEAD_1b      = '\x80';
    static const char UTF8_MASK_LEAD_2b      = '\xE0';
    static const char UTF8_MASK_LEAD_3b      = '\xF0';
    static const char UTF8_MASK_LEAD_4b      = '\xF8';
    static const char UTF8_MASK_CONT         = '\xC0';
    static const char *UTF8_REPLACEMENT_CHAR = u8"\uFFFD";  // The � REPLACEMENT CHARACTER

    inline bool IsUtf8Lead1b(const char c)
    {
        return (c & UTF8_MASK_LEAD_1b) == UTF8_SIG_LEAD_1b;
    }

    inline bool IsUtf8Lead2b(const char c)
    {
        return (c & UTF8_MASK_LEAD_2b) == UTF8_SIG_LEAD_2b;
    }

    inline bool IsUtf8Lead3b(const char c)
    {
        return (c & UTF8_MASK_LEAD_3b) == UTF8_SIG_LEAD_3b;
    }

    inline bool IsUtf8Lead4b(const char c)
    {
        return (c & UTF8_MASK_LEAD_4b) == UTF8_SIG_LEAD_4b;
    }

    inline bool IsUtf8Cont(const char c)
    {
        return (c & UTF8_MASK_CONT) == UTF8_SIG_CONT;
    }

    /// Mercilessly replaces all invalid code units with supplied sequence.
    /// OctetIterator_T can be either STL iterator or plain old `const char*`.
    /// @param OctetIterator_T start Start of source string. Required.
    /// @param OctetIterator_T end   End of source. Required.
    /// @param const char*     sub   Substitute sequence; default: U+FFFD � REPLACEMENT CHARACTER.
    template<typename OctetIterator_T>
    std::string SanitizeUtf8(OctetIterator_T start, OctetIterator_T end, const char *sub = UTF8_REPLACEMENT_CHAR)
    {
        std::string res;
        char        buffer[]    = { '\0', '\0', '\0', '\0', '\0' }; // 4 octets + NULL terminator
        int         buffer_next = 0;                                // Next available octet slot
        int         buffer_max  = 0;                                // Current character's declared number of octets.

        for (OctetIterator_T pos = start; pos != end; ++pos)
        {
            if (buffer_next == 0)    // We're at lead byte and must detect.
            {
                if (IsUtf8Lead1b(*pos))
                {
                    res += *pos;
                }
                else if (IsUtf8Cont(*pos))
                {
                    res += sub;
                }
                else
                {
                    buffer_max            = (IsUtf8Lead2b(*pos)) ? 2 : buffer_max;
                    buffer_max            = (IsUtf8Lead3b(*pos)) ? 3 : buffer_max;
                    buffer_max            = (IsUtf8Lead4b(*pos)) ? 4 : buffer_max;
                    buffer[buffer_next++] = *pos;
                }
            }
            else
            {
                if (IsUtf8Cont(*pos))
                {
                    buffer[buffer_next++] = *pos;
                    if (buffer_next == buffer_max)
                    {
                        buffer[buffer_max] = '\0';
                        res               += buffer;
                        buffer_next        = 0;
                    }
                }
                else
                {
                    res        += sub;
                    buffer_next = 0;
                }
            }
        }

        if (buffer_next != 0)
            res += sub;

        return res;   // We rely on C++11's move semantics -> no copy here.
    }

    /// Convenience overload for plain old NULL-terminated C-strings
    inline std::string SanitizeUtf8(const char *str, const char *sub = UTF8_REPLACEMENT_CHAR)
    {
        return SanitizeUtf8(str, str + strlen(str), sub);
    }

    /// Replicates behavior of `isspace()` under "C" locale - to be independent and faster.
    inline bool IsWhitespaceAscii(char c)
    {
        return (c == ' ')      // (0x20)  space (SPC)
               || (c == '\t')  // (0x09)  horizontal tab (TAB)
               || (c == '\n')  // (0x0a)  newline (LF)
               || (c == '\v')  // (0x0b)  vertical tab (VT)
               || (c == '\f')  // (0x0c)  feed (FF)
               || (c == '\r'); // (0x0d)  carriage return (CR)
    }

    // TODO: implement `TrimUtf8()`!
    /// @param start Pointer to first character (or NUL-terminating character if the string is empty)
    /// @param end Pointer to after-the-last character (the NUL-terminating character)
    inline void TrimAscii(char *&start, char *&end)
    {
        while ((start != end) && IsWhitespaceAscii(*start))
        {
            ++start;
        }

        while ((start != end) && IsWhitespaceAscii(*(end - 1)))
        {
            --end;
        }
    }
} // namespace Str
