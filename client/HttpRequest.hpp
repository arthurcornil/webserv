#ifndef HTTPREQUEST_HPP
#define HTTPREQUEST_HPP

#include "./../Webserv.hpp"


class HttpRequest{
	public:
		HttpRequest();
		HttpRequest(HttpRequest const &copy);
		~HttpRequest();

		HttpRequest&	operator=(HttpRequest const &assignment);

		void			setMaxBodySize(int maxBodySize);
		void			setReady(bool);
		void			addToBuffer(std::string &chunk);
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
	private:
		void			parseHeaders(int endOfHeaders);
		void 			parseFirstLine(std::string &firstLine);
		void 			addHeader(std::string key, std::string value);
		void 			addChunkedbody(std::string &chunk);
		void 			addContentlen(std::string &chunk);

		std::string 						_buffer;
		std::string							_path;
		std::string							_queryString;
		std::string							_method;
		std::string							_version;
		std::map<std::string, std::string>	_headers;
		std::string							_body;
		bool								_requestReady;
		bool								_headersReady;
		int									_errorCode;
		size_t								_max_body_size;
		unsigned long						_chunkSize;
		bool								_readingSize;
		size_t								_contentLen;
};

std::ostream& operator<<( std::ostream &os, HttpRequest &req);

#endif
