
#ifndef SERVER_HPP
#define SERVER_HPP

#include "./../Webserv.hpp"
#include "Location.hpp"

class Location;

class Server {
	public:
		Server();
		Server(Server const &copy);
		~Server();

		Server& operator=(Server const &assignment);

		void						parseConfig(std::list<std::string> &serverTokens);
		void 						setUpListenSocket();
		void						findLocation(std::string path);
		const Location*				matchLocation(const std::string& requestedPath);

		int&						getListenSocket();
		int& 						getPort();
		std::string& 				getHost();
		std::string&				getRoot();
		std::vector<std::string>&	getIndexes();
		size_t& 					getMaxBodySize();
		std::vector<std::string>&	getServerNames();
		std::map<int, std::string>& getErrorPages();
		std::vector<Location>&		getLocations();

		std::list<std::string>		_serverTokens;
	private:
		void setPort();
		void setHost();
		void setRoot();
		void setMaxBodySize();
		void addIndexes();
		void addLocation();
		void addServerNames();
		void addErrorPages();
		void configIsValid();
		void expected(std::string expected);

		int										_port;
		std::string								_host;
		std::vector<Location>					_locations;
		std::vector<std::string>				_server_names;
		size_t									_client_max_body_size;
		std::string								_root;
		std::vector<std::string>				_indexes;
		std::map<int, std::string>				_error_pages;
		int										_listenSocket;
};

std::ostream& operator<<(std::ostream &os, Server &conf);

#endif
