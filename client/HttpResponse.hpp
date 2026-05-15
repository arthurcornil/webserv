#ifndef HTTPRESPONSE_HPP
#define HTTPRESPONSE_HPP

#include "./../Webserv.hpp"
#include "../server/Location.hpp"
#include "Cgi.hpp"
#include <sys/stat.h>
#include <dirent.h>

class Client;

class HttpResponse{
	public:
		HttpResponse();
		HttpResponse(HttpResponse const &copy);
		~HttpResponse();
		HttpResponse &operator=(HttpResponse const &assignment);

		bool set(Client &client);
		bool setDelete(Client &client);
		void setCgiResponse(std::string &output, Client &client);

		std::string &getRaw();
		void clear();
		std::string getStatusCodeMessage();
		size_t getSendOffset() const;
		void setSendOffset(size_t);
		int getStatusCode() const;
		const std::string& getBody() const;
		std::map<std::string, std::string> &getHeaders();
		size_t buildCgiHeaders(std::string &buffer, Client &client);
		
	private:
		int	_statusCode;
		std::map<std::string, std::string> _headers;
		std::string _body;
		std::string _raw;
		Location *_location;
		size_t	_sendOffset;

		bool handleUpload(Client &);
		void setHeaders(std::string filename = "", Client *client = NULL);
		void setRaw();
		static std::string getMimeType(std::string filename);
		static std::string getIMFFixdate();
		std::string checkFile(std::string &filename, Server &server, std::string &reqPath);
		std::string getResolvedRoute(Client &client);
		std::string getRawRoute(Client &client);
		void findDefaultFile(std::string &path, std::string &reqUri);
		void generateDefaultErrorPage();
		bool generateAutoindex(std::string &path, std::string reqUri);
		bool shouldTriggerCgi(std::string filename);

		class NoDefaultContentException : public std::exception {};
};

#endif
