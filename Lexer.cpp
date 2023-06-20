#include "Lexer.h"
#include "Commands.h"
#include <ctype.h>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <glob.h>
namespace Lexer {
static bool IsOctDigit(char chr) {
  switch (chr) {
  case '0':
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7':
    return true;
  }
  return false;
}

static bool IsDelimeter(char chr) {
  switch (chr) {
  case '(':
  case ')':
  case '=':
  case '\n':
  case '\t':
  case ' ':
  case '\r':
    return true;
  }
  return false;
}
static  bool IsNameChar(char chr) {
	//I put '-' here so '-f' appears as a single token 
	return isalnum(chr)||chr=='.'||chr=='?'||chr=='*'||chr=='-';
} 
std::string LexItem(const std::string input, size_t &idx) {
  std::string token;
  std::ostringstream stm;
  CLexerExcept except;
  int base = 10, result, digit;
  auto prs_string = [&](char delim, bool allow_var_exp) -> std::string {
    std::string variable = "";
    std::ostringstream stm;
    int substr_len = 0;
    stm << delim;
    while (true) {
      if (idx >= input.size()) {
      str_fail:
        except.message = "Expected a ";
        except.message += delim;
        except.message += "to end the string.";
        except.at = idx;
        throw(except);
      }
      if (input[idx] == delim) {
        idx++;
        break;
      }
      if (allow_var_exp && input[idx] == '$') {
        idx++;
        substr_len = 0;
        variable = "";
        do {
          if (idx >= input.size())
            goto str_fail;
          if (isalnum(input[idx])) {
            if (!substr_len && isdigit(input[idx])) {
              except.message = "Variable names must not start with a digit";
              except.at = idx;
              throw(except);
            }
          } else
			break;
          substr_len++;
          idx++;
        } while (true);
        variable = input.substr(idx - substr_len, substr_len);
        if (!Commands::envVariables.count(variable)) {
          except.message = "Unknown variable \"" + variable + "\".";
          except.at = idx - substr_len;
          throw(except);
        }
        stm << Commands::envVariables[variable];
        continue;
      }
      if (input[idx] == '\\') {
        idx++;
        if (idx >= input.size()) {
        str_fail_esc:
          except.message = "Expected a escape sequence charactor";
          except.at = idx;
          throw(except);
        }
        auto esc_num = [&](int base) -> char {
          int result = 0;
          while (idx <= input.size()) {
            result *= base;
            if (base == 8) {
              if (IsOctDigit(input[idx]))
                result += char(input[idx]) - '0';
              else
                break;
            } else if (base == 16) {
              if (isdigit(input[idx])) {
                result += char(input[idx]) - '0';
              }
              if (isxdigit(input[idx])) {
                result += toupper(char(input[idx])) - 'A' + 10; // 0xA == 10
              } else
                break;
            } else
              abort();
            idx++;
          }
          return result;
        };
          switch (input[idx]) {
          case 'e':
            stm << '\e';
            idx++;
            break;
          case 'b': // Ding ding
            stm << '\b';
            idx++;
            break;
          case 'n':
            stm << '\n';
            idx++;
            break;
          case 'r':
            stm << '\r';
            idx++;
            break;
          case 't':
            stm << '\t';
            idx++;
            break;
          case '\\':
            stm << '\\';
            idx++;
            break;
          case 'x':
          case 'X':
            stm << char(esc_num(16));
            break;
          default:
            if (IsOctDigit(input[idx])) {
              stm << char(esc_num(8));
            } else {
              except.message = "Invalid escape sequence '\\";
              except.message += input[idx];
              except.message += "\'.";
              except.at = idx;
              throw(except);
            }
        }
        continue;
      } else
        stm << input[idx];
      idx++;
    }
    stm << delim;
    return stm.str();
  };
enter:
  // I will have a thing for parsing X base intergers.
  // 0x hex
  // 0b binary
  // 0 octal
  if (input[idx] == '0') {
    idx++;
    if (idx >= input.size()) {
      stm << "0";
      return token;
    }
    switch (input[idx]) {
    case 'b':
      idx++;
      if (idx >= input.size()) {
      bin_fail:
        except.message = "Malformed binary literal(I want 1/0)";
        except.at = idx;
        throw(except);
      }
      base = 2;
      result = 0;
      while (idx < input.size()) {
        result *= base;
        if (IsDelimeter(input[idx]))
          break;
        digit = int(input[idx++]) - '0';
        if (digit >= 2 || digit < 0)
          goto bin_fail;
        result += digit;
        idx++;
      }
      stm << result;
      goto finish;
    case 'x':
    case 'X':
      idx++;
      if (idx >= input.size()) {
      hex_fail:
        except.message = "Malformed Hexidecimal literal(I want 0-9,A-F)";
        except.at = idx;
        throw(except);
      }
      base = 16;
      result = 0;
      while (idx < input.size()) {
        result *= base;
        if (IsDelimeter(input[idx]))
          break;
        if (isdigit(input[idx])) {
          digit = input[idx] - '0';
        } else if (isxdigit(input[idx])) {
          digit = toupper(input[idx]) - 'A' + 10; // 0xA == 10
        } else
          goto hex_fail;
        result += digit;
        idx++;
      }
      stm << result;
      goto finish;
    default:
      if (!IsOctDigit(input[idx])) {
      oct_fail:
        except.message = "Malformed binary literal(I want 0-9,A-F)";
        except.at = idx;
        throw(except);
      }
      base = 8;
      result = 0;
      while (idx < input.size()) {
        result *= base;
        if (IsDelimeter(input[idx]))
          break;
        digit = input[idx] - '0';
        if (digit >= 8 || digit < 0)
          goto oct_fail;
        result += digit;
        idx++;
      }
      stm << result;
      goto finish;
    }
  } else if (isdigit(input[idx])) {
    base = 10;
    result = 0;
    while (idx < input.size()) {
      result *= base;
      if (IsDelimeter(input[idx]))
        break;
      if (isdigit(input[idx])) {
        digit = input[idx] - '0';
      } else {
        except.message = "Malformed decmal literal(I want 0-9)";
        except.at = idx;
        throw(except);
      }
      result += digit;
      idx++;
    }
    stm << result;
    goto finish;
  } else if (isspace(input[idx])) {
    idx++;
    if (idx >= input.size()) {
      stm << "";
      goto finish;
    }
    goto enter;
  } else if (IsNameChar(input[idx])||input[idx]=='$') {
	bool needs_glob=false; 
    while (idx < input.size()) {
      if (IsDelimeter(input[idx]) && !IsNameChar(input[idx]))
        break;
      switch(input[idx]) {
		  case '?':
		  case '*':
		  needs_glob=1;
	  }
      token += input[idx++];
    }
    if(needs_glob) {
		int i;
		//man 3 glob 
		glob_t matches;
		matches.gl_offs=0;
		glob(token.c_str(),GLOB_DOOFFS,NULL,&matches);
		for(i=0;matches.gl_pathv[i];i++) {
			if(i)
				stm << " ";
			stm << matches.gl_pathv[i]; 
		}
		globfree(&matches);
	} else
		stm << token;
	token.clear();
    goto finish;
  } else if (input[idx] == '\'') {
    // In GuSh,I(nrootconauto) will make '' a RAW string untained by expanding
    // $var's in the string
    idx++;
    stm << prs_string('\'', false);
  } else if (input[idx] == '"') {
    // You can use "$var" in here
    idx++;
    stm << prs_string('"', true);
  } else if(input[idx]=='|') {
	  if(++idx<input.size())
		if(input[idx]=='|') {
			idx++;
			stm<<"||";
			goto finish;
		}
	stm<<"|";
	goto finish;
  } else if(input[idx]=='&') {
	  if(++idx<input.size())
		if(input[idx]=='&') {
			idx++;
			stm<<"&&";
			goto finish;
		}
	stm<<"&";
	goto finish;
  } else {
    stm << input[idx++];
    goto finish;
  }
finish:
  token = stm.str();
  if(!token.size()) return token;
  if(token[0]=='$') {
	std::string variable = token.substr(1);
	if (!Commands::envVariables.count(variable)) {
	  except.message = "Unknown variable \"" + variable + "\".";
	  except.at = idx;
	  throw(except);
	}
	return Commands::envVariables[variable];
  }
  if(Commands::aliasMap.count(token))
	  return Commands::aliasMap[token];
  return token;
}
} // namespace Lexer
