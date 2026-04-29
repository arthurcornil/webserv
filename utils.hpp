#ifndef UTILS_HPP
#define UTILS_HPP

#include "Webserv.hpp"

void        			removeSpacesAround(std::string &str);
bool       				isSpacesInside(std::string &str);
void       				addSpaces(std::string &line, char c);
void        			ipIsValid(std::string ip);
std::string 			lexer(std::string &file);
std::list<std::string>	tokenizer(std::string &buffer);
void					ipIsValid(std::string ip);
std::string				sizetostr(int num);
long					ft_strhexatoul(std::string str);

#endif
