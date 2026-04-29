
#include "HttpRequest.hpp"

HttpRequest::HttpRequest(): _requestReady(false), _headersReady(false), _errorCode(0), _chunkSize(0), _readingSize(true), _contentLen(0) {}
HttpRequest::HttpRequest(HttpRequest const &copy) {
	*this = copy;
}
HttpRequest::~HttpRequest() {}
HttpRequest& HttpRequest::operator=(HttpRequest const &assignment) {
	if (this != &assignment)
	{
		_buffer = assignment._buffer;
		_path = assignment._path;
		_method = assignment._method;
		_version = assignment._version;
		_headers = assignment._headers;
		_body = assignment._body;
		_requestReady = assignment._requestReady;
		_headersReady = assignment._headersReady;
		_max_body_size = assignment._max_body_size;
		_readingSize = assignment._readingSize;
		_chunkSize = assignment._chunkSize;
		_contentLen = assignment._contentLen;
		_errorCode = assignment._errorCode;
	}
	return *this;
}

std::string& HttpRequest::getMethod() {return _method;}
std::string& HttpRequest::getPath() {return _path;}
std::string& HttpRequest::getQueryString() {return _queryString;}
std::string& HttpRequest::getVersion() {return _version;}
std::string& HttpRequest::getBody() {return _body;}
std::string& HttpRequest::getBuffer() {return _buffer;}
std::map<std::string, std::string>& HttpRequest::getHeaders() {return _headers;}
bool HttpRequest::endOfHeaders() {return _headersReady;}
bool HttpRequest::isReady() {return _requestReady;}
size_t	HttpRequest::getMaxBodySize() {return _max_body_size;}

void	HttpRequest::setMaxBodySize(int maxBodySize) {_max_body_size = maxBodySize;}
void	HttpRequest::setReady(bool ready) {_requestReady = ready;}

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
			if (!_chunkSize) {
				_requestReady = true;
				_buffer.erase(0, offset + 2); 
				return;
			}
			_readingSize = false;
		}
		
		if (_buffer.size() - offset < _chunkSize + 2)
			break;
			
		_body.append(_buffer.substr(offset, _chunkSize));
		offset += _chunkSize + 2;
		_readingSize = true;
	}
	_buffer.erase(0, offset);
}

void	HttpRequest::addContentlen(std::string &chunk) {
	if (!_contentLen && _headers.find("Content-Length") == _headers.end())
	{
		_errorCode = 411;
		_requestReady = true;
		return;
	}
	
	_body.append(chunk);
	
	if (_body.size() >= _contentLen) {
		if (_body.size() > _contentLen)
			_body.resize(_contentLen); 
		_requestReady = true;
	}
}

void	HttpRequest::parseBody(std::string &chunk) {
	if (_body.size() + chunk.size() > _max_body_size)
	{
		_errorCode = 413 ; // body > client max body size
		_requestReady = true;
		return;
	}
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
	_body.clear();
	_requestReady = false;
	_headersReady = false;
	_errorCode = 0;
	_contentLen = 0;
	_chunkSize = 0;
	_readingSize = true;
}

void 	HttpRequest::addHeader(std::string key, std::string value) {
	removeSpacesAround(key);
	removeSpacesAround(value);
	if (key.empty() || value.empty() || isSpacesInside(key))
	{
		_errorCode = 400; // "HTTP: invalid header: " + key;
		_requestReady = true;
		return;
	}
	_headers[key] = value;
}

void	HttpRequest::parseFirstLine(std::string &firstLine) {
	std::stringstream stream(firstLine);
	stream >> _method;
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

void	HttpRequest::parseHeaders(int endOfHeaders) {
	std::string line;
	std::string rest = _buffer.substr(endOfHeaders + 4);
	_buffer.erase(endOfHeaders + 4);
	size_t endline = _buffer.find("\r\n");
	line = _buffer.substr(0, endline);
	parseFirstLine(line);
	_buffer.erase(0, endline + 2);
	while ((endline = _buffer.find("\r\n")) != std::string::npos)
	{
		line = _buffer.substr(0, endline);
		size_t colon = line.find(":");
		if (colon != std::string::npos)
			addHeader(line.substr(0, colon), line.substr(colon + 1));
		if (_errorCode)
		{
			std::cout << "Parser triggered error code: " << _errorCode << std::endl;
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
			_body.reserve(_contentLen); 
		}
		parseBody(rest);
	}
	else
		_requestReady = true;
}

void	HttpRequest::addToBuffer(std::string &chunk) {
	if (!_headersReady && _buffer.size() + chunk.size() > MAX_HEADER_SIZE)
	{
		_errorCode = 431; // "HTTP: headers too long"
		_requestReady = true;
		return;
	}
	_buffer.append(chunk);
	size_t endOfHeaders = _buffer.find("\r\n\r\n");
	if (endOfHeaders != std::string::npos)
		parseHeaders(endOfHeaders);
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
	os << "body = " << req.getBody() << std::endl;
	os << "buffer = " << req.getBuffer() << std::endl;
	os << "------" << std::endl;
	return os;
}
