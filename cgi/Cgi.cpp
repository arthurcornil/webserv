#include "Cgi.hpp"
#include "../client/Client.hpp"

Cgi::Cgi() : _bodyOffset(0) {}

void Cgi::set(std::string filename, Client &client, Location *location) {
	_env.clear();
	_formattedEnv.clear();
	_rawEnv.clear();
	_buffer.clear();
	const size_t dotIndex = filename.find_last_of('.');
	std::string extension = filename.substr(dotIndex);
	std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
	std::map<std::string, std::string> &cgis = location->getCgi();
	_cgiPath = cgis.find(extension)->second;
	_body = client.getRequest().getBody();
	this->setEnv(filename, client);

	for (std::map<std::string, std::string>::iterator it = _env.begin(); it != _env.end(); it ++) {
		_formattedEnv.push_back(it->first + "=" + it->second);
	}
	for (size_t i = 0; i < _formattedEnv.size(); i ++) {
		_rawEnv.push_back(const_cast<char *>(_formattedEnv[i].c_str()));
	}
	_rawEnv.push_back(NULL);
	_buffer.reserve(client.getRequest().getBody().length());
}

Cgi::Cgi(const Cgi &other)
	: _env(other._env), _formattedEnv(other._formattedEnv), _rawEnv(other._rawEnv),
	_readPipe(other._readPipe), _writePipe(other._writePipe), _pid(other._pid),
	_buffer(other._buffer), _cgiPath(other._cgiPath), _body(other._body), _bodyOffset(other._bodyOffset) {}

Cgi& Cgi::operator=(const Cgi &other) {
	if (this != &other) {
		_env = other._env;
		_formattedEnv = other._formattedEnv;
		_rawEnv = other._rawEnv;
		_readPipe = other._readPipe;
		_writePipe = other._writePipe;
		_pid = other._pid;
		_buffer = other._buffer;
		_cgiPath = other._cgiPath;
		_body = other._body;
		_bodyOffset = other._bodyOffset;
	}
	return (*this);
}

Cgi::~Cgi() {}

void Cgi::setEnv(std::string filename, Client &client) {
    _env["GATEWAY_INTERFACE"] = "CGI/1.1";
    _env["SERVER_PROTOCOL"] = "HTTP/1.1";
    _env["SERVER_SOFTWARE"] = "webserv/1.0";
    _env["SERVER_NAME"] = client.getServer().getHost();
    _env["SERVER_PORT"] = sizetostr(client.getServer().getPort());
    _env["REQUEST_METHOD"] = client.getRequest().getMethod();
    _env["QUERY_STRING"] = client.getRequest().getQueryString();
    _env["REDIRECT_STATUS"] = "200";

	//?
	_env["SCRIPT_NAME"] = client.getRequest().getPath();
	_env["PATH_INFO"] = client.getRequest().getPath();
	_env["REQUEST_URI"] = client.getRequest().getPath();

	//?
    _env["SCRIPT_FILENAME"] = filename;
	//?
    _env["PATH_TRANSLATED"] = filename;
    if (client.getRequest().getMethod() == "POST") {
        _env["CONTENT_LENGTH"] = sizetostr(_body.length());
        _env["CONTENT_TYPE"] = client.getRequest().getHeaders()["Content-Type"];
    }

    std::map<std::string, std::string> headers = client.getRequest().getHeaders();
    for (std::map<std::string, std::string>::iterator it = headers.begin(); it != headers.end(); ++it) {
        std::string headerName = it->first;
        std::string envName = "HTTP_";
        for (size_t i = 0; i < headerName.length(); i++) {
            if (headerName[i] == '-')
                envName += '_';
            else
                envName += toupper(headerName[i]);
        }
        if (_env.find(envName) == _env.end()) {
            _env[envName] = it->second;
        }
    }
}

void Cgi::execute() {
    int readFds[2];
    int writeFds[2];
    if (pipe(readFds) == -1) {
        throw std::runtime_error("Cgi: Failed to create read pipe");
    }
    if (pipe(writeFds) == -1) {
        close(readFds[0]);
        close(readFds[1]);
        throw std::runtime_error("Cgi: Failed to create write pipe");
    }
	fcntl(readFds[0], F_SETFL, O_NONBLOCK);
	fcntl(writeFds[1], F_SETFL, O_NONBLOCK);
    _pid = fork();
    if (_pid < 0) {
        close(readFds[0]);
        close(readFds[1]);
        close(writeFds[0]);
        close(writeFds[1]);
        throw std::runtime_error("Cgi: Failed to fork process");
    }

    if (_pid == 0) {
        dup2(readFds[1], STDOUT_FILENO);
        dup2(writeFds[0], STDIN_FILENO);
        close(readFds[0]);
        close(readFds[1]);
        close(writeFds[0]);
        close(writeFds[1]);

        std::string scriptPath = _env["SCRIPT_FILENAME"];
        size_t lastSlash = scriptPath.find_last_of("/");
        if (lastSlash != std::string::npos) {
            std::string directory = scriptPath.substr(0, lastSlash + 1);
            if (chdir(directory.c_str()) == -1) {
                std::cerr << "[ERROR] Cgi: chdir failed for " << directory << std::endl;
                exit(EXIT_FAILURE);
            }
        }

        char *argv[] = {
            const_cast<char *>(_cgiPath.c_str()),
            const_cast<char *>(scriptPath.c_str()),
            NULL
        };
        
        execve(_cgiPath.c_str(), argv, &_rawEnv[0]);
        perror("execve failed");
        exit(EXIT_FAILURE);
    }
    close(readFds[1]);
    close(writeFds[0]);
    _readPipe = readFds[0];
    _writePipe = writeFds[1];
}

int Cgi::getReadPipe() const  { return _readPipe; }
int Cgi::getWritePipe() const  { return _writePipe; }
size_t Cgi::getBodyOffset() const  { return _bodyOffset; }
pid_t Cgi::getPid() const { return _pid; }
void Cgi::advanceBodyOffset(size_t val) { _bodyOffset += val; }
void Cgi::appendToBuffer(char *str, ssize_t size) { _buffer.append(str, size); };
std::string &Cgi::getBuffer() { return _buffer; }
std::string &Cgi::getBody() { return _body; }
