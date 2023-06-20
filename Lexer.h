#pragma once
#include <vector>
#include <string>
namespace Lexer {
	class CLexerExcept {
	public:
	  std::string message;
	  int at;
	};
	std::string LexItem(const std::string input, size_t &idx,bool is_arith=false);
};
