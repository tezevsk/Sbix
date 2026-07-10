#pragma once

#include <string>

#include "Lexer.h"
#include "Token.h"

struct TokenMapping {
  std::string_view name;
  TokenType type;
};

static constexpr TokenMapping keyword[] = {
    {"let", TokenType::let},
    {"const", TokenType::const_},
    {"bring", TokenType::bring},
    {"if", TokenType::if_},
    {"else", TokenType::else_},
    {"while", TokenType::while_},
    {"for", TokenType::for_},
    {"return", TokenType::return_},
    {"function", TokenType::function},
    {"extern", TokenType::extern_},
    {"break", TokenType::break_},
};

#ifndef _lexHelper_noFunctions

Token keywordCheck(std::string word, int l, int c) {
  for (const auto& item : keyword) {
    if (item.name == word) {
      std::string value = "";
      return {item.type, value, {l, c}};
    }
  }
  if (word == "true" || word == "false") {
    return {TokenType::boolean, word, {l, c}};
  }
  return {TokenType::identifier, word, {l, c}};
}

TokenType symb(char c) {
  switch (c) {
    case '=':
      return TokenType::equ;
    case '+':
      return TokenType::plus;
    case '-':
      return TokenType::minus;
    case '*':
      return TokenType::multiply;
    case '/':
      return TokenType::divide;
    case '(':
      return TokenType::lparen;
    case ')':
      return TokenType::rparen;
    case '{':
      return TokenType::lbrace;
    case '}':
      return TokenType::rbrace;
    case '[':
      return TokenType::lbracket;
    case ']':
      return TokenType::rbracket;
    case ',':
      return TokenType::comma;
    case ';':
      return TokenType::semicolon;
    case '!':
      return TokenType::bang;
    case '^':
      return TokenType::caret;
    case '%':
      return TokenType::mod;
    case '>':
      return TokenType::gt;
    case '<':
      return TokenType::lt;
    case '&':
      return TokenType::and_;
    case '.':
      return TokenType::dot;
    case '|':
      return TokenType::pipe;
    case ':':
      return TokenType::colon;
    default:
      return TokenType::unknown;
  }
}
#endif