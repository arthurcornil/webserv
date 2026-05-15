#include "Cluster.hpp"

Cluster::Cluster() {}
Cluster::Cluster(Cluster const &copy): _tokens(copy._tokens), _servers(copy._servers), _pollFds(copy._pollFds), _clients(copy._clients)  {}
Cluster::~Cluster() {}
Cluster& Cluster::operator=(Cluster const &assignment) {
    if (this != &assignment)
    {
        _tokens = assignment._tokens;
        _servers = assignment._servers;
        _pollFds = assignment._pollFds;
        _clients = assignment._clients;
    }
    return *this;
}

void Cluster::config(std::string &configPath) {
    std::string buffer = lexer(configPath);
    _tokens = tokenizer(buffer);
    while (!_tokens.empty())
    {
        std::string token = _tokens.front();
        _tokens.pop_front();
        if (token == "server" && _tokens.front() == "{")
        {
            std::list<std::string>    serverTokens;
            int braceCount = 0;
            while (!_tokens.empty())
            {
                token = _tokens.front();
                _tokens.pop_front();
                if (token == "{") braceCount++;
                if (token == "}") braceCount--;
                serverTokens.push_back(token);
                if (braceCount == 0)
                    break;
            }
            Server    newServer;
            newServer.parseConfig(serverTokens);
            _servers.push_back(newServer);
        }
        else
            throw std::runtime_error("Config: unexpected token: " + token);

    }
}

bool Cluster::disconnect(Client &client) {
    const int clientFd = client.getFd();
    Cgi *cgi = client.getCgi();

    if (cgi->getReadPipe() != -1 && _cgis.find(cgi->getReadPipe()) != _cgis.end())
        removeCgiFd(cgi->getReadPipe());
    if (cgi->getWritePipe() != -1 && _cgis.find(cgi->getWritePipe()) != _cgis.end())
        removeCgiFd(cgi->getWritePipe());
    if (cgi->getBodyTmpFd() != -1)
        removeCgiFd(cgi->getBodyTmpFd());

    int pid = cgi->getPid();
    if (pid > 0) {
        kill(pid, SIGKILL);
        waitpid(pid, NULL, WNOHANG);
    }

    close(clientFd);
    removeFromPoll(clientFd);
    std::list<Client>::iterator it = _clients.begin();
    while (it != _clients.end()) {
        if (it->getFd() == clientFd) {
            _clients.erase(it);
            break;
        }
        it++;
    }
    std::cout << "Client (fd:" << clientFd << ") disconnected!" << std::endl;
    return false;
}

void Cluster::disconnectUnactives() {
    std::list<Client>::iterator it = _clients.begin();
    while (it != _clients.end())
    {
        if (it->isTimedOut()) {
            disconnect(*it);
            it = _clients.begin(); 
        }
        else
            it++;
    }
}

void Cluster::dispatchEvents(ssize_t &readyCount) {
	size_t i = 0;
	while (i < _pollFds.size() && readyCount)
    {
        if (!_pollFds[i].revents)
		{
			i++;
			continue;
		}
		readyCount--;
        if (findServer(_pollFds[i].fd))
			handleServerEvent(i);
        else if (!handleClientEvent(i))
			continue;
        i++;
    }
}

void Cluster::serv() {
    signal(SIGPIPE, SIG_IGN);
    size_t i = 0;
    while (i < _servers.size())
    {
        _servers[i].setUpListenSocket();
        addToPoll(_servers[i++].getListenSocket());
    }
    std::cout << "Cluster is running..." << std::endl;
    while (true)
    {
        ssize_t readyCount = poll(&_pollFds[0], _pollFds.size(), TIMEOUT * 1000);
        if (readyCount < 0)
        {
            if (errno == EINTR) break;
            throw std::runtime_error("poll() failed: " + std::string(std::strerror(errno)));
        }
        disconnectUnactives();
        dispatchEvents(readyCount);
    }
}
