#include "Client.hpp"

Client::Client(): _server(NULL), _lastActivity(time(NULL)) {}
Client::Client(Client const &copy): _fd(copy._fd), _server(copy._server), _request(copy._request), _response(copy._response), _lastActivity(copy._lastActivity) {}
Client::~Client() {}
Client& Client::operator=(Client const &assignment) {
	if (this != &assignment)
	{
		_fd = assignment._fd;
		_request = assignment._request;
		_response = assignment._response;
		_server = assignment._server;
		_lastActivity = assignment._lastActivity;
		_cgi = assignment._cgi;
	}
	return *this;
}

void	Client::updateLastActivity() {_lastActivity = time(NULL);}
void	Client::setFd(int fd) {_fd = fd;}
void	Client::setServer(Server &server) {_server = &server;}

int Client::getFd() {return _fd;}
HttpRequest& Client::getRequest() {return _request;}
HttpResponse& Client::getResponse() {return _response;}
Server &Client::getServer() const {return *(this->_server);}
time_t	Client::getLastActivity() {return _lastActivity;}
Cgi		*Client::getCgi() {return &_cgi;}

bool Client::isTimedOut() {return ((time(NULL) - _lastActivity) > TIMEOUT);}

void	Client::parseRequest(std::string &chunk) {

	_request.setMaxBodySize(_server->getMaxBodySize());
	if (!_request.endOfHeaders())
		_request.addToBuffer(chunk);
	else if (!_request.isReady() && _request.getMethod() == "POST")
		_request.parseBody(chunk);
}

bool Client::sendResponse() {
	const std::string& raw = _response.getRaw();
	size_t remaining = raw.length() - _response.getSendOffset();

	ssize_t sentBytes = send(_fd, raw.c_str() + _response.getSendOffset(), remaining, MSG_NOSIGNAL);
	if (sentBytes <= 0) {
		std::cout << "sentBytes: " << sentBytes << std::endl;
		return false;
	}
	_response.setSendOffset(_response.getSendOffset() + sentBytes);
	if (_response.getSendOffset() >= raw.length()) {
		this->getRequest().clear();
		this->getResponse().clear();
	}
	return true;
}

bool 	Client::setResponse() {
	if (_request.getMethod() == "DELETE")
		return _response.setDelete(*this);
	return _response.set(*this);
}

std::ostream& operator<<(std::ostream &os, Client &client) {
	os << BOLD_CYAN "*** NEW CLIENT ***" RESET << std::endl;
	os << "fd = " << client.getFd() << std::endl;
	os << "server fd = " << client.getServer().getListenSocket() << std::endl;
	os << "request = " << client.getRequest() << std::endl;
	os << "***" << std::endl;
	return os;
}
