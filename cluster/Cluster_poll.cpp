#include "Cluster.hpp"

void Cluster::addToPoll(int socketFd) {
    struct pollfd pfd;
    fcntl(socketFd, F_SETFL, O_NONBLOCK);
    fcntl(socketFd, F_SETFD, FD_CLOEXEC);
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
    std::list<Client>::iterator it = _clients.begin();
    while (it != _clients.end())
    {
        if (it->getFd() == clientFd)
            return &(*it);
        it++;
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

Client* Cluster::findCgi(int fd) {
    std::map<int, Client*>::iterator it = _cgis.find(fd);
    if (it != _cgis.end())
        return it->second;
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

bool Cluster::removeFromPoll(int fd) {
    size_t i = 0;
    while (i < _pollFds.size()) {
        if (_pollFds[i].fd == fd) {
            _pollFds.erase(_pollFds.begin() + i);
            return false;
        }
        i++;
    }
    return false;
}