#ifndef HTTPRESPONSE_HPP
#define HTTPRESPONSE_HPP

#include "./../Webserv.hpp"
#include "../server/Location.hpp"

class Client;

class HttpResponse{
	public:
		HttpResponse();
		HttpResponse(HttpResponse const &copy);
		~HttpResponse();
		HttpResponse &operator=(HttpResponse const &assignment);

		bool set(Client &client);
		bool setDelete(Client &client);
		void setCgiResponse(std::string &output, Server &server);

		std::string getRaw() const;
		void clear();
		std::string getStatusCodeMessage();
		size_t getSendOffset() const;
		void setSendOffset(size_t);
	private:
		int	_statusCode;
		std::map<std::string, std::string> _headers;
		std::string _body;
		std::string _raw;
		Location *_location;
		size_t	_sendOffset;

		void setHeaders(std::string filename = "", Client *client = NULL);
		void setRaw();
		static std::string getMimeType(std::string filename);
		static std::string getIMFFixdate();
		std::string checkFile(std::string &filename, Server &server, std::string &reqPath);
		std::string getResolvedRoute(Client &client);
		void findDefaultFile(std::string &path, std::string &reqUri);
		void generateDefaultErrorPage();
		bool generateAutoindex(std::string &path, std::string reqUri);
		bool shouldTriggerCgi(std::string filename);

		class NoDefaultContentException : public std::exception {};
};

#endif
