
#include "Server.hpp"

Server::Server(): _client_max_body_size(static_cast<size_t>(MAX_BODY_SIZE_GO) * 1024 * 1024 * 1024), _listenSocket(-1) {}
Server::Server(Server const &copy) {
	*this = copy;
}
Server::~Server() {}
Server& Server::operator=(Server const &assignment) {
	if (this != &assignment)
	{
		_port = assignment._port;
		_host = assignment._host;
		_locations = assignment._locations;
		_server_names = assignment._server_names;
		_client_max_body_size = assignment._client_max_body_size;
		_root = assignment._root;
		_indexes = assignment._indexes;
		_error_pages = assignment._error_pages;
		_listenSocket = assignment._listenSocket;
	}
	return *this;
}

int&	 Server::getListenSocket() {return _listenSocket;}
int& Server::getPort() {return _port;}
std::string& Server::getHost() {return _host;}
std::string& Server::getRoot() {return _root;}
std::vector<std::string>& Server::getIndexes() {return _indexes;}
size_t& Server::getMaxBodySize() {return _client_max_body_size;}
std::vector<std::string>& Server::getServerNames() {return _server_names;}
std::map<int, std::string>& Server::getErrorPages() {return _error_pages;}
std::vector<Location>& Server::getLocations() {return _locations;}

void Server::setHost() {
	_host = _serverTokens.front();
	_serverTokens.pop_front();
}

void Server::setPort() {
	std::string port;
	std::string token = _serverTokens.front();
	size_t separator = token.find(':');
	if (separator != std::string::npos)
	{
		_host = token.substr(0, separator);
		port = (token.substr(static_cast<int>(separator + 1)));
	}
	else if (token.find('.') != std::string::npos)
		_host = token;
	else
		port = token;
	std::stringstream ss(port);
	ss >> _port;
	if (ss.fail() || !ss.eof() || _port < 1 || _port > 65535)
		throw std::runtime_error("Config: invalid port value");
	_serverTokens.pop_front();
}

void Server::setRoot() {
	_root = _serverTokens.front();
	_serverTokens.pop_front();
}

void Server::setMaxBodySize() {
    _client_max_body_size = 0;
    std::string token = _serverTokens.front();
    char unit = token[token.length() - 1];
    std::string value;

    if (std::isdigit(unit)) {
        value = token;
        unit = 'B';
    } else {
        value = token.substr(0, token.length() - 1);
    }

    std::stringstream ss(value);
    ss >> _client_max_body_size;
    if (ss.fail() || !ss.eof())
        throw std::runtime_error("Config: invalid client_max_body_size value");
    if (_client_max_body_size <= 0)
        throw std::runtime_error("Config: invalid client_max_body_size value");

    if (unit == 'K' || unit == 'k')
    {
        if (_client_max_body_size > MAX_BODY_SIZE_GO * 1024 * 1024)
            _client_max_body_size = MAX_BODY_SIZE_GO * 1024 * 1024;
        _client_max_body_size *= 1024;
    }
    else if (unit == 'M' || unit == 'm')
    {
        if (_client_max_body_size > MAX_BODY_SIZE_GO * 1024)
            _client_max_body_size = MAX_BODY_SIZE_GO * 1024;
        _client_max_body_size *= 1024 * 1024;
    }
    else if (unit == 'G' || unit == 'g')
    {
        if (_client_max_body_size > MAX_BODY_SIZE_GO)
            _client_max_body_size = MAX_BODY_SIZE_GO;
        _client_max_body_size *= 1024 * 1024 * 1024;
    }
    else if (unit == 'B' || unit == 'b')
    {
        if (_client_max_body_size > (long long)MAX_BODY_SIZE_GO * 1024 * 1024 * 1024)
            _client_max_body_size = (long long)MAX_BODY_SIZE_GO * 1024 * 1024 * 1024;
    }
    else
        throw std::runtime_error("Config: invalid client_max_body_size unit");
    _serverTokens.pop_front();
}

void Server::addLocation() {
	std::list<std::string>	locationTokens;

	locationTokens.push_back(_serverTokens.front());
	_serverTokens.pop_front();
	if (!_serverTokens.empty() && _serverTokens.front() == "{")
	{
		int braceCount = 0;
		while (!_serverTokens.empty())
		{
			std::string token = _serverTokens.front();
			_serverTokens.pop_front();
			if (token == "{")
				braceCount++;
			if (token == "}")
				braceCount--;
			locationTokens.push_back(token);
			if (braceCount == 0)
				break;
		}
	}
	Location newLocation;
	newLocation.parser(locationTokens);
	_locations.push_back(newLocation);
}

void Server::addServerNames() {
	while (!_serverTokens.empty() && _serverTokens.front() != ";")
	{
		_server_names.push_back(_serverTokens.front());
		_serverTokens.pop_front();
	}
	size_t i = 0;
	while (i < _server_names.size())
		if (_server_names[i++].empty())
			throw std::runtime_error("Config: empty server_name");
}

void Server::addIndexes() {
	while (!_serverTokens.empty() && _serverTokens.front() != ";")
	{
		_indexes.push_back(_serverTokens.front());
		_serverTokens.pop_front();
	}
}

void Server::addErrorPages() {
	std::vector<std::string> codes;
	std::string path;
	size_t i = 0;

	while (_serverTokens.front() != ";")
	{
		codes.push_back(_serverTokens.front());
		_serverTokens.pop_front();
	}
	path = codes.back();
	codes.pop_back();
	while (i < codes.size())
	{
		int code;
		std::stringstream ss(codes[i++]);
		ss >> code;
		if (code < 400 || code > 599 || ss.fail() || !ss.eof())
			throw std::runtime_error("Config: invalid error page code :" + codes[i - 1]);
		_error_pages[code] = path;
	}
}

void Server::expected(std::string expected) {
	if (_serverTokens.empty() || _serverTokens.front() != expected)
		throw std::runtime_error("Config: expected '" + expected + "' but got '" + _serverTokens.front() + "'");
	_serverTokens.pop_front();
}

void Server::configIsValid() {
	if (!_port) _port = 80;
	if (_host.empty()) _host = "0.0.0.0";
	else
		ipIsValid(_host);
	if (_root.empty()) throw std::runtime_error("Config: server has no root");
	if (_indexes.empty()) _indexes.push_back("index.html");
	if (_locations.empty()) throw std::runtime_error("Config: server has no location");
	size_t i = 0;
	while (i < _locations.size())
		_locations[i++].isValid(*this);
}

void Server::setUpListenSocket() {
	int socketFd = socket(AF_INET, SOCK_STREAM, 0);
	if (socketFd == -1)
		throw std::runtime_error("socket() failed: " + std::string(strerror(errno)));
	int sockopt = 1;
	setsockopt(socketFd, SOL_SOCKET, SO_REUSEADDR, &sockopt, sizeof(sockopt));
	setsockopt(socketFd, SOL_SOCKET, SO_REUSEPORT, &sockopt, sizeof(sockopt));
	struct sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(getPort());
	serverAddr.sin_addr.s_addr = inet_addr(getHost().c_str());
	if (bind(socketFd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1)
		throw std::runtime_error("bind() failed on " + _host + ": " + std::string(strerror(errno)));
	if (listen(socketFd, 1024) == -1)
		throw std::runtime_error("listen() failed: " + std::string(strerror(errno)));
	_listenSocket = socketFd;
	std::cout << "Server listening on " << getHost() << ": " << getPort() << std::endl;
}

void Server::parseConfig(std::list<std::string> &serverTokens) {
	_serverTokens = serverTokens;
	_serverTokens.pop_front();
	while (!_serverTokens.empty() && _serverTokens.front() != "}")
	{
		std::string directive = _serverTokens.front();
		_serverTokens.pop_front();
		if (!_serverTokens.empty() && _serverTokens.front() != ";")
		{
			if (directive == "listen") setPort();
			else if (directive == "host") setHost();
			else if (directive == "server_name") addServerNames();
			else if (directive == "root") setRoot();
			else if (directive == "index") addIndexes();
			else if (directive == "client_max_body_size") setMaxBodySize();
			else if (directive == "error_page") addErrorPages();
			else if (directive == "location") addLocation();
			else
				throw std::runtime_error("Config: unknown directive: " + directive);
		}
		else
			throw std::runtime_error("Config:  empty directive: " + directive);
		if (directive != "location")
			expected(";");
	}
	expected("}");
	configIsValid();
}

const Location* Server::matchLocation(const std::string& reqPath) {
    std::vector<Location> &locations = this->getLocations();
	Location *matched = NULL;

    for (size_t i = 0; i < locations.size(); i ++) {
        const std::string currLocPath = locations[i].getPath();
        if (reqPath.find(currLocPath) == 0) {
            if (!(reqPath.length() == currLocPath.length()
                || reqPath[currLocPath.length()] == '/'
                || currLocPath[currLocPath.length() - 1] == '/'))
                    continue ;
            else if (!matched || currLocPath.length() > matched->getPath().length())
                matched = &(locations[i]);
        }
    }
	return matched;
}

std::ostream& operator<<(std::ostream &os, Server &conf) {
	os << BOLD_PINK "========= NEW SERVER CONFIG =========" RESET << std::endl;
	os << "_port = " << conf.getPort() << std::endl;
	os << "_host = " << conf.getHost() << std::endl;
	if (!conf.getRoot().empty())
		os << "_root = " << conf.getRoot() << std::endl;
	os << "_client_max_body_size = " << conf.getMaxBodySize() << std::endl;
	os << "_listenSocket = " << conf.getListenSocket() << std::endl;
	if (!conf.getServerNames().empty())
	{
		os << "_server_names = " << std::endl;
		std::vector<std::string>::iterator it = conf.getServerNames().begin();
		while (it != conf.getServerNames().end())
		{
			os << "-> " << *it << std::endl;
			it++;
		}
	}
	if (!conf.getIndexes().empty())
	{
		os << "_indexes = " << std::endl;
		std::vector<std::string>::iterator it = conf.getIndexes().begin();
		while (it != conf.getIndexes().end())
		{
			os << "-> " << *it << std::endl;
			it++;
		}
	}
	if (!conf.getErrorPages().empty())
	{
		os << "_error_pages = " << std::endl;
		std::map<int, std::string>::iterator ite = conf.getErrorPages().begin();
		while (ite != conf.getErrorPages().end())
		{
			os << "->key: " << ite->first << " ; value: " << ite->second << std::endl;
			ite++;
		}
	}
	if (!conf.getLocations().empty())
	{
		std::vector<Location>::iterator iter = conf.getLocations().begin();
		while (iter != conf.getLocations().end())
		{
			os << "-> " << *iter << std::endl;
			iter++;
		}	
	}
	os << "===end of this server===" << std::endl;
	return os;
}
