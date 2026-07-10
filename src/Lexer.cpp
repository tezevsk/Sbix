#include "Lexer.h"

#include "Token.h"
#include "lex_helper.h"

int line = 1, column = 1;

TokenArray Lex(std::string source) {
  TokenArray temp;
  int i = 0;
  while (i < source.length()) {
    if (isspace(source[i])) {
      if (source[i] == '\n') {
        line++;
        column = 1;
      }
      i++;
      continue;
    }
    if (source[i] == '/' && i + 1 < source.length() && source[i + 1] == '/') {
      i += 2;
      while (source[i] != '\n' && i < source.length()) {
        i++;
      }
      column = 0;
      line++;
      continue;
    }
    if (source[i] == '/' && i + 1 < source.length() && source[i + 1] == '*') {
      i += 2;
      column += 2;
      while (i + 1 < source.length() &&
             !(source[i] == '*' && source[i + 1] == '/')) {
        i++;
        column++;
        if (source[i] == '\n') {
          column = 0;
          line++;
        }
      }
      if (i + 1 < source.length()) i += 2;
      continue;
    }
    if (isdigit(source[i]) || (source[i] == '-' && isdigit(source[i + 1])) ||
        (source[i] == '.' && isdigit(source[i + 1]))) {
      bool isFloat = (source[i] == '.' && isdigit(source[i + 1]));
      std::string tempDigit = "";
      tempDigit += source[i];
      i++;
      column++;
      while (isdigit(source[i])) {
        tempDigit += source[i];
        i++;
        column++;
      }
      if (source[i] == '.' && isdigit(source[i + 1])) {
        isFloat = true;
        tempDigit += '.';
        i++;
        column++;
        while (isdigit(source[i])) {
          tempDigit += source[i];
          i++;
          column++;
        }
      }
      temp.push_back({isFloat ? TokenType::float_ : TokenType::number,
                      tempDigit,
                      {line, column}});
      continue;
    }
    if (isalpha(source[i])) {
      std::string tempWord = "";
      tempWord += source[i];
      i++;
      column++;
      while (isalpha(source[i]) || isdigit(source[i]) || source[i] == '_') {
        tempWord += source[i];
        i++;
        column++;
      }
      temp.push_back(keywordCheck(tempWord, line, column));
      continue;
    }
    if (symb(source[i]) != TokenType::unknown) {
      if (source[i] == '-' && (i < source.length() && source[i + 1] == '>')) {
        temp.push_back({TokenType::arrow, "", {line, column}});
        i += 2;
        column += 2;
        continue;
      }
      if (source[i] == '=' && (i < source.length() && source[i + 1] == '=')) {
        temp.push_back({TokenType::equequ, "", {line, column}});
        i += 2;
        column += 2;
        continue;
      }
      temp.push_back({symb(source[i]), "", {line, column}});
      i++;
      column++;
      continue;
    }
    if (source[i] == '@') {
      i++;
      std::string tempWord = "";
      tempWord += source[i];
      i++;
      column++;
      while (isalpha(source[i]) || isdigit(source[i]) || source[i] == '_') {
        tempWord += source[i];
        i++;
        column++;
      }
      if (tempWord == "entry") {
        temp.push_back({TokenType::entry, "", {line, column}});
      }
      continue;
    }
    if (source[i] == '"') {
      i++;
      column++;
      std::string tempString = "";
      tempString += source[i];
      i++;
      column++;
      while (source[i] != '"' && i < source.length()) {
        tempString += source[i];
        i++;
        column++;
      }
      i++;
      column++;
      temp.push_back({TokenType::string, tempString, {line, column}});
      continue;
    } else {
      i++;
      column++;
      continue;
    }
  }
  temp.push_back({TokenType::EoF, "", {line, column}});
  return temp;
}
