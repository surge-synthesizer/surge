/*
 * Surge XT - a free and open source hybrid synthesizer,
 * built by Surge Synth Team
 *
 * Learn more at https://surge-synthesizer.github.io/
 *
 * Copyright 2018-2024, various authors, as described in the GitHub
 * transaction log.
 *
 * Surge XT is released under the GNU General Public Licence v3
 * or later (GPL-3.0-or-later). The license is found in the "LICENSE"
 * file in the root of this repository, or at
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 * Surge was a commercial product from 2004-2018, copyright and ownership
 * held by Claes Johanson at Vember Audio during that period.
 * Claes made Surge open source in September 2018.
 *
 * All source for Surge XT is available at
 * https://github.com/surge-synthesizer/surge
 */

#include "LuaTokeniserSurge.h"
// #include "juce_gui_extra/code_editor/juce_CPlusPlusCodeTokeniserFunctions.h"

namespace Surge
{

struct LuaTokeniserSurgeFunctions
{
    static bool isReservedKeyword(juce::String::CharPointerType token,
                                  const int tokenLength) noexcept
    {
        static const char *const keywords2Char[] = {"if", "or", "in", "do", nullptr};

        static const char *const keywords3Char[] = {"and", "end", "for", "nil", "not", nullptr};

        static const char *const keywords4Char[] = {"then", "true", "else", "goto", nullptr};

        static const char *const keywords5Char[] = {"false", "local", "until",
                                                    "while", "break", nullptr};

        static const char *const keywords6Char[] = {"repeat", "return", "elseif", nullptr};

        static const char *const keywordsOther[] = {
            "function", "@interface", "@end",       "@synthesize", "@dynamic", "@public",
            "@private", "@property",  "@protected", "@class",      nullptr};

        const char *const *k;

        switch (tokenLength)
        {
        case 2:
            k = keywords2Char;
            break;
        case 3:
            k = keywords3Char;
            break;
        case 4:
            k = keywords4Char;
            break;
        case 5:
            k = keywords5Char;
            break;
        case 6:
            k = keywords6Char;
            break;

        default:
            if (tokenLength < 2 || tokenLength > 16)
                return false;

            k = keywordsOther;
            break;
        }

        for (int i = 0; k[i] != nullptr; ++i)
            if (token.compare(juce::CharPointer_ASCII(k[i])) == 0)
                return true;

        return false;
    }

    template <typename Iterator> static int parseIdentifier(Iterator &source) noexcept
    {
        int tokenLength = 0;
        juce::String::CharPointerType::CharType possibleIdentifier[100] = {};
        juce::String::CharPointerType possible(possibleIdentifier);

        while (juce::CppTokeniserFunctions::isIdentifierBody(source.peekNextChar()))
        {
            auto c = source.nextChar();

            if (tokenLength < 20)
                possible.write(c);

            ++tokenLength;
        }

        if (tokenLength > 1 && tokenLength <= 16)
        {
            possible.writeNull();

            if (isReservedKeyword(juce::String::CharPointerType(possibleIdentifier), tokenLength))
                return LuaTokeniserSurge::tokenType_keyword;
        }

        return LuaTokeniserSurge::tokenType_identifier;
    }

    template <typename Iterator> static int readNextToken(Iterator &source)
    {
        source.skipWhitespace();

        auto firstChar = source.peekNextChar();

        switch (firstChar)
        {
        case 0:
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
        case '.':
        {
            auto result = juce::CppTokeniserFunctions::parseNumber(source);

            if (result == LuaTokeniserSurge::tokenType_error)
            {
                source.skip();

                if (firstChar == '.')
                    return LuaTokeniserSurge::tokenType_punctuation;
            }

            return result;
        }

        case ',':
        case ';':
        case ':':
            source.skip();
            return LuaTokeniserSurge::tokenType_punctuation;

        case '(':
        case ')':
        case '{':
        case '}':
        case '[':
        case ']':
            source.skip();
            return LuaTokeniserSurge::tokenType_bracket;

        case '"':
        case '\'':
            juce::CppTokeniserFunctions::skipQuotedString(source);
            return LuaTokeniserSurge::tokenType_string;

        case '+':
            source.skip();
            juce::CppTokeniserFunctions::skipIfNextCharMatches(source, '+', '=');
            return LuaTokeniserSurge::tokenType_operator;

        case '-':
        {
            source.skip();
            auto result = juce::CppTokeniserFunctions::parseNumber(source);

            // comment
            if (source.nextChar() == '-')
            {

                auto currentLine = source.getLine();

                if (source.nextChar() == '[' && source.nextChar() == '[')
                {
                    // multi line comment
                    while (!source.isEOF())
                    {
                        source.skip();
                        if (source.peekNextChar() == ']')
                        {

                            auto startPos = source.getPosition();
                            if (source.nextChar() == ']' && source.nextChar() == ']')
                            {
                                // end of multi line comment
                                break;
                            }
                            else
                            {
                                auto count = source.getPosition() - startPos;
                                for (int i = 0; i < count; i++)
                                {
                                    source.previousChar();
                                }
                            }
                        }
                    }
                }
                else
                {
                    source.previousChar();
                    source.skipToEndOfLine();
                }

                return LuaTokeniserSurge::tokenType_comment;
            }
            else
            {
                source.previousChar();
            }

            if (result == LuaTokeniserSurge::tokenType_error)
            {
                juce::CppTokeniserFunctions::skipIfNextCharMatches(source, '-', '=');
                return LuaTokeniserSurge::tokenType_operator;
            }

            return result;
        }

        case '*':
        case '%':
        case '=':
        case '!':
            source.skip();
            juce::CppTokeniserFunctions::skipIfNextCharMatches(source, '=');
            return LuaTokeniserSurge::tokenType_operator;

        case '?':
        case '~':
        // SURGE ADDITION
        case '#':
        case '/':
            // END SURGE ADDITION
            source.skip();
            return LuaTokeniserSurge::tokenType_operator;

        case '<':
        case '>':
        case '|':
        case '&':
        case '^':
            source.skip();
            juce::CppTokeniserFunctions::skipIfNextCharMatches(source, firstChar);
            juce::CppTokeniserFunctions::skipIfNextCharMatches(source, '=');
            return LuaTokeniserSurge::tokenType_operator;

        default:
            if (juce::CppTokeniserFunctions::isIdentifierStart(firstChar))
                return parseIdentifier(source);

            source.skip();
            break;
        }

        return juce::LuaTokeniser::tokenType_error;
    }
};

LuaTokeniserSurge::LuaTokeniserSurge() {}
LuaTokeniserSurge::~LuaTokeniserSurge() {}

int LuaTokeniserSurge::readNextToken(juce::CodeDocument::Iterator &i)
{
    return LuaTokeniserSurgeFunctions::readNextToken(i);
}

juce::CodeEditorComponent::ColourScheme LuaTokeniserSurge::getDefaultColourScheme()
{
    static const juce::CodeEditorComponent::ColourScheme::TokenType types[] = {
        {"Error", juce::Colour(0xffcc0000)},      {"Comment", juce::Colour(0xff3c3c3c)},
        {"Keyword", juce::Colour(0xff0000cc)},    {"Operator", juce::Colour(0xff225500)},
        {"Identifier", juce::Colour(0xff000000)}, {"Integer", juce::Colour(0xff880000)},
        {"Float", juce::Colour(0xff885500)},      {"String", juce::Colour(0xff990099)},
        {"Bracket", juce::Colour(0xff000055)},    {"Punctuation", juce::Colour(0xff004400)}};

    juce::CodeEditorComponent::ColourScheme cs;

    for (auto &t : types)
        cs.set(t.name, juce::Colour(t.colour));

    return cs;
}

} // namespace Surge
