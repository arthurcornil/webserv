#include "HttpResponse.hpp"
#include "Client.hpp"
#include "Cgi.hpp"

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

bool HttpResponse::handleUpload(Client &client) {
    if (client.getRequest().getMethod() != "POST") return false;
    if (!_location || _location->getUploadDir().empty()) return false;

    std::string ct = client.getRequest().getHeaders()["Content-Type"];
    if (ct.find("multipart/form-data") == std::string::npos) return false;

    size_t bpos = ct.find("boundary=");
    if (bpos == std::string::npos) { _statusCode = 400; return true; }
    std::string boundary = "--" + ct.substr(bpos + 9);
    if (!boundary.empty() && boundary[boundary.size()-1] == '"') boundary.erase(boundary.size()-1);
    size_t q = boundary.find('"', 2);
    if (q != std::string::npos) boundary.erase(q);
    size_t semi = boundary.find(';');
    if (semi != std::string::npos) boundary.erase(semi);

    std::string bodyPath = client.getRequest().getBodyTmpPath();
    if (bodyPath.empty()) { _statusCode = 400; return true; }
    std::ifstream in(bodyPath.c_str(), std::ios::binary);
    if (!in.is_open()) { _statusCode = 500; return true; }
    std::string body((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();

    std::string uploadDir = _location->getUploadDir();
    if (!uploadDir.empty() && uploadDir[uploadDir.size()-1] == '/')
        uploadDir.erase(uploadDir.size()-1);

    struct stat st;
    if (stat(uploadDir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
        _statusCode = 500;
        return true;
    }

    int filesWritten = 0;
    size_t pos = 0;
    while (true) {
        size_t start = body.find(boundary, pos);
        if (start == std::string::npos) break;
        start += boundary.length();
        if (body.compare(start, 2, "--") == 0) break;
        if (body.compare(start, 2, "\r\n") == 0) start += 2;

        size_t end = body.find(boundary, start);
        if (end == std::string::npos) break;

        size_t headerEnd = body.find("\r\n\r\n", start);
        if (headerEnd == std::string::npos || headerEnd >= end) { pos = end; continue; }

        std::string partHeaders = body.substr(start, headerEnd - start);
        size_t contentStart = headerEnd + 4;
        size_t contentEnd = end >= 2 ? end - 2 : end;
        if (contentEnd < contentStart) { pos = end; continue; }
        size_t contentLen = contentEnd - contentStart;

        size_t fn = partHeaders.find("filename=\"");
        if (fn == std::string::npos) { pos = end; continue; }
        fn += 10;
        size_t fnEnd = partHeaders.find('"', fn);
        if (fnEnd == std::string::npos) { pos = end; continue; }
        std::string filename = partHeaders.substr(fn, fnEnd - fn);

        size_t slash = filename.find_last_of("/\\");
        if (slash != std::string::npos) filename = filename.substr(slash + 1);
        if (filename.empty() || filename == "." || filename == "..") { pos = end; continue; }

        std::string outPath = uploadDir + "/" + filename;
        std::ofstream out(outPath.c_str(), std::ios::binary | std::ios::trunc);
        if (!out.is_open()) { _statusCode = 500; return true; }
        if (contentLen > 0)
            out.write(body.data() + contentStart, contentLen);
        out.close();
        filesWritten++;

        pos = end;
    }

    if (filesWritten == 0) {
        _statusCode = 400;
    } else {
        _statusCode = 201;
        _body = "";
    }
    setHeaders();
    setRaw();
    return true;
}

bool HttpResponse::set(Client &client) {
	_statusCode = 200;
	std::string rawRoute = this->getRawRoute(client);
	if (_statusCode == 200) {
		int parseError = client.getRequest().getErrorCode();
		if (parseError != 0)
			_statusCode = parseError;
	}
	if (_statusCode == 200 && shouldTriggerCgi(rawRoute)) {
		client.getCgi()->set(rawRoute, client, _location);
		client.getCgi()->execute();
		return false;
	}
	if (_statusCode == 200 && handleUpload(client))
		return true;
	std::string filename = this->getResolvedRoute(client);
	if (filename.length()) {
		std::ifstream file(filename.c_str(), std::ios::binary);
		std::ostringstream ss;
		ss << file.rdbuf();
		_body = ss.str();
	}
	if (client.getRequest().getMethod() == "HEAD")
		_body = "";
	this->setHeaders(filename, &client);
	this->setRaw();
	return true;
}

bool HttpResponse::setDelete(Client &client) {
    _statusCode = 200;
    std::string filename = this->getRawRoute(client);
    if (_statusCode != 200) {
        std::string empty;
        this->checkFile(empty, client.getServer(), empty);
        this->setHeaders();
        this->setRaw();
        return true;
    }
    struct stat statBuf;
    if (stat(filename.c_str(), &statBuf) != 0) {
        _statusCode = 404;
    } else if (S_ISDIR(statBuf.st_mode)) {
        _statusCode = 403;
    } else if (access(filename.c_str(), W_OK) != 0) {
        _statusCode = 403;
    } else if (unlink(filename.c_str()) != 0) {
        _statusCode = 500;
    } else {
        _statusCode = 204;
    }
    if (_statusCode >= 400) {
        std::string empty;
        this->checkFile(empty, client.getServer(), empty);
    } else {
        _body = "";
    }

    this->setHeaders();
    this->setRaw();
    return true;
}

void HttpResponse::setCgiResponse(std::string &output, Client &client) {
	Server &server = client.getServer();
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
		if (colonIndex == std::string::npos) continue;
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
    std::string body;
    body.swap(output);
    body.erase(0, nlIndex + separatorLen);
    _body.swap(body);
	this->setHeaders();
	this->setRaw();
}

void HttpResponse::setHeaders(std::string filename, Client *client) {
    _headers["Content-Length"] = sizetostr(_body.length());
    if (_headers.find("Content-type") == _headers.end())
        _headers["Content-type"] = filename.length() ? getMimeType(filename) : "text/html";
    _headers["Date"] = getIMFFixdate();
    _headers["Server"] = "webserv/1.0";
    if (_statusCode == 413 || _statusCode == 400)
        _headers["Connection"] = "close";
    else if (client && client->getRequest().getMethod() == "POST")
        _headers["Connection"] = "close";
    else
        _headers["Connection"] = "keep-alive";
    if (_statusCode >= 300 && _statusCode < 400 && client)
        _headers["Location"] = client->getRequest().getPath() + "/";
}

void HttpResponse::setRaw() {
	_raw = "HTTP/1.1 " + sizetostr(_statusCode) + " " + getStatusCodeMessage() + "\r\n";
	for (std::map<std::string, std::string>::iterator it = _headers.begin(); it != _headers.end(); it ++) {
		_raw.append(it->first + ": " + it->second + "\r\n");
	}
	_raw += "\r\n";
	if (_body.size() <= LARGE_BODY_THRESHOLD) {
		_raw.append(_body);
		std::string empty;
		_body.swap(empty);
	}
}

std::string &HttpResponse::getRaw() { return _raw; }

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
		case 201:
			return "Created";
		case 204:
			return "No Content";
		case 400:
			return "Bad Request";
		case 403:
			return "Forbidden";
		case 404:
			return "Not found";
		case 405:
			return "Method Not Allowed";
		case 411:
			return "Length Required";
		case 413:
			return "Payload Too Large";
		case 431:
			return "Request Header Fields Too Large";
		case 418:
			return "I'm a teapot";
		case 500:
			return "Internal Error";
		case 501:
			return "Not Implemented";
		case 502:
			return "Bad Gateway";
		case 505:
			return "HTTP Version Not Supported";
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

std::string HttpResponse::getRawRoute(Client &client) {
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
    // _statusCode = 200;
    if (!(client.getRequest().getMethod() == "GET"
        || client.getRequest().getMethod() == "POST"
        || client.getRequest().getMethod() == "DELETE")) {
        _statusCode = 501;
		std::cout << "501 Because wrong method: " << client.getRequest().getMethod() << std::endl;
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
    return rootPath + reqPath; 
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
	// _statusCode = 200;
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

size_t HttpResponse::buildCgiHeaders(std::string &buffer, Client &client) {
	size_t separatorLen;
	size_t nlIndex = buffer.find("\r\n\r\n");
	if (nlIndex != std::string::npos)
		separatorLen = 4;
	else
	{
		nlIndex = buffer.find("\n\n");
		if (nlIndex == std::string::npos)
	        return std::string::npos;
		separatorLen = 2;
	}
    const std::string rawHeaders = buffer.substr(0, nlIndex);
    std::stringstream ss(rawHeaders);
    std::string line;
    _statusCode = 200;
    while (std::getline(ss, line))
	{
        if (line.empty())
			continue;
        if (line[line.length() - 1] == '\r')
            line.erase(line.length() - 1);
        
		size_t colonIndex = line.find(':');
        if (colonIndex == std::string::npos)
			continue;
        const std::string key = line.substr(0, colonIndex);
        std::string value = line.substr(colonIndex + 1);
        if (!value.empty() && value[0] == ' ')
            value.erase(0, 1);
        if (key == "Status")
		{
            _statusCode = atoi(value.c_str());
            continue;
        }
        _headers[key] = value;
    }
    setHeaders(std::string(), &client);
    _headers.erase("Content-Length");
    setRaw();
    return nlIndex + separatorLen;
}

size_t HttpResponse::getSendOffset() const { return _sendOffset; }
int HttpResponse::getStatusCode() const { return _statusCode; }
void HttpResponse::setSendOffset(size_t val) { _sendOffset = val; }
const std::string& HttpResponse::getBody() const { return _body; }
std::map<std::string, std::string> &HttpResponse::getHeaders() { return _headers; }
