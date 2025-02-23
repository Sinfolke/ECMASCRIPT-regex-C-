#pragma once

#include <string>
#include <forward_list>
#include <list>
#include <any>
#include <iostream>
#include <stdexcept>

enum class Tokens {
    NONE, CHARACTER_SEQUENCE, ANY, ESCAPE, UNICODE_PROPERTY, UNICODE_PROPERTY_NEGATIVE, HEX_CHARACTER, CONTOROL_CHARACTER, OR, GROUP, NONCAPTURE_GROUP, NAMED_CAPTURE_GROUP, 
    POSITIVE_LOOKAHEAD, NEGATIVE_LOOKAHEAD, POSITIVE_LOOKBEHIND, NEGATIVE_LOOKBEHIND, CHARACTER, STR_BEGIN, STR_END
};
enum class Quantifier_kind {
    CHAR, EXACT, EXACT_OR_MORE, FROM_TO
};
struct character_sequence;
struct named_capture_group;
struct Quantifier;
struct Token;

struct character_sequence {
    bool _not;
    std::forward_list<char> characters;
    std::forward_list<std::pair<char, char>> diapasons;
};
struct named_capture_group {
    std::string name;
    std::list<Token> value;
};
struct Quantifier {
    Quantifier_kind kind;
    std::pair<size_t, size_t> data;
};
struct Token {
    Tokens name = Tokens::NONE;
    std::any value;
    Quantifier qualifier;
};

using Token_sequence = std::list<Token>;

const char* token_to_string(Tokens token) {
    switch (token) {
        case Tokens::NONE: return "NONE";
        case Tokens::CHARACTER_SEQUENCE: return "CHARACTER_SEQUENCE";
        case Tokens::ANY: return "ANY";
        case Tokens::ESCAPE: return "ESCAPE";
        case Tokens::OR: return "OR";
        case Tokens::GROUP: return "GROUP";
        case Tokens::NONCAPTURE_GROUP: return "NONCAPTURE_GROUP";
        case Tokens::NAMED_CAPTURE_GROUP: return "NAMED_CAPTURE_GROUP";
        case Tokens::CHARACTER: return "CHARACTER";
        case Tokens::STR_BEGIN: return "STR_BEGIN";
        case Tokens::STR_END: return "STR_END";
        default: return "UNKNOWN";
    }
}
