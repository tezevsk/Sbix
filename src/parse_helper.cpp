#include "parse_helper.h"
#define _lexHelper_noFunctions
#include "lex_helper.h"

std::string getSpecial(TokenType t) {
  switch (t) {
    case TokenType::equ:
      return "=";
    case TokenType::plus:
      return "+";
    case TokenType::minus:
      return "-";
    case TokenType::multiply:
      return "*";
    case TokenType::divide:
      return "/";
    case TokenType::lparen:
      return "(";
    case TokenType::rparen:
      return ")";
    case TokenType::lbrace:
      return "{";
    case TokenType::rbrace:
      return "}";
    case TokenType::lbracket:
      return "[";
    case TokenType::rbracket:
      return "]";
    case TokenType::comma:
      return ",";
    case TokenType::semicolon:
      return ";";
    case TokenType::bang:
      return "!";
    case TokenType::caret:
      return "^";
    case TokenType::mod:
      return "%";
    case TokenType::gt:
      return ">";
    case TokenType::lt:
      return "<";
    case TokenType::and_:
      return "&";
    case TokenType::dot:
      return ".";
    case TokenType::pipe:
      return "|";
    default:
      return "#";
  }
}

std::string getByTokenType(TokenType t) {
  if (t == TokenType::equ || t == TokenType::plus || t == TokenType::minus ||
      t == TokenType::multiply || t == TokenType::divide ||
      t == TokenType::lparen || t == TokenType::rparen ||
      t == TokenType::lbrace || t == TokenType::rbrace ||
      t == TokenType::lbracket || t == TokenType::rbracket ||
      t == TokenType::comma || t == TokenType::semicolon ||
      t == TokenType::bang || t == TokenType::caret || t == TokenType::mod ||
      t == TokenType::gt || t == TokenType::lt || t == TokenType::and_ ||
      t == TokenType::dot || t == TokenType::pipe)
    return getSpecial(t);
  for (const auto& item : keyword) {
    if (item.type == t) {
      return std::string(item.name);
    }
  }
  return "";
}