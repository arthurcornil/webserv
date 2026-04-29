#ifndef CGI_HPP
#define CGI_HPP
#include <sys/types.h>
#include "../Webserv.hpp"
#include "../server/Location.hpp"

class Client;

class Cgi {
public:
	Cgi();
	void set(std::string filename, Client &client, Location *location);
	Cgi(const Cgi &other);
	Cgi& operator=(const Cgi &other);
	~Cgi();
	void execute();
	int getReadPipe() const;
	int getWritePipe() const;
	size_t getBodyOffset() const;
	pid_t getPid() const;
	void appendToBuffer(char *, ssize_t);
	void advanceBodyOffset(size_t);
	std::string &getBuffer();
	std::string &getBody();

private:
	std::map<std::string, std::string> _env;
	std::vector<std::string> _formattedEnv;
	std::vector<char *> _rawEnv;
	int _readPipe;
	int _writePipe;
	pid_t _pid;
	std::string _buffer;
	std::string _cgiPath;
	std::string _body;
	size_t _bodyOffset;

	void setEnv(std::string filename, Client &client);
	void setFormattedEnv();
};

#endif
