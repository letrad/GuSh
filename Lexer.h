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
	//If a single token (such as a globed thing) which has whitespace in it,split it 
	std::vector<std::string> ExpandToken(const std::string& input);
};
