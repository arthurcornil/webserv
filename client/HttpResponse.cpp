#include "HttpResponse.hpp"
#include "Client.hpp"
#include "../cgi/Cgi.hpp"
#include <sys/stat.h>
#include <dirent.h>

HttpResponse::HttpResponse(): _location(NULL), _sendOffset(0) {}

HttpResponse::HttpResponse(HttpResponse const &copy) :
	_statusCode(copy._statusCode), _headers(copy._headers), _body(copy._body), _raw(copy._raw),  _location(copy._location), _sendOffset(copy._sendOffset) {}

HttpResponse& HttpResponse::operator=(HttpResponse const &assignment) {
	if (this != &assignment)
	{
		_statusCode = assignment._statusCode;
		_headers = assignment._headers;
		_body = assignment._body;
		_raw = assignment._raw;
		_location = assignment._location;
		_sendOffset = assignment._sendOffset;
	}
	return *this;
}

HttpResponse::~HttpResponse() {}

bool HttpResponse::set(Client &client) {
	std::string filename = this->getResolvedRoute(client);
	if (shouldTriggerCgi(filename)) {
		client.getCgi()->set(filename, client, _location);
		client.getCgi()->execute();
		return true;
	}
	else if (filename.length()) {
		std::ifstream file(filename.c_str(), std::ios::binary);
		std::ostringstream ss;
		ss << file.rdbuf();
		_body = ss.str();
	}
	if (client.getRequest().getMethod() == "HEAD")
		_body = "";
	this->setHeaders(filename, &client);
	this->setRaw();
	return false;
}

bool HttpResponse::setDelete(Client &client) {(void)client; return false;}

void HttpResponse::setCgiResponse(std::string &output, Server &server) {
	size_t nlIndex = output.find("\r\n\r\n");
	size_t separatorLen = 4;

	if (nlIndex == std::string::npos) {
		nlIndex = output.find("\n\n");
		separatorLen = 2;
	}
	if (nlIndex == std::string::npos) {
		_statusCode = 502;
		std::string tmp = "";
		this->checkFile(tmp, server, tmp);
		this->setHeaders();
		this->setRaw();
		return ;
	}

	const std::string rawHeaders = output.substr(0, nlIndex);
	std::stringstream ss(rawHeaders);
	std::string line;
	while (std::getline(ss, line)) {
		if (line.empty()) continue;
		if (line[line.length() - 1] == '\r')
			line.erase(line.length() - 1);
		size_t colonIndex = line.find(':');
		const std::string key = line.substr(0, colonIndex);
		std::string value = line.substr(colonIndex + 2);
		if (!value.empty() && value[0] == ' ')
			value.erase(0, 1);
		if (key == "Status") {
			_statusCode = atoi(value.c_str());
			continue ;
		}
		_headers[key] = value;
	}
	_body = output.substr(nlIndex + separatorLen);
	this->setHeaders();
	this->setRaw();
}

void HttpResponse::setHeaders(std::string filename, Client *client) {
	_headers["Content-Length"] = sizetostr(_body.length());
	if (_headers.find("Content-type") == _headers.end())
		_headers["Content-type"] = filename.length() ? getMimeType(filename) : "text/html";
	_headers["Date"] = getIMFFixdate();
	_headers["Server"] = "webserv/1.0";
	//TODO: Make connection dynamic
	_headers["Connection"] = "keep-alive";
	if (_statusCode >= 300 && _statusCode < 400 && client) {
		_headers["Location"] = client->getRequest().getPath() + "/";
	}
}

void HttpResponse::setRaw() {
	_raw = "HTTP/1.1 " + sizetostr(_statusCode) + " " + getStatusCodeMessage() + "\r\n";
	for (std::map<std::string, std::string>::iterator it = _headers.begin(); it != _headers.end(); it ++) {
		_raw.append(it->first + ": " + it->second + "\r\n");
	}
	_raw += "\r\n";
	_raw.append(_body);
}

std::string HttpResponse::getRaw() const { return _raw; }

void HttpResponse::clear() {
	_headers.clear();
	_raw.clear();
	_body.clear();
	_location = NULL;
	_sendOffset = 0;
}

std::string HttpResponse::getStatusCodeMessage() {
	switch(_statusCode) {
		case 200:
			return "OK";
		case 403:
			return "Forbidden";
		case 404:
			return "Not found";
		case 405:
			return "Method Not Allowed";
		case 418:
			return "I'm a teapot";
		case 500:
			return "Internal Error";
		case 501:
			return "Not Implemented";
		case 502:
			return "Bad Gateway";
		default :
			return "";
	}
}

std::string HttpResponse::getMimeType(std::string filename) {
	const size_t dotIndex = filename.find_last_of('.');
	if (dotIndex == std::string::npos)
		return "application/octet-stream";
	std::string extension = filename.substr(dotIndex);
	std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
	if (extension == ".html")
		return "text/html";
	else if (extension == ".css")
		return "text/css";
	else if (extension == ".js")
		return "text/javascript";
	else if (extension == ".txt")
		return "text/plain";
	else if (extension == ".png")
		return "image/png";
	else if (extension == ".jpg" || extension == ".jpeg")
		return "image/jpeg";
	else if (extension == ".ico")
		return "image/x-icon";
	return "application/octet-stream";
}

std::string HttpResponse::getIMFFixdate() {
	char buf[1000];
	time_t now = time(0);
	struct tm tm = *gmtime(&now);
	strftime(buf, sizeof buf, "%a, %d %b %Y %H:%M:%S GMT", &tm);
	return std::string(buf);
}

std::string HttpResponse::checkFile(std::string &filename, Server &server, std::string &reqPath) {
	struct stat statBuf;
	int statStatus = -1;
	if (_statusCode == 200) {
		statStatus = stat(filename.c_str(), &statBuf);
		if (statStatus != 0)
			_statusCode = 404;
		else if (access(filename.c_str(), R_OK) != 0) {
			_statusCode = 403;
			statStatus = -1;
		}
		else if (S_ISDIR(statBuf.st_mode)) {
			if (filename[filename.length() - 1] != '/')
				_statusCode = 301;
			else {
				try {
					this->findDefaultFile(filename, reqPath);
				} catch(const std::exception &e) {
					_statusCode = 404;
					statStatus = -1;
				}
			}
		}
	}
	if (_statusCode != 200) {
		if (server.getErrorPages().find(_statusCode) != server.getErrorPages().end()) {
			filename = server.getErrorPages()[_statusCode];
			statStatus = stat(filename.c_str(), &statBuf);
			if (statStatus == 0 && (S_ISDIR(statBuf.st_mode) || access(filename.c_str(), R_OK) != 0))
				statStatus = -1;
		}
		if (statStatus != 0) {
			this->generateDefaultErrorPage();
			filename = "";
		}
	}
	return filename;
}

std::string HttpResponse::getResolvedRoute(Client &client) {
	std::vector<Location> &locations = client.getServer().getLocations();
	std::string reqPath = client.getRequest().getPath();

	for (size_t i = 0; i < locations.size(); i ++) {
		const std::string currLocPath = locations[i].getPath();
		if (reqPath.find(currLocPath) == 0) {
			if (!(reqPath.length() == currLocPath.length()
				|| reqPath[currLocPath.length()] == '/'
				|| currLocPath[currLocPath.length() - 1] == '/'))
					continue ;
			else if (!_location || currLocPath.length() > _location->getPath().length())
				_location = &(locations[i]);
		}
	}
	if (_location)
		reqPath.erase(reqPath.begin(), reqPath.begin() + _location->getPath().length());
	_statusCode = 200;
	if (!(client.getRequest().getMethod() == "GET"
		|| client.getRequest().getMethod() == "POST"
		|| client.getRequest().getMethod() == "DELETE")) {
		_statusCode = 501;
	}
	if (_location && !_location->getMethod(client.getRequest().getMethod())) {
		_statusCode = 405;
	}
	std::string rootPath = !_location ? client.getServer().getRoot() : _location->getRoot();
	if (rootPath[rootPath.length() - 1] != '/') {
		rootPath.append("/");
	}
	if (reqPath[0] == '/')
		reqPath.erase(reqPath.begin());
	std::string resolvedPath = rootPath + reqPath;
	return this->checkFile(resolvedPath, client.getServer(), reqPath);
}

void HttpResponse::findDefaultFile(std::string &path, std::string &reqUri) {
	bool foundDefault = false;
	if (_location && !_location->getIndexes().empty()) {
		const std::vector<std::string> &indexes = _location->getIndexes();
		for (size_t i = 0; i < indexes.size(); i ++) {
			const std::string currPath = path + indexes[i];
			struct stat statBuf;
			int statStatus = stat(currPath.c_str(), &statBuf);
			if (statStatus != 0)
				continue ;
			if (S_ISREG(statBuf.st_mode) && access(currPath.c_str(), R_OK) == 0) {
				path = currPath;
				foundDefault = true;
				break ;
			}
		}
	}
	if (!foundDefault) {
		bool triggerAutoindex = false;
		if (_location && _location->isAutoindex())
			triggerAutoindex = generateAutoindex(path, reqUri);
		if (!triggerAutoindex) {
			_statusCode = 403;
			throw NoDefaultContentException();
		}
	}
}

void HttpResponse::generateDefaultErrorPage() {
	_body = "<html>\r\n"
           "<head><title>" + sizetostr(_statusCode) + " " + getStatusCodeMessage() + "</title></head>\r\n"
           "<body style=\"text-align: center; margin-top: 50px;\">\r\n"
           "<h1>" + sizetostr(_statusCode) + " " + getStatusCodeMessage() + "</h1>\r\n"
           "<hr>\r\n"
           "<p>webserv</p>\r\n"
           "</body>\r\n"
           "</html>\r\n";
}

bool HttpResponse::generateAutoindex(std::string &path, std::string reqUri) {
	DIR *dir = opendir(path.c_str());
	if (!dir) return false;

	if (!reqUri.empty() && reqUri[reqUri.length() - 1] != '/') {
        reqUri += "/";
    }
	_body = "<html><head><title>Index of " + reqUri + "</title></head>\r\n"
		"<body>\r\n"
		"<h1>Index of " + reqUri + "</h1><hr>\r\n<pre>\r\n";
	struct dirent *entry;
	while ((entry = readdir(dir)) != NULL) {
		const std::string name = entry->d_name;
		if (name[0] == '.' && name != "..") continue ;
		_body += "<a href=\"" + reqUri + name + "\">" + name + "</a>\r\n";
	}
	closedir(dir);
    _body += "</pre><hr></body></html>\r\n";
	path = "";
	return true;
}

bool HttpResponse::shouldTriggerCgi(std::string filename) {
	const size_t dotIndex = filename.find_last_of('.');
	if (dotIndex == std::string::npos || !_location)
		return false;

	std::string extension = filename.substr(dotIndex);
	std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
	std::map<std::string, std::string> &cgis = _location->getCgi();

	if (cgis.find(extension) == cgis.end())
		return false;
	return true;
}

size_t HttpResponse::getSendOffset() const { return _sendOffset; }
void HttpResponse::setSendOffset(size_t val) { _sendOffset = val; }
