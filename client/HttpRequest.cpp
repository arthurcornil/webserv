
#include "HttpRequest.hpp"
#include "Client.hpp"

HttpRequest::HttpRequest(): _requestReady(false), _headersReady(false), _errorCode(0), _chunkSize(0), _readingSize(true), _contentLen(0), _bodyTmpfd(-1), _bodyTmpSize(0) {}
HttpRequest::HttpRequest(HttpRequest const &copy) {
	*this = copy;
}
HttpRequest::~HttpRequest()  {clear();}
HttpRequest& HttpRequest::operator=(HttpRequest const &assignment) {
	if (this != &assignment)
	{
		_buffer = assignment._buffer;
		_path = assignment._path;
		_method = assignment._method;
		_version = assignment._version;
		_headers = assignment._headers;
		_requestReady = assignment._requestReady;
		_headersReady = assignment._headersReady;
		_max_body_size = assignment._max_body_size;
		_readingSize = assignment._readingSize;
		_chunkSize = assignment._chunkSize;
		_contentLen = assignment._contentLen;
		_errorCode = assignment._errorCode;
		_bodyTmpfd = assignment._bodyTmpfd;
		_bodyTmpSize = assignment._bodyTmpSize;
		_bodyTmpPath = assignment._bodyTmpPath;
	}
	return *this;
}

std::string& HttpRequest::getMethod() {return _method;}
std::string& HttpRequest::getPath() {return _path;}
std::string& HttpRequest::getQueryString() {return _queryString;}
std::string& HttpRequest::getVersion() {return _version;}
std::string& HttpRequest::getBuffer() {return _buffer;}
int HttpRequest::getErrorCode() {return _errorCode;}
size_t HttpRequest::getContentLen() {return _contentLen;}
std::map<std::string, std::string>& HttpRequest::getHeaders() {return _headers;}
bool HttpRequest::endOfHeaders() {return _headersReady;}
bool HttpRequest::isReady() {return _requestReady;}
size_t	HttpRequest::getMaxBodySize() {return _max_body_size;}
std::string HttpRequest::getBodyTmpPath() {return _bodyTmpPath;}
size_t HttpRequest::getBodyTmpSize() {return _bodyTmpSize;}
int HttpRequest::getBodyTmpFd() {return _bodyTmpfd;}

void	HttpRequest::setMaxBodySize(int maxBodySize) {_max_body_size = maxBodySize;}
void	HttpRequest::setReady(bool ready) {_requestReady = ready;}

void HttpRequest::addToTmpBodyFile(const char *chunk, size_t len) {
    if (_bodyTmpfd == -1)
	{
        static size_t _fileCounter = 0;
        _bodyTmpPath = "/tmp/webserv_" + toString(getpid()) + "_" + toString(_fileCounter++);
        _bodyTmpfd = open(_bodyTmpPath.c_str(), O_CREAT | O_RDWR | O_TRUNC | O_CLOEXEC, 0600);
        if (_bodyTmpfd == -1)
		{
            _errorCode = 500;
            _requestReady = true;
            return;
        }
    }
    write(_bodyTmpfd, chunk, len);
    _bodyTmpSize += len;
}

void	HttpRequest::addChunkedbody(std::string &chunk) {
	_buffer.append(chunk);
	size_t offset = 0;

	while (offset < _buffer.size())
	{
		if (_readingSize)
		{
			size_t endSize = _buffer.find("\r\n", offset);
			if (endSize == std::string::npos)
				break;
				
			std::string sizeStr = _buffer.substr(offset, endSize - offset);
			_chunkSize = ft_strhexatoul(sizeStr);
			offset = endSize + 2;
			if (_bodyTmpSize + _chunkSize > _max_body_size) {
                _errorCode = 413;
                _requestReady = true;
                return;
            }
            if (!_chunkSize)
			{
                close(_bodyTmpfd);
                _bodyTmpfd = -1;
                _requestReady = true;
                _buffer.erase(0, offset + 2);
                return;
            }
            _readingSize = false;
        }
        if (_buffer.size() - offset < _chunkSize + 2)
            break;
        addToTmpBodyFile(_buffer.c_str() + offset, _chunkSize);
        offset += _chunkSize + 2;
        _readingSize = true;
    }
    _buffer.erase(0, offset);
}

void	HttpRequest::addContentlen(std::string &chunk) {
	if (!_contentLen && _headers.find("Content-Length") == _headers.end())
	{
        _requestReady = true;
		return;
	}
    size_t remaining = _contentLen - _bodyTmpSize;
    size_t toWrite = chunk.size() < remaining ? chunk.size() : remaining;
    if (toWrite > 0)
        addToTmpBodyFile(chunk.c_str(), toWrite);
    if (_bodyTmpSize >= _contentLen) {
        close(_bodyTmpfd);
        _bodyTmpfd = -1;
        _requestReady = true;
    }
}

void	HttpRequest::parseBody(std::string &chunk) {
	if (_headers.find("Transfer-Encoding") != _headers.end() && _headers["Transfer-Encoding"] == "chunked")
		addChunkedbody(chunk);
	else
		addContentlen(chunk);
}

void HttpRequest::clear() {
	_buffer.clear();
	_path.clear();
	_method.clear();
	_version.clear();
	_headers.clear();
	_requestReady = false;
	_headersReady = false;
	_errorCode = 0;
	_contentLen = 0;
	_chunkSize = 0;
	_readingSize = true;
	if (!_bodyTmpPath.empty())
	{
		unlink(_bodyTmpPath.c_str());
		_bodyTmpPath.clear();
	}
	if (_bodyTmpfd != -1)
	{
		close(_bodyTmpfd);
		_bodyTmpfd = -1;
	}
	_bodyTmpSize = 0;
	
}

void 	HttpRequest::addHeader(std::string key, std::string value) {
	removeSpacesAround(key);
	removeSpacesAround(value);
	if (key.empty() || value.empty() || isSpacesInside(key))
	{
		_errorCode = 400;
		return;
	}
	_headers[key] = value;
}

void	HttpRequest::parseFirstLine(std::string &firstLine) {
	std::stringstream stream(firstLine);
	stream >> _method;
	if (_method.empty()) {
		_errorCode = 400;
		_requestReady = true;
		return ;
	}
	stream >> _path;
	if (_path.find('?') != std::string::npos) {
		_queryString = _path.substr(_path.find('?') + 1);
		_path.erase(_path.find('?'));
	}
	else
		_queryString = "";
	if (_path.empty() || _path[0] != '/')
	{
		_errorCode = 400; // "HTTP: invalid path: " + _path
		_requestReady = true;
		return;
	}
	stream >> _version;
	if (_version != "HTTP/1.1")
	{
		_errorCode = 505; // "HTTP: unsupported version: " + _version
		_requestReady = true;
		return;
	}
}

void	HttpRequest::parseHeaders(int endOfHeaders, Client &client) {
	std::string line;
	std::string rest = _buffer.substr(endOfHeaders + 4);
	_buffer.erase(endOfHeaders + 4);
	size_t endline = _buffer.find("\r\n");
	line = _buffer.substr(0, endline);
	parseFirstLine(line);
	
	const Location* loc = client.getServer().matchLocation(_path);
	long long activeLimit = client.getServer().getMaxBodySize(); 
	if (loc && loc->getMaxBodySize() != -1) {
		activeLimit = loc->getMaxBodySize();
	}
	_max_body_size = activeLimit; 
	_buffer.erase(0, endline + 2);
	while ((endline = _buffer.find("\r\n")) != std::string::npos)
	{
		line = _buffer.substr(0, endline);
		size_t colon = line.find(":");
		if (colon != std::string::npos)
			addHeader(line.substr(0, colon), line.substr(colon + 1));
		if (_errorCode)
		{
			_headersReady = true;
			_requestReady = true; 
			return;
		}
		_buffer.erase(0, endline + 2);
	}
	if (_headers.find("Host") == _headers.end() || _headers["Host"].empty())
	{
		_errorCode = 505; // "HTTP: missing Host header"
		_requestReady = true;
		return;
	}
	_buffer.clear();
	_headersReady = true;
	if (_method == "POST") {
		if (_headers.find("Content-Length") != _headers.end()) {
			std::stringstream ss(_headers["Content-Length"]);
			ss >> _contentLen;

			if (_contentLen > _max_body_size || !ss.eof() || ss.fail()) {
				_errorCode = 413;
				_requestReady = true;
				return;
			}
		}
		parseBody(rest);
	}
	else {
		_requestReady = true;
	}
}

void HttpRequest::addToBuffer(std::string &chunk, Client &client) {
    if (_headersReady)
        return;
    _buffer.append(chunk);
    size_t startPos = _buffer.find_first_not_of("\r\n\t ");
    if (startPos == std::string::npos) {
        _buffer.clear();
        return;
    }
    if (startPos > 0)
        _buffer.erase(0, startPos);
    size_t endOfHeaders = _buffer.find("\r\n\r\n");
    if (endOfHeaders != std::string::npos) {
        parseHeaders(endOfHeaders, client);
        return;
    }
    if (_buffer.size() > MAX_HEADER_SIZE) {
        _errorCode = 431;
        _requestReady = true;
        return;
    }
}

std::ostream& operator<<(std::ostream &os, HttpRequest &req) {
	os << BOLD_CYAN "--- NEW HTTP REQUEST ---" RESET << std::endl;
	os << "version = " << req.getVersion() << std::endl;
	os << "method = " << req.getMethod() << std::endl;
	os << "path = " << req.getPath() << std::endl;
	os << "headers = " << std::endl;
	std::map<std::string, std::string>::iterator it = req.getHeaders().begin();
	while (it != req.getHeaders().end())
	{
		os << "->key: " << it->first << " ; value: " << it->second << std::endl;
		it++;
	}
	os << "bodyTmpPath = " << req.getBodyTmpPath() << std::endl;
	os << "buffer = " << req.getBuffer() << std::endl;
	os << "------" << std::endl;
	return os;
}
