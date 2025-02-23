#include "regex.h"

struct result_t {
    bool status = false;
    Token token;
};
struct quanitifier_result_t {
    int status = false;
    Quantifier data = {};
};
bool is_silent_error = false;

class regex_syntax_error : public std::exception {
private:
public:
    std::string msg;
    std::string rawmsg;
    regex_syntax_error(std::string msg, const char* str, size_t pos) : rawmsg(msg) {
        std::string res = std::string("\n") + str;
        res += "\n";
        res += std::string(pos, ' ');
        res += "^";
        res += "\n";
        res += "Regex Syntax Error [" + std::to_string(pos) + "]: ";
        res += msg;
        res += '\n';
        this->msg = res;
    }
    const char* what() const noexcept override {
        return msg.c_str();
    }
};

[[noreturn]] void throw_syntax_error(std::string msg);
void throw_error_unterminated_group(const char* str);
static result_t match_character_sequence(const char* str, size_t &pos, Token_sequence &sequence);
static result_t match_any(const char* str, size_t &pos, Token_sequence &sequence);
static result_t match_escape(const char* str, size_t &pos, Token_sequence &sequence);
static result_t match_or(const char* str, size_t &pos, Token_sequence &sequence);
static void match_group_content(const char* str, size_t &pos, Token_sequence &sequence, std::list<Token> &list);
static result_t match_group(const char* str, size_t &pos, Token_sequence &sequence);
static result_t match_noncapture_group(const char* str, size_t &pos, Token_sequence &sequence);
static result_t match_anything(const char* str, size_t &pos, Token_sequence &sequence);
static result_t match_strbegin(const char* str, size_t &pos);
static result_t match_strend(const char* str, size_t &pos);
static result_t match_all(const char* str, size_t &pos, Token_sequence &sequence);
static std::string id(const char* str, size_t &pos) {
    std::string res;
    if (!isalpha(str[pos]) && str[pos] != '_')
        return "";
    
    res += str[pos];

    while(isalnum(str[pos]) || str[pos] == '_') {
        res += str[pos];
        pos++;
    }
    return res;
}
static result_t match_character_sequence(const char* str, size_t &pos, Token_sequence &sequence) {
    Token token = {Tokens::CHARACTER_SEQUENCE};
    character_sequence data;
    
    if (str[pos] != '[') 
        return {};
    pos++;

    if (str[pos] == '^') {
        data._not = true;
        pos++;
    } else {
        data._not = false;
    }

    bool escape = false;
    while (str[pos] != ']' || escape) {
        if (str[pos] == '\0') throw_error_unterminated_group(str);

        if (!escape && str[pos] == '\\') {
            escape = true;
        } else if (!escape && isalnum(str[pos]) && str[pos + 1] == '-' && isalnum(str[pos + 2])) {
            data.diapasons.push_front({str[pos], str[pos + 2]});
            pos += 3;
        } else {
            data.characters.push_front(str[pos]);
            pos++;
            escape = false;
        }
    }

    pos++;  // Consume ']'
    token.value = data;
    return { true, token };
}

static result_t match_any(const char* str, size_t &pos, Token_sequence &sequence) {
    if (str[pos] == '.' && (pos == 0 || str[pos - 1] != '\\'))
        return {true, Tokens::ANY};
    return {};
}

static result_t match_escape(const char* str, size_t &pos, Token_sequence &sequence) {
    Token token = {Tokens::ESCAPE};
    if (str[pos] != '\\')
        return {};
    pos++;
    if (str[pos] == 'd' && str[pos + 1] == 'd' && str[pos + 2] == 'd') {
        token.value = '\0';
        pos += 3;
    }  else {
        token.value = str[pos];
        pos++;
    }

    return {true, token};
}
static result_t match_unicode_property(const char* str, size_t &pos, Token_sequence &sequence) {
    Token token = {Tokens::ESCAPE};
    if (str[pos] != '\\')
        return {};
    pos++;
    if (str[pos] == 'p' || str[pos] == 'P') {
        bool negative = str[pos] == 'P';
        pos++;
        if (str[pos] != '{') {
            throw regex_syntax_error("Invalid escape sequence", str, pos);
        }
        pos++;
        std::string name = id(str, pos);
        if (name == "") {
            throw regex_syntax_error("Invalid escape sequence", str, pos);
        }
        if (str[pos] != '}') {
            throw regex_syntax_error("Invalid escape sequence", str, pos);
        }
        pos++;
        token.name = negative ? Tokens::UNICODE_PROPERTY_NEGATIVE : Tokens::UNICODE_PROPERTY;
        token.value = name;
    }
}
static result_t match_hex_character(const char* str, size_t &pos, Token_sequence &sequence) {
    Token token = {Tokens::ESCAPE};
    if (str[pos] != '\\')
        return {};
    pos++;
    if (str[pos] == 'u' || str[pos] == 'x') {
        std::string hex;
        if (str[pos] == 'u') {
            pos++;
            if (str[pos] == '\0' || str[pos + 1] == '\0' || str[pos + 2] == '\0' || str[pos + 3] == '\0') {
                throw regex_syntax_error("Hex expected", str, pos);
            }
            hex = std::string(pos, 4);
            pos += 4;
        } else {
            if (str[pos] == '\0' || str[pos + 1] == '\0') {
                throw regex_syntax_error("Hex expected", str, pos);
            }
            pos++;
            hex = std::string(pos, 2);
            pos += 2
        }
        token.name = Tokens::HEX_CHARACTER;
        token.value = hex;
    }
}
static result_t match_control_character(const char* str, size_t &pos, Token_sequence &sequence) {
    Token token = {Tokens::ESCAPE};
    if (str[pos] != '\\')
        return {};
    pos++;
    if (str[pos] == 'c') {
        pos++;
        if (str[pos] == '\0') {
            throw_syntax_error("Invalid escape sequence");
        }
        token.name = Tokens::CONTOROL_CHARACTER;
        token.value = str[pos++];
    }
}
static result_t match_or(const char* str, size_t &pos, Token_sequence &sequence) {
    Token token = {Tokens::OR};
    std::list<Token> list; 
    if (str[pos] != '|' || str[pos - 1] == '\\')
        return {};
    pos++;
    if (sequence.empty())
        return {};

    // as optimization we do not try to match match_any '|' match_any, instead if we find '|' then use previous token
    list.push_back(sequence.back());
    sequence.pop_back();
    // match once element after '|'
    list.push_back(match_all(str, pos, sequence).token);
    while (str[pos] == '|') {
        pos++;
        list.push_back(match_all(str, pos, sequence).token);
    }
    token.value = list;
    return {true, token};
}

static void match_group_content(const char* str, size_t &pos, size_t group_begin, Token_sequence &sequence, std::list<Token> &list) {
    while (str[pos] != ')') {
        if (str[pos] == '\0')
            throw regex_syntax_error("Unterminated group", str, group_begin);
        list.push_back(match_all(str, pos, sequence).token);
    }
    pos++;
}

static result_t match_group(const char* str, size_t &pos, Token_sequence &sequence) {
    Token token = {Tokens::GROUP};
    std::list<Token> list;
    if (str[pos] != '(')
        return {};
    pos++;
    match_group_content(str, pos, pos - 1, sequence, list);
    token.value = list;
    return {true, token};
}

static result_t match_noncapture_group(const char* str, size_t &pos, Token_sequence &sequence) {
    Token token = {Tokens::NONCAPTURE_GROUP};
    std::list<Token> list;
    if (str[pos] != '(' || str[pos + 1] != '?' || str[pos + 2] != ':')
        return {};
    pos += 3;
    match_group_content(str, pos, pos - 3,sequence, list);
    token.value = list;
    return {true, token};
}
static result_t match_named_capture_group(const char* str, size_t &pos, Token_sequence &sequence) {
    Token token = {Tokens::NAMED_CAPTURE_GROUP};
    std::list<Token> list;
    std::string name;
    size_t begin = pos;
    if (str[pos] != '(' || str[pos + 1] != '<')
        return {};
    pos += 2;
    name = id(str, pos);
    if (name == "" || str[pos] != '>')
        return {};
    pos++;
    match_group_content(str, pos, begin, sequence, list);


    named_capture_group data;
    data.name = name;
    data.value = list;
    token.value = data;
    return {true, token};
}
static result_t match_positive_lookahead(const char* str, size_t &pos, Token_sequence &sequence) {
    Token token = {Tokens::POSITIVE_LOOKAHEAD};
    std::list<Token> list;
    if (str[pos] != '(' || str[pos + 1] != '?' || str[pos + 2] != '=')
        return {};
    pos += 3;
    match_group_content(str, pos, pos - 3, sequence, list);
    token.value = list;
    return {true, token};
}
static result_t match_negative_lookahead(const char* str, size_t &pos, Token_sequence &sequence) {
    Token token = {Tokens::NEGATIVE_LOOKAHEAD};
    std::list<Token> list;
    if (str[pos] != '(' || str[pos + 1] != '?' || str[pos + 2] != '!')
        return {};
    pos += 3;
    match_group_content(str, pos, pos - 3, sequence, list);
    token.value = list;
    return {true, token};
}
static result_t match_positive_lookbehind(const char* str, size_t &pos, Token_sequence &sequence) {
    Token token = {Tokens::POSITIVE_LOOKBEHIND};
    std::list<Token> list;
    if (str[pos] != '(' || str[pos + 1] != '?' || str[pos + 2] != '<' || str[pos + 3] != '=')
        return {};
    pos += 4;
    match_group_content(str, pos, pos - 4, sequence, list);
    token.value = list;
    return {true, token};
}
static result_t match_negative_lookbehind(const char* str, size_t &pos, Token_sequence &sequence) {
    Token token = {Tokens::NEGATIVE_LOOKBEHIND};
    std::list<Token> list;
    if (str[pos] != '(' || str[pos + 1] != '?' || str[pos + 2] != '<' || str[pos + 3] != '!')
        return {};
    pos += 4;
    match_group_content(str, pos, pos - 4, sequence, list);
    token.value = list;
    return {true, token};
}
static result_t match_anything(const char* str, size_t &pos, Token_sequence &sequence) {
    if (str[pos] != '\0') {
        return {true, {Tokens::CHARACTER, str[pos++]}};
    } else {
        return {};
    }
}

static result_t match_strbegin(const char* str, size_t &pos) {
    if (str[pos] != '^')
        return {};
    pos++;
    return {true, Tokens::STR_BEGIN};
}

static result_t match_strend(const char* str, size_t &pos) {
    if (str[pos] != '$')
        return {};
    pos++;
    return {true, Tokens::STR_END};
}
static quanitifier_result_t match_quantifier(const char* str, size_t &pos, Token_sequence &sequence) {
    Quantifier quantifier;
    if (str[pos] == '?' || str[pos] == '+' || str[pos] == '*') {
        quantifier.kind == Quantifier_kind::CHAR;
        if (str[pos + 1] == '?') {
            // handle lazy quantifier
            quantifier.data = {str[pos], 1};
            pos += 2;
        } else {
            quantifier.data = {str[pos], 0};
            pos++;
        }
    } else if (str[pos] == '{' && str[pos - 1] != '\\') {
        pos++;
        size_t first, second = -1;
        if (str[pos] == '}') {
            // it is empty so the last token is considered without match (e.g {0})
            sequence.pop_back();
            pos++;
            return {};
        }
        std::string num_str;
        while(isdigit(str[pos])) {
            num_str += str[pos];
            pos++;
        }
        first = std::stoull(num_str);
        if (str[pos] == '}') {
            quantifier.kind = Quantifier_kind::EXACT;
            quantifier.data = {first, second};
            pos++;
        } else if (str[pos] == ',') {
            pos++;
            if (str[pos] == '}') {
                quantifier.kind = Quantifier_kind::EXACT_OR_MORE;
                quantifier.data = {first, second};
                pos++;
            } else {
                num_str.clear();
                while(isdigit(str[pos])) {
                    num_str += str[pos];
                    pos++;
                };
                if (str[pos] != '}')
                    throw regex_syntax_error("missing end of quantifier", str, pos);
                second = std::stoull(num_str);
                quantifier.kind = Quantifier_kind::FROM_TO;
                quantifier.data = {first, second};
                pos++;
            }
        } else {
            throw regex_syntax_error("missing end of quantifier", str, pos);
        }
    } else {
        return {};
    }
    return {true, quantifier};
}
static result_t match_all(const char* str, size_t &pos, Token_sequence &sequence) {
    result_t res = match_character_sequence(str, pos, sequence);
    if (!res.status) {
        res = match_any(str, pos, sequence);
        if (!res.status) {
            res = match_escape(str, pos, sequence);
            if (!res.status) {
                res = match_or(str, pos, sequence);
                if (!res.status) {
                    res = match_noncapture_group(str, pos, sequence);
                    if (!res.status) {
                        res = match_named_capture_group(str, pos, sequence);
                        if (!res.status) {
                            res = match_positive_lookahead(str, pos, sequence);
                            if (!res.status) {
                                res = match_negative_lookahead(str, pos, sequence);
                                if (!res.status) {
                                    res = match_positive_lookbehind(str, pos, sequence);
                                    if (!res.status) {
                                        res = match_negative_lookbehind(str, pos, sequence);
                                        if (!res.status) {
                                            res = match_group(str, pos, sequence);
                                            if (!res.status) {
                                                res = match_strbegin(str, pos);
                                                if (!res.status) {
                                                    res = match_strend(str, pos);
                                                    if (!res.status) {
                                                        res = match_anything(str, pos, sequence);
                                                        if (!res.status) {
                                                            return {};
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    quanitifier_result_t quantifier = match_quantifier(str, pos, sequence);
    if (quantifier.status == 2) {
        is_silent_error = true;
        return res;
    }
    if (quantifier.status) {
        res.token.qualifier = quantifier.data;
    }
    return res;
}

namespace regex {
    Token_sequence compile(std::string str) {
        return compile(str.c_str());
    }

    Token_sequence compile(const char* str) {
        Token_sequence tokens;
        result_t result;
        size_t pos = 0;
        while ((result = match_all(str, pos, tokens)).status) {
            tokens.push_back(result.token);
            if (is_silent_error) {
                return {};
            }
        }
        return tokens;
    }
}

#ifdef EXPORT_FOR_LIBRARY
    extern "C" {
        Token_sequence __regex_compile(const char* str) {
            return regex::compile(str);
        }
    }
#endif
