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
	pid_t getPid() const;
	void appendToBuffer(char *, ssize_t);
	std::string &getBuffer();
	void setBodyTmpPath(const std::string &path);
	int getBodyTmpFd() const;
	std::string getBodyTmpPath() const;
	void setBodyTmpFd(int fd);
	bool getHeadersSent() const;
	void setHeadersSent(bool val);
	std::string &getWriteBuffer();
	std::string &getClientBuffer();
	bool getCgiPipeEnded() const;
	void setCgiPipeEnded(bool val);
	bool getBodyTmpDone() const;
	void setBodyTmpDone(bool val);

private:
	std::map<std::string, std::string>	_env;
	std::vector<std::string>			_formattedEnv;
	std::vector<char *>					_rawEnv;
	int									_readPipe;
	int									_writePipe;
	pid_t								_pid;
	std::string							_buffer;
	std::string							_cgiPath;
	std::string							_bodyTmpPath;
	int									_bodyTmpFd;
	bool								_headersSent;
	std::string							_writeBuffer;
	std::string							_clientBuffer;
	bool								_cgiPipeEnded;
	bool								_bodyTmpDone;
	void setEnv(std::string filename, Client &client);
	void setFormattedEnv();
};

#endif
