#pragma once

enum class TokenType {
  // * Keywords *

  let,
  const_,
  if_,
  else_,
  for_,
  while_,
  bring,
  function,
  collection,
  return_,
  extern_,
  break_,

  // * Types *

  string,
  number,
  float_,
  boolean,
  name,
  // call,

  // * Symbols *

  equ,        // =
  equequ,     // ==
  not_equ,    // !=
  andand_,    // &&
  or_,        // ||
  lparen,     // (
  rparen,     // )
  lbrace,     // {
  rbrace,     // }
  lbracket,   // [
  rbracket,   // ]
  comma,      // ,
  colon,      // :
  semicolon,  // ;
  plus,       // +
  minus,      // -
  multiply,   // *
  divide,     // /
  bang,       // !
  caret,      // ^
  mod,        // %
  gt,         // >
  lt,         // <
  and_,       // &
  dot,        // .
  pipe,       // |
  arrow,      // ->

  // * Special *

  entry,
  identifier,
  unknown,
  EoF
};