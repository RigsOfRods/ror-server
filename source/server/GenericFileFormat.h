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

/// @file
/// @brief Generic text file parser
/// 
/// Syntax:
///  - Lines starting with semicolon (;) (ignoring leading whitespace) are comments
///  - Separators are space, tabulator or comma (,)
///  - All strings must be in double quotes (except if OPTION_ALLOW_NAKED_STRINGS is used).
///  - If the first argument on line is an unqoted string, it's considered KEYWORD token type.
///  - Reserved keywords are 'true' and 'false' for the BOOL token type.
/// 
/// Remarks:
///  - Strings cannot be multiline. Linebreak within string ends the string.
///  - KEYWORD tokens cannot start with a digit or special character.

#pragma once

#include "rornet.h"

#include <angelscript.h>
#include <vector>
#include <string>
#include <cassert>

enum class GDocTokenType // RORSERVER: This is just `RoR::TokenType` on client, but here it conflicts with <winnt.h> `_TOKEN_INFORMATION_CLASS::TokenType`
{
    NONE,
    LINEBREAK,    // Input: LF (CR is ignored); Output: platform-specific.
    COMMENT,      // Line starting with ; (skipping whitespace). Data: offset in string pool.
    STRING,       // Quoted string. Data: offset in string pool.
    FLOAT,        // Numbers with or without a decimal point.
    INT,          // Only numbers without decimal point.
    BOOL,         // Lowercase 'true'/'false'. Data: 1.0 for true, 0.0 for false.
    KEYWORD,      // Unquoted string at start of line (skipping whitespace). Data: offset in string pool.
};

struct Token
{
    GDocTokenType type;
    float     data;
};

struct GenericDocument
{
    static const BitMask_t OPTION_ALLOW_NAKED_STRINGS = BITMASK(1); //!< Allow strings without quotes, for backwards compatibility.
    static const BitMask_t OPTION_ALLOW_SLASH_COMMENTS = BITMASK(2); //!< Allow comments starting with `//`. 
    static const BitMask_t OPTION_FIRST_LINE_IS_TITLE = BITMASK(3); //!< First non-empty & non-comment line is a naked string with spaces.
    static const BitMask_t OPTION_ALLOW_SEPARATOR_COLON = BITMASK(4); //!< Allow ':' as separator between tokens.
    static const BitMask_t OPTION_PARENTHESES_CAPTURE_SPACES = BITMASK(5); //!< If non-empty NAKED string encounters '(', following spaces will be captured until matching ')' is found.
    static const BitMask_t OPTION_ALLOW_BRACED_KEYWORDS = BITMASK(6); //!< Allow INI-like '[keyword]' tokens.
    static const BitMask_t OPTION_ALLOW_SEPARATOR_EQUALS = BITMASK(7); //!< Allow '=' as separator between tokens.
    static const BitMask_t OPTION_ALLOW_HASH_COMMENTS = BITMASK(8); //!< Allow comments starting with `#`. 

    virtual ~GenericDocument() {};

    std::vector<char> string_pool; // Data of COMMENT/KEYWORD/STRING tokens; NUL-terminated strings.
    std::vector<Token> tokens;

    virtual bool loadFromFile(const std::string& filename, BitMask_t options = 0); //!< Loaded from dedicated 'storage' folder specified in server config.
    virtual bool saveToFile(const std::string& filename); //!< Stored to dedicated 'storage' folder specified in server config.

    // AngelScript reference counting
    void AddRef()
    {
        // Increase the reference counter
        refCount++;
    }
    void Release()
    {
        // Decrease ref count and delete if it reaches 0
        if( --refCount == 0 )
            delete this;
    }
private:
    int refCount = 1;
};

struct GenericDocContext
{
    GenericDocContext(GenericDocument* d) : doc(d)
    {
        assert(doc != nullptr);
        if (doc == nullptr && asGetActiveContext() != nullptr)
        {
            asGetActiveContext()->SetException("Cannot create GenericDocContextClass from null GenericDocument!");
        }
    }
    virtual ~GenericDocContext() {};

    GenericDocument* doc;
    uint32_t token_pos = 0;

    // PLEASE maintain the same order as in 'bindings/GenericFileFormatAngelscript.cpp' and 'doc/*/GenericDocContextClass.h'

    bool moveNext() { token_pos++; return endOfFile(); }
    uint32_t getPos() const { return token_pos; }
    bool seekNextLine();
    int countLineArgs();
    bool endOfFile(int offset = 0) const { return token_pos + offset >= doc->tokens.size(); }
    GDocTokenType tokenType(int offset = 0) const { return !endOfFile(offset) ? doc->tokens[token_pos + offset].type : GDocTokenType::NONE; }

    std::string getTokString(int offset = 0) const { assert(isTokString(offset)); return getStringData(offset); }
    float getTokFloat(int offset = 0) const { assert(isTokFloat(offset)); return getFloatData(offset); }
    int getTokInt(int offset = 0) const { assert(isTokInt(offset)); return (int)getFloatData(offset); }
    float getTokNumeric(int offset = 0) const { assert(isTokNumeric(offset)); return getFloatData(offset); }
    bool getTokBool(int offset = 0) const { assert(isTokBool(offset)); return getFloatData(offset) == 1.f; }
    std::string getTokKeyword(int offset = 0) const { assert(isTokKeyword(offset)); return getStringData(offset); }
    std::string getTokComment(int offset = 0) const { assert(isTokComment(offset)); return getStringData(offset); }

    bool isTokString(int offset = 0) const { return tokenType(offset) == GDocTokenType::STRING; }
    bool isTokFloat(int offset = 0) const { return tokenType(offset) == GDocTokenType::FLOAT || tokenType(offset) == GDocTokenType::INT; }
    bool isTokInt(int offset = 0) const { return tokenType(offset) == GDocTokenType::INT; }
    bool isTokBool(int offset = 0) const { return tokenType(offset) == GDocTokenType::BOOL; }
    bool isTokKeyword(int offset = 0) const { return tokenType(offset) == GDocTokenType::KEYWORD; }
    bool isTokComment(int offset = 0) const { return tokenType(offset) == GDocTokenType::COMMENT; }
    bool isTokLineBreak(int offset = 0) const { return tokenType(offset) == GDocTokenType::LINEBREAK; }
    bool isTokNumeric(int offset = 0) const { return isTokInt(offset) || isTokFloat(offset); }

    // Editing functions:

    void appendTokens(int count); //!< Appends a series of `GDocTokenType::NONE` and sets Pos at the first one added; use `setTok*` functions to fill them.
    bool insertToken(int offset = 0); //!< Inserts `GDocTokenType::NONE`; @return false if offset is beyond EOF
    bool eraseToken(int offset = 0); //!< @return false if offset is beyond EOF

    void appendTokString(const std::string& str) { appendTokens(1); setTokString(0, str); }
    void appendTokFloat(float val) { appendTokens(1); setTokFloat(0, val); }
    void appendTokInt(int val) { appendTokens(1); setTokInt(0, val); }
    void appendTokBool(bool val) { appendTokens(1); setTokBool(0, val); }
    void appendTokKeyword(const std::string& str) { appendTokens(1); setTokKeyword(0, str); }
    void appendTokComment(const std::string& str) { appendTokens(1); setTokComment(0, str); }
    void appendTokLineBreak() { appendTokens(1); setTokLineBreak(0); }

    bool setTokString(int offset, const std::string& str) { return setStringData(offset, GDocTokenType::STRING, str); }
    bool setTokFloat(int offset, float val) { return setFloatData(offset, GDocTokenType::FLOAT, val); }
    bool setTokInt(int offset, int val) { return setFloatData(offset, GDocTokenType::INT, (float)val); }
    bool setTokBool(int offset, bool val) { return setFloatData(offset, GDocTokenType::BOOL, val); }
    bool setTokKeyword(int offset, const std::string& str) { return setStringData(offset, GDocTokenType::KEYWORD, str); }
    bool setTokComment(int offset, const std::string& str) { return setStringData(offset, GDocTokenType::COMMENT, str); }
    bool setTokLineBreak(int offset) { return setFloatData(offset, GDocTokenType::LINEBREAK, 0.f); }

    // Not exported to script:

    const char* getStringData(int offset = 0) const { return !endOfFile(offset) ? (doc->string_pool.data() + (uint32_t)doc->tokens[token_pos + offset].data) : ""; }
    float getFloatData(int offset = 0) const { return !endOfFile(offset) ? doc->tokens[token_pos + offset].data : 0.f; }
    bool setStringData(int offset, GDocTokenType type, const std::string& data); //!< @return false if offset is beyond EOF
    bool setFloatData(int offset, GDocTokenType type, float data); //!< @return false if offset is beyond EOF

    // AngelScript reference counting
    void AddRef()
    {
        // Increase the reference counter
        refCount++;
    }
    void Release()
    {
        // Decrease ref count and delete if it reaches 0
        if( --refCount == 0 )
            delete this;
    }
private:
    int refCount = 1;
};

void RegisterGenericFileFormat(asIScriptEngine* engine);
