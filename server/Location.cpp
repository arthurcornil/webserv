
#include "Location.hpp"

Location::Location(): _autoindex(false), _returnCode(-1), _allow_get(false), _allow_post(false), _allow_delete(false), _client_max_body_size(-1) {}
Location::Location(Location const &copy) {
	*this = copy;
}
Location::~Location() {}
Location& Location::operator=(Location const &assignment) {
	if (this != &assignment)
	{
		_locationTokens = assignment._locationTokens;
		_path = assignment._path;
		_root = assignment._root;
		_indexes = assignment._indexes;
		_autoindex = assignment._autoindex;
		_returnCode = assignment._returnCode;
		_returnUrl = assignment._returnUrl;
		_allow_get = assignment._allow_get;
		_allow_post = assignment._allow_post;
		_allow_delete = assignment._allow_delete;
		_uploadDir = assignment._uploadDir;
		_cgi = assignment._cgi;
		_client_max_body_size = assignment._client_max_body_size;
	}
	return *this;
}

std::string& Location::getPath() {return _path;}
std::string& Location::getRoot()  {return _root;}
bool&	Location::isAutoindex() {return _autoindex;}
bool&	Location::canGet() {return _allow_get;}
bool&	Location::canPost() {return _allow_post;}
bool&	Location::canDelete() {return _allow_delete;}
int&	Location::getRedirectionCode() {return _returnCode;}
std::string& Location::getRedirectionUrl() {return _returnUrl;}
std::string& Location::getUploadDir() {return _uploadDir;}
std::vector<std::string>& Location::getIndexes() {return _indexes;}
std::map<std::string, std::string>&	Location::getCgi() {return _cgi;}
long long Location::getMaxBodySize() const { return _client_max_body_size; }
bool	Location::getMethod(std::string &method) {
	if (method == "GET")
		return canGet();
	if (method == "POST")
		return canPost();
	if (method == "DELETE")
		return canDelete();
	return false;
}

void Location::expected(std::string expected) {
	if (_locationTokens.empty() || _locationTokens.front() != expected)
		throw std::runtime_error("Config: expected '" + expected + "' but got '" + _locationTokens.front() + "'");
	_locationTokens.pop_front();
}

void Location::setPath() {
	_path = _locationTokens.front();
	_locationTokens.pop_front();
}

void Location::setRoot() {
	_root = _locationTokens.front();
	_locationTokens.pop_front();
}

void Location::addIndexes() {
	if (_locationTokens.empty() || _locationTokens.front() == ";")
		std::runtime_error("Config: no arguments provided for index directive");
	while (!_locationTokens.empty() && _locationTokens.front() != ";")
	{
		_indexes.push_back(_locationTokens.front());
		_locationTokens.pop_front();
	}
	if (_locationTokens.empty())
        throw std::runtime_error("Config: missing ';' after index directive");
}

void Location::setAutoindex() {
	if (_locationTokens.front() == "on") _autoindex = true;
	else if (_locationTokens.front() == "off") _autoindex = false;
	else
		throw std::runtime_error("Config: autoindex must be 'on' or 'off'");
	_locationTokens.pop_front();
}

void Location::setRedirection() {
	std::string code = _locationTokens.front();
	_locationTokens.pop_front();
	std::stringstream ss(code);
	ss >> _returnCode;
	if (ss.fail() || !ss.eof() || _returnCode < 200 || _returnCode > 599)
		throw std::runtime_error("Config: invalid return code: " + code);
	_returnUrl = _locationTokens.front();
	_locationTokens.pop_front();
}

void Location::setMethod() {
	while (!_locationTokens.empty() && _locationTokens.front() != ";")
	{
		if (_locationTokens.front() == "GET") _allow_get = true;
		else if (_locationTokens.front() == "POST") _allow_post = true;
		else if (_locationTokens.front() == "DELETE") _allow_delete = true;
		else
			throw std::runtime_error("Config: unknown method: " + _locationTokens.front());
		_locationTokens.pop_front();
	}
}

void Location::setUploadDir() {
	_uploadDir = _locationTokens.front();
	_locationTokens.pop_front();
}

void Location::setClientMaxBodySize() {
    std::string token = _locationTokens.front();
    _locationTokens.pop_front();

    char unit = token[token.length() - 1];
    std::string value = token.substr(0, token.length() - 1);

    std::stringstream ss(value);
    long long size = 0;
    ss >> size;
    if (ss.fail() || !ss.eof() || size < 0)
        throw std::runtime_error("Config: invalid client_max_body_size value");

    if (unit == 'K' || unit == 'k')
    {
        if (size > (long long)MAX_BODY_SIZE_GO * 1024 * 1024)
            size = (long long)MAX_BODY_SIZE_GO * 1024 * 1024;
        size *= 1024;
    }
    else if (unit == 'M' || unit == 'm')
    {
        if (size > (long long)MAX_BODY_SIZE_GO * 1024)
            size = (long long)MAX_BODY_SIZE_GO * 1024;
        size *= 1024 * 1024;
    }
    else if (unit == 'G' || unit == 'g')
    {
        if (size > MAX_BODY_SIZE_GO)
            size = MAX_BODY_SIZE_GO;
        size *= 1024 * 1024 * 1024;
    }
	else if (unit == 'B' || unit == 'b')
	{
		if (size > (long long)MAX_BODY_SIZE_GO * 1024 * 1024 * 1024)
			size = (long long)MAX_BODY_SIZE_GO * 1024 * 1024 * 1024;
	}
    else
        throw std::runtime_error("Config: invalid client_max_body_size unit");

    _client_max_body_size = size;
}

void Location::addCgi() {
	std::string extension = _locationTokens.front();
	_locationTokens.pop_front();
	if (_locationTokens.empty() || _locationTokens.front() == ";")
		throw std::runtime_error("Config: missing binary path for cgi extension: " + extension);
	std::string binPath = _locationTokens.front();
	_locationTokens.pop_front();
	_cgi[extension] = binPath;
}

void Location::isValid(Server &parentServer) {
	if (_root.empty())
		_root = parentServer.getRoot();
	if (_indexes.empty())
		_indexes = parentServer.getIndexes();
	if (!_allow_get && !_allow_post && !_allow_delete)
		_allow_get = true;
}

void	Location::parser(std::list<std::string> &locationTokens) {
	_locationTokens = locationTokens;
	setPath();
	expected("{");
	while (!_locationTokens.empty() && _locationTokens.front() != "}")
	{
		std::string directive = _locationTokens.front();
		_locationTokens.pop_front();
		if (directive == "root") setRoot();
		else if (directive == "index") addIndexes();
		else if (directive == "autoindex") setAutoindex();
		else if (directive == "return") setRedirection();
		else if (directive == "allow_methods") setMethod();
		else if (directive == "upload_dir") setUploadDir();
		else if (directive == "cgi") addCgi();
		else if (directive == "client_max_body_size") setClientMaxBodySize();
		else
			throw std::runtime_error("Config: unknown location directive: " + directive);
		expected(";");
	}
	expected("}");
}

std::ostream& operator<<(std::ostream &os, Location &loc) {
	os << BOLD_MAGENTA "--- Location ---" RESET << std::endl;

	os << "_path = " << loc.getPath() << std::endl;
	os << "_root = " << loc.getRoot() << std::endl;
	os << "_autoindex = " << loc.isAutoindex() << std::endl;
	os << "_returnCode = " << loc.getRedirectionCode() << std::endl;
	os << "_returnUrl = " << loc.getRedirectionUrl() << std::endl;
	os << "_allow_get = " << loc.canGet() << std::endl;
	os << "_allow_post = " << loc.canPost() << std::endl;
	os << "_allow_delete = " << loc.canDelete() << std::endl;
	os << "_uploadDir = " << loc.getUploadDir() << std::endl;
	os << "_indexes = " << std::endl;
	std::vector<std::string>::iterator it = loc.getIndexes().begin();
	while (it != loc.getIndexes().end())
	{
		os << "-> " << *it << std::endl;
		it++;
	}
	os << "_cgi = " << std::endl;
	std::map<std::string, std::string>::iterator ite = loc.getCgi().begin();
	while (ite != loc.getCgi().end())
	{
		os << "->key: " << ite->first << " ; value: " << ite->second << std::endl;
		ite++;
	}
	os << "---";
	return os;
}
