#include "utils.hpp"


void removeSpacesAround(std::string &str) {
	size_t start = str.find_first_not_of(" \t\r\n");
	if (start == std::string::npos)
		str = "";
	size_t end = str.find_last_not_of(" \t\r\n");
	str = str.substr(start, end - start + 1);
}

bool isSpacesInside(std::string &str) {
	if (str.find_first_of(" \t\r\n") != std::string::npos)
		return true;
	return false;
}

void addSpaces(std::string &line, char c) {
	size_t pos = line.find(c);
	std::string charC(1, c);
	while (pos != std::string::npos)
	{
		line.replace(pos, 1, (" " + charC + " "));
		pos += 3;
		pos = line.find(c, pos);
	}
}

std::list<std::string> tokenizer(std::string &buffer) {
	std::list<std::string> tokens;
	std::string token;

	std::stringstream bufferStream(buffer);
	while (bufferStream >> token)
		tokens.push_back(token);
	return tokens;
}

std::string lexer(std::string &file) {
	std::string buffer;
	std::string line;

	std::ifstream filestream(file.c_str());
	if (!filestream.is_open())
		throw ;
	while (getline(filestream, line))
	{
		size_t commentPos = line.find('#');
		if (commentPos != line.std::string::npos)
			line = line.substr(0, commentPos);
		buffer += line + ' ';
	}
	addSpaces(buffer, '{');
	addSpaces(buffer, '}');
	addSpaces(buffer, ';');
	return buffer;
}

void ipIsValid(std::string ip){
	std::cout << ip << std::endl;
	addSpaces(ip, '.');
	std::cout << ip << std::endl;
	std::stringstream ss(ip);
	int i = 0;
		std::string dot;
	while (i++ < 4)
	{
		int bytes = -1;
		ss >> bytes;
		if (i < 4)
			ss >> dot;
		if (bytes < 0 || bytes > 255 || dot != ".")
		{
			std::cerr << bytes << std::endl;
			throw std::runtime_error("Config: invalid IP address: " + ip);
		}
	}
	if (ss.fail() || !ss.eof())
		throw std::runtime_error("Config: failed to parse IP address: " + ip);
}

std::string sizetostr(int num) {
	std::stringstream ss;
	ss << num;
	return ss.str();
}
long ft_strhexatoul(std::string str) {
	std::stringstream ss;
	unsigned long res;
	ss << std::hex << str;
	ss >> res;
	return res;
}
