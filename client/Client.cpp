#include "Client.hpp"
#include "HttpResponse.hpp"
#include "HttpRequest.hpp"

Client::Client(): _server(NULL), _lastActivity(time(NULL)) {}
Client::Client(Client const &copy): _fd(copy._fd), _server(copy._server), _request(copy._request), _response(copy._response), _lastActivity(copy._lastActivity), _cgi(copy._cgi) {}
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
Server &Client::getServer() const {return *_server;}
time_t	Client::getLastActivity() {return _lastActivity;}
Cgi		*Client::getCgi() {return &_cgi;}

bool Client::isTimedOut() {return ((time(NULL) - _lastActivity) > TIMEOUT);}

void	Client::parseRequest(std::string &chunk) {
	if (!_request.endOfHeaders()) {
		_request.addToBuffer(chunk, *this);
	}
	else if (!_request.isReady() && _request.getMethod() == "POST")
		_request.parseBody(chunk);
}

bool Client::sendResponse() {
    const std::string& header = _response.getRaw();
    const std::string& body   = _response.getBody();
    size_t totalLen            = header.length() + body.length();
    size_t offset              = _response.getSendOffset();

    if (offset >= totalLen)
        return true;
    const char* ptr;
    size_t remaining;
    if (offset < header.length()) {
        ptr       = header.c_str() + offset;
        remaining = header.length() - offset;
    } else {
        ptr       = body.c_str() + (offset - header.length());
        remaining = totalLen - offset;
    }

    ssize_t sent = send(_fd, ptr, remaining, MSG_NOSIGNAL);
    if (sent <= 0) return false;
    _response.setSendOffset(offset + sent);
    return true;
}

bool 	Client::responseIsReady() {
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
