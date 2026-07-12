#include "HandleBring.h"

#include <filesystem>
#include <iostream>
#include <string>

#include <fstream>
#include <sstream>

#include <algorithm>

#include "Lexer.h"
#include "Parser.h"


extern std::unordered_map<std::string, bool> brought; 
extern std::filesystem::path filePth;
extern std::filesystem::path programPath;



NodeArray HandleBring(std::string_view ref)
{
    NodeArray nrr;
    if (filePth.empty() || ref.empty()) {
        std::cerr << "No path specified.\n";
        return nrr;
    }

    std::string modifiedRef(ref);
    std::replace(modifiedRef.begin(), modifiedRef.end(), '.', 
                 static_cast<char>(std::filesystem::path::preferred_separator));

    std::filesystem::path finalPath;

    std::filesystem::path refPath(modifiedRef);
    if (refPath.extension() != ".sbix") {
        refPath += ".sbix";
    }

    std::filesystem::path relPath = filePth.parent_path() / refPath;
    if (std::filesystem::exists(relPath)) {
        finalPath = std::filesystem::canonical(relPath);
    } else {
        std::filesystem::path globalPath = programPath.parent_path() / "library" / refPath;
        if (!std::filesystem::exists(globalPath)) {
            std::cerr << "[\033[31mBRING FAILED\033[0m] File" << ref << "not found";
            return nrr;
        }
        finalPath = std::filesystem::canonical(globalPath);
    }
    std::string pathStr = finalPath.string();
    if (brought[pathStr]) {
        return nrr;
    }
    brought[pathStr] = true;

    std::ifstream file(finalPath);
	if (!file.is_open()) {
		std::cerr << "Failed to read " << relPath << "\n";
		return nrr;
	}
	std::stringstream ss;
	ss << file.rdbuf();
	const std::string source = ss.str();
    TokenArray arr = Lex(source);
    nrr = Parse(arr);

    return nrr;
}