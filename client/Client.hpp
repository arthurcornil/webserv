#ifndef CLIENT_HPP
#define CLIENT_HPP

#include "./../Webserv.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "./../server/Server.hpp"
#include "../cgi/Cgi.hpp"

class Client{
	public:
		Client();
		Client(Client const &copy);
		~Client();

		Client&			operator=(Client const &assignment);

		void 			parseRequest(std::string &chunk);
		bool 			sendResponse();
		bool 			isTimedOut();

		void 			setFd(int fd);
		void			setServer(Server &server);
		Server			&getServer() const;
		bool 			setResponse();
		void			setCgi(Cgi *);
		void			updateLastActivity();
		

		int 			getFd();
		HttpRequest&	getRequest();
		HttpResponse&	getResponse();
		time_t			getLastActivity();
		int				getMaxBodySize();
		Cgi				*getCgi();

		class NotImplementedMethodException: public std::exception {};
	private:
		int				_fd;
		Server*			_server;
		HttpRequest		_request;
		HttpResponse	_response;
		time_t			_lastActivity;
		Cgi				_cgi;
};

std::ostream& operator<<(std::ostream &os, Client &client);

#endif
