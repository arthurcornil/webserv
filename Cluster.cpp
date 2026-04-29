
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
			std::list<std::string>	serverTokens;
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
			Server	newServer;
			newServer.parseConfig(serverTokens);
			_servers.push_back(newServer);
		}
		else
			throw std::runtime_error("Config: unexpected token: " + token);

	}
}

void Cluster::addToPoll(int socketFd) {
	struct pollfd pfd;
	fcntl(socketFd, F_SETFL, O_NONBLOCK);
	pfd.fd = socketFd;
	pfd.events = POLLIN;
	pfd.revents = 0;
	_pollFds.push_back(pfd);
}

void Cluster::addClient(Server &server) {
	Client newClient;
	newClient.setServer(server);
	struct sockaddr_in clientAddr;
	socklen_t addrSize = sizeof(clientAddr);
	int newClientFd = accept(server.getListenSocket(), (struct sockaddr*)&clientAddr, &addrSize);
	if (newClientFd == -1)
	{
		std::cerr << "accept() failed: " << std::string(std::strerror(errno)) << std::endl;
		return ;
	}
	else
		addToPoll(newClientFd);
	newClient.setFd(newClientFd);
	_clients.push_back(newClient);
	std::cout << "New client (fd:" << newClient.getFd() << ") connected to server (fd:" << newClient.getServer().getListenSocket() << ")" << std::endl;
}

Client* Cluster::findClient(int clientFd) {
	size_t i = 0;
	while (i < _clients.size())
	{
		if (_clients[i].getFd() == clientFd)
			return &_clients[i];
		i++;
	}
	return NULL;
}

Server* Cluster::findServer(int serverFd) {
	size_t i = 0;
	while (i < _servers.size())
	{
		if (_servers[i].getListenSocket() == serverFd)
			return &(_servers[i]);
		i++;
	}
	return NULL;
}

void Cluster::updatePoll(int clientFd, int event) {
	size_t i = 0;
	while (i < _pollFds.size())
	{
		if (_pollFds[i].fd == clientFd)
		{
			_pollFds[i].events = event;
			break;
		}
		i++;
	}
}

void Cluster::disconnect(Client &client) {
	const int clientFd = client.getFd();
	close(clientFd);
	for (std::vector<struct pollfd>::iterator it = _pollFds.begin(); it != _pollFds.end(); it ++) {
		if (it->fd == clientFd) {
			_pollFds.erase(it);
			break ;
		}
	}
	std::cout << "Client (fd:" << clientFd  << ") disconnected!" << std::endl;
	for (std::vector<Client>::iterator it = _clients.begin(); it != _clients.end(); it ++) {
		if (it->getFd() == clientFd) {
			_clients.erase(it);
			break;
		}
	}
}

void Cluster::disconnectTimedOut() {
	size_t i = 0;
	while (i < _clients.size())
	{
		if (_clients[i].isTimedOut())
			disconnect(_clients[i]);
		else
			i++;
	}
}

bool Cluster::readRequest(Client &client) {
	char chunk[BUFFER_SIZE];
	int bytes_read = recv(client.getFd(), chunk, sizeof(chunk), 0);
	if (bytes_read <= 0) {
		std::cout << "Client " << client.getFd() << " disconnected." << std::endl;
		disconnect(client);
		return false;
	}
	client.updateLastActivity();
	std::string buffer(chunk, bytes_read);
	client.parseRequest(buffer);
	
	return true;
}

bool Cluster::sendResponse(Client &client) {
	if (!client.sendResponse())
		return false;
	client.updateLastActivity();
	if (client.getResponse().getRaw().empty()) {
		client.getRequest().clear();
		client.getResponse().clear();
		updatePoll(client.getFd(), POLLIN);
	}
	return true;
}

void Cluster::serv() {
	size_t i = 0;
	while (i < _servers.size())
	{
		_servers[i].setUpListenSocket();
		std::cout << _servers[i] << std::endl;
		addToPoll(_servers[i++].getListenSocket());
	}
	std::cout << "Cluster is running..." << std::endl;
	while (true)
	{
		ssize_t readyCount = poll(&_pollFds[0], _pollFds.size(), TIMEOUT * 1000);
		if (readyCount < 0)
		{
			if (errno == EINTR)
				break ;
			throw std::runtime_error("poll() failed: " + std::string(std::strerror(errno)));
		}
		disconnectTimedOut();
		i = 0;
		while (i < _pollFds.size() && readyCount)
		{
			if (!_pollFds[i].revents) {
				i++;
				continue ;
			} 
			readyCount--;
			if (isServerFd(_pollFds[i].fd))
			{
				if (_pollFds[i].revents & POLLIN)
				{
					Server* server = findServer(_pollFds[i].fd);
					if (server)
						addClient(*server);
					else
						std::cerr << "Unknown server FD" << std::endl;
				}
				else if (_pollFds[i].revents & (POLLERR | POLLHUP | POLLNVAL))
				{
					std::cerr << "Error on FD " << _pollFds[i].fd << " - ";
					std::cerr << std::endl;
					//TODO: Restart server
				}
				i++;
				continue ;
			}
			else if (_cgis.find(_pollFds[i].fd) != _cgis.end()) {
				Client *client = _cgis[_pollFds[i].fd];
				Cgi *cgi = client->getCgi();

				if (_pollFds[i].fd == cgi->getWritePipe()) {
					if (_pollFds[i].revents & POLLOUT) {
						size_t remaining = cgi->getBody().length() - cgi->getBodyOffset();
						ssize_t bytesWritten = write(
							_pollFds[i].fd,
							cgi->getBody().c_str() + cgi->getBodyOffset(),
							remaining);
						if (bytesWritten > 0) {
							cgi->advanceBodyOffset(bytesWritten);
						} 
						if (cgi->getBodyOffset() >= cgi->getBody().length() || bytesWritten <= 0) {
							close(_pollFds[i].fd);
							_cgis.erase(_pollFds[i].fd);
							_pollFds.erase(_pollFds.begin() + i);
							continue ;
						}
					}
					else if (_pollFds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
						close(_pollFds[i].fd);
						_cgis.erase(_pollFds[i].fd);
						_pollFds.erase(_pollFds.begin() + i);
						continue ;
					}
					i++;
					continue ;
				} else if (_pollFds[i].fd == cgi->getReadPipe()) {
					bool isPipeActive = true;
					if (_pollFds[i].revents & POLLIN) {
						char buffer[BUFFER_SIZE];
						ssize_t bytesRead = read(client->getCgi()->getReadPipe(), buffer, sizeof(buffer));
						if (!bytesRead)
							isPipeActive = false;
						else if (bytesRead < 0)
							;//TODO: Respond with 500 error
						else
							client->getCgi()->appendToBuffer(buffer, bytesRead);
					}
					bool isHangupWithoutData = (_pollFds[i].revents & POLLHUP) && !(_pollFds[i].revents & POLLIN);
					if (!isPipeActive || isHangupWithoutData || (_pollFds[i].revents & (POLLERR | POLLNVAL))) {
						int status;
						waitpid(client->getCgi()->getPid(), &status, WNOHANG);
						client->getResponse().setCgiResponse(client->getCgi()->getBuffer(), client->getServer());
						updatePoll(client->getFd(), POLLOUT);
						close(_pollFds[i].fd);
						_cgis.erase(_pollFds[i].fd);
						_pollFds.erase(_pollFds.begin() + i);
						continue ; 
					}
					i++;
					continue ;
				}
			}
			Client* client = findClient(_pollFds[i].fd);
			if (!client)
			{
				close(_pollFds[i].fd);
				_pollFds.erase(_pollFds.begin() + i);
				continue;
			}
			bool isClientActive = true;
			if (_pollFds[i].revents & POLLIN)
			{
				isClientActive = readRequest(*client);
				if (isClientActive && client->getRequest().isReady())
				{
					bool isCgiProcess = client->setResponse();
					if (!isCgiProcess)
						updatePoll(client->getFd(), POLLOUT);
					else {
						updatePoll(client->getFd(), 0);
						addToPoll(client->getCgi()->getReadPipe());
						addToPoll(client->getCgi()->getWritePipe());
						updatePoll(client->getCgi()->getWritePipe(), POLLOUT);
						_cgis[client->getCgi()->getReadPipe()] = client;
						_cgis[client->getCgi()->getWritePipe()] = client;
					}
				}
			}
			if (isClientActive && (_pollFds[i].revents & POLLOUT)) {
				if (!sendResponse(*client)) {
					disconnect(*client);
					isClientActive = false;
				}
			}
			if (!isClientActive || _pollFds[i].revents & (POLLHUP | POLLERR | POLLNVAL))
			{
				disconnect(*client);
				isClientActive = false;
			}
			else if (isClientActive)
				i++;
		}
	}
}

bool Cluster::isServerFd(int fd) {
	for (size_t i = 0; i < _servers.size(); i ++) {
		if (_servers[i].getListenSocket() == fd) 
			return true;
	}
	return false;
}
