#ifndef LEXER_H
#define LEXER_H

#include <string>
#include <vector>

#include "Token.h"

typedef const char* i_cant_decide;  // maybe

struct pos {
  int ln, col;
};

struct Token {
  TokenType type;  // I decided!
  std::string content;
  pos loc;
};

using TokenArray = std::vector<Token>;

TokenArray Lex(std::string source);

#endif  // !LEXER_H