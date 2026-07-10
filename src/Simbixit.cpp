
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <sstream>

#include "Lexer.h"
#include "Parser.h"
#include "Semantic.h"
#include "builder.h"

bool hasErrors = false;
unsigned int totalError = 0;

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cout << "Usage: " << argv[0] << " <filename>\n use |   " << argv[0]
              << " --help  | for more info\n";
    return 1;
  }

  std::filesystem::path rel_path(argv[1]);
  std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path path = cwd / rel_path;
  if (!std::filesystem::exists(path)) {
    std::cerr << rel_path << " No such file or directory\n";
    return 1;
  }

  std::ifstream file(path);
  if (!file.is_open()) {
    std::cerr << "Failed to read " << rel_path << "\n";
    return 1;
  }
  std::stringstream ss;
  ss << file.rdbuf();
  const std::string source = ss.str();
  auto start = std::chrono::system_clock::now();
  std::cout << std::format("[info] Compilation started [ {:%H:%M:%S} ]\n",
                           start);
  TokenArray tokens = Lex(source);
  std::cout << std::format("[info] Lexer passed [ {:%H:%M:%S} ]\n",
                           std::chrono::system_clock::now());
  NodeArray nodes = Parse(tokens);
  std::cout << std::format("[info] Parser passed [ {:%H:%M:%S} ]\n",
                           std::chrono::system_clock::now());
  Semantic(nodes);
  auto end = std::chrono::system_clock::now();
  auto elapsed =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  if (hasErrors) {
    std::cout << "Compilation failed. Total errors " << totalError << " \n";
    return 1;
  }
  compile(nodes);
  std::cout << std::format("Complete! time elapsed: {} \n", elapsed);

  return 0;
}

/*
 |---------------| /--------\  |-----------| /--------\  -           -  /------|
|   /| |          |        |             /  |        |  \           /  | |  / |
|        |            /   |        |   \         /   \          | / | |________/
/    |________/    \       /     \______   |/
                |          |                   /     |              \     /
\  |\
                |          |                  /      |               \___/ \ | \
                |          \________/  |----------|  \________/ |-------/ |  \|

Разработка:
        1. Лексер [✓]  05.04.2026 - 07.04.2026
        2. Парсер [⏱] 06.04.2026 - 28.04.2026
        3. Семантический анализатор [⏱] 29.04.2026 - 01.07.2026
        4. Компилятор [X] 01.07.2026
*/

/*Ранний дебаг текст:
  " 565 75 302 69395  7 464 2 8 5457445 45 HelloWorld7I29NU489a let const 5a\n
  if else aboba\n75+4,3 +d+6*3k (y/a) 75.59008 \"Hello world!\"ab\"tezevsk\" -
  -7.5 -5 -ab"
*/
/* Стадия 2
  "let a = 7 * 3 - 7 + -2 + 8 * 3 - (4 - 1) * 5 myFunction(b, 7, \"hello
  world\") if (a) { print(\"hello world!\")} else if (b) { explode() } else {
  none() }"
*/

/* Стадия 3, последняя стадия
 "let a = 8 function helloworld() { let suz = 7 * 3 - 7 + -2 + 8 * 3 - (4 - 1) *
 5 let suz2 = suz * 2 if ( 5 > 2 ) { let a = 10 if ( true ) { let b = 7 } } for
 (u -> 2:9 -> 2) { let hello = 0 } } \n @entry helloworld()"
*/

// Тестового текста больше не будет