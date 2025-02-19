#include "regex.cpp"
#include <fstream>
// Function to print Token_sequence to file
void print_tokens_to_file(const Token_sequence& tokens, const std::string& filename) {
    std::ofstream file(filename);
    if (!file) {
        std::cerr << "Error opening file: " << filename << '\n';
        return;
    }
    
    for (const auto& token : tokens) {
        file << "Token: " << token_to_string(token.name) << "\n";
        
        if (token.name == Tokens::CHARACTER_SEQUENCE) {
            try {
                const auto& seq = std::any_cast<const character_sequence&>(token.value);
                file << "  Negated: " << (seq._not ? "Yes" : "No") << "\n  Characters: ";
                for (char c : seq.characters) file << c << ' ';
                file << "\n  Ranges: ";
                for (const auto& [start, end] : seq.diapasons) file << '[' << start << "-" << end << "] ";
                file << "\n";
            } catch (const std::bad_any_cast&) {
                file << "  (Invalid CHARACTER_SEQUENCE data)\n";
            }
        } else if (token.name == Tokens::CHARACTER) {
            file << "value: " << std::any_cast<char>(token.value) << '\n';
        } else if (token.name == Tokens::OR) {
            auto data = std::any_cast<std::list<Token>>(token.value);
            file << "size: " << data.size() << "\n";
        }
    }
    
    file.close();
}

int main() {
    const char* reg = "[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}a|B|([a-zA-Z0-9])(<_0f>abc--023)$";
    auto result = regex::compile(reg);
    std::cout << "printing to file\n"; 
    print_tokens_to_file(result, "result");
}