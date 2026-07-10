#pragma once

#include <format>
#include <iostream>
#include <string_view>

#include "Lexer.h"

extern bool hasErrors;
extern int totalError;

template <typename... Args>
void errorf(pos loc, std::string code, std::format_string<Args...> format,
            Args&&... args) {
  std::cerr << "[\x1b[31mSBX-" << code << "\x1b[0m] ";
  std::cerr << std::format(format, std::forward<Args>(args)...);
  std::cerr << std::format(" ({}:{})\n", loc.ln, loc.col);
  hasErrors = true;
  totalError++;
}
