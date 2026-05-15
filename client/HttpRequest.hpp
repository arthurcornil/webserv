#ifndef HTTPREQUEST_HPP
#define HTTPREQUEST_HPP

#include "./../Webserv.hpp"
#include "../server/Location.hpp"

class Client;

class HttpRequest{
	public:
		HttpRequest();
		HttpRequest(HttpRequest const &copy);
		~HttpRequest();

		HttpRequest&	operator=(HttpRequest const &assignment);

		void			setMaxBodySize(int maxBodySize);
		void			setReady(bool);
		void			addToBuffer(std::string &chunk, Client &client);
		void			parseBody(std::string &chunk);
		void			clear();

		std::string&						getMethod();
		std::string&						getPath();
		std::string&						getQueryString();
		std::string&						getVersion();
		std::map<std::string, std::string>&	getHeaders();
		std::string&						getBody();
		std::string&						getBuffer();
		bool								endOfHeaders();
		bool								isReady();
		int									getErrorCode();
		size_t								getMaxBodySize();
		size_t								getContentLen();
		int									getBodyTmpFd();
		size_t								getBodyTmpSize();
		std::string							getBodyTmpPath();

	private:
		void			parseHeaders(int endOfHeaders, Client &client);
		void 			parseFirstLine(std::string &firstLine);
		void 			addHeader(std::string key, std::string value);
		void 			addChunkedbody(std::string &chunk);
		void 			addContentlen(std::string &chunk);
		void			addToTmpBodyFile(const char *chunk, size_t len);

		std::string 						_buffer;
		std::string							_path;
		std::string							_queryString;
		std::string							_method;
		std::string							_version;
		std::map<std::string, std::string>	_headers;
		bool								_requestReady;
		bool								_headersReady;
		int									_errorCode;
		size_t								_max_body_size;
		unsigned long						_chunkSize;
		bool								_readingSize;
		size_t								_contentLen;
		int									_bodyTmpfd;
		size_t								_bodyTmpSize;
		std::string							_bodyTmpPath;
};

std::ostream& operator<<( std::ostream &os, HttpRequest &req);

#endif
