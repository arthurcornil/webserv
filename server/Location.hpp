
#ifndef LOCATION_HPP
#define LOCATION_HPP

#include "./../Webserv.hpp"
#include "Server.hpp"

class Server;

class Location{
	public:
		Location();
		Location(Location const &copy);
		~Location();

		Location&	operator=(Location const &assignment);

		void		parser(std::list<std::string> &locationTokens);
		void 		isValid(Server &parentServer);

		std::string& 						getPath();
		std::string& 						getRoot();
		bool& 								isAutoindex();
		bool& 								canGet();
		bool& 								canPost();
		bool& 								canDelete();
		int&								getRedirectionCode();
		std::string& 						getRedirectionUrl();
		std::string&						getUploadDir();
		bool 								getMethod(std::string &method);
		std::vector<std::string>& 			getIndexes();
		long long							getMaxBodySize() const;
		std::map<std::string, std::string>&	getCgi();

	private:
		void setPath();
		void setRoot();
		void setAutoindex();
		void setRedirection();
		void setMethod();
		void setUploadDir();
		void addIndexes();
		void addCgi();
		void setClientMaxBodySize();

		void expected(std::string expected);

		std::list<std::string> 				_locationTokens;
		std::string							_path;
		std::string							_root;
		std::vector<std::string>			_indexes;
		bool								_autoindex;
		int									_returnCode;
		std::string							_returnUrl;
		bool								_allow_get;
		bool								_allow_post;
		bool								_allow_delete;
		std::string							_uploadDir;
		std::map<std::string, std::string>	_cgi;
		long long							_client_max_body_size;
};

std::ostream& operator<<(std::ostream &os, Location &location);

#endif
