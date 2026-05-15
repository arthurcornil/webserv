#include "Cluster.hpp"

bool Cluster::handleClientEvent(size_t &i) {
	if (findCgi(_pollFds[i].fd))
        return handleCgiEvent(i);
    Client* client = findClient(_pollFds[i].fd);
    if (!client)
    {
        close(_pollFds[i].fd);
        _pollFds.erase(_pollFds.begin() + i);
        return false;
    }
    if (_pollFds[i].revents & POLLIN)
        if (!readFromClient(*client)) return false;
    if (_pollFds[i].revents & POLLOUT)
        if (!writeToClient(*client)) return false;
    if (_pollFds[i].revents & (POLLHUP | POLLERR | POLLNVAL))
        return disconnect(*client);
    return true;
}

bool Cluster::readFromClient(Client &client) {
	char chunk[BUFFER_SIZE];
    int bytes_read = recv(client.getFd(), chunk, sizeof(chunk), 0);
    if (bytes_read <= 0 || client.getCgi()->getCgiPipeEnded())
		return disconnect(client);
	client.updateLastActivity();
    std::string buffer(chunk, bytes_read);
    client.parseRequest(buffer);
    if (!client.getRequest().isReady())
		return true;
	if (client.getRequest().getMethod().empty())
		return disconnect(client);
	
	if (client.responseIsReady())
        updatePoll(client.getFd(), POLLOUT);
	else 
		addCgiPipeToPoll(client);
    return true;
}

void Cluster::addCgiPipeToPoll(Client &client) {
    Cgi *cgi = client.getCgi();
	int pipe[2] = {cgi->getReadPipe(), cgi->getWritePipe()};
    updatePoll(client.getFd(), 0);                 // on n'écoute plus la socket cliente
    addToPoll(pipe[0]);
    addToPoll(pipe[1]);
    updatePoll(pipe[1], POLLOUT);
    _cgis[pipe[0]] = &client;
    _cgis[pipe[1]] = &client;

	if (!cgi->getBodyTmpPath().empty())
	{
        int tmpFd = open(cgi->getBodyTmpPath().c_str(), O_RDONLY);
        if (tmpFd != -1)
		{
            cgi->setBodyTmpFd(tmpFd);
            addToPoll(tmpFd);
            updatePoll(tmpFd, POLLIN);
            _cgis[tmpFd] = &client;
        }
	}
}

bool Cluster::writeToClient(Client &client) {
    if (client.getCgi()->getHeadersSent())
        return CgiWriteProcess(client);
    if (!client.sendResponse())
		return disconnect(client);
    client.updateLastActivity();

    size_t total = client.getResponse().getRaw().length() + client.getResponse().getBody().length();
	if (client.getResponse().getSendOffset() >= total)
	{
		bool keepAlive = (client.getResponse().getHeaders()["Connection"] == "keep-alive");
		client.getRequest().clear();
		client.getResponse().clear();
		if (!keepAlive)
			return disconnect(client);
	}
	updatePoll(client.getFd(), POLLIN);
    return true;
}

bool Cluster::CgiWriteProcess(Client &client) {
    if (!streamCgiBuffer(client))
        return true;
    client.updateLastActivity();
    if (client.getCgi()->getCgiPipeEnded())
	{
    	char drain[4096];
    	int bytes_read = recv(client.getFd(), drain, sizeof(drain), 0);
   		if (bytes_read <= 0)
    		return disconnect(client);
	}
    updatePoll(client.getFd(), 0);                 // bascule sur le read pipe pour la suite
    updatePoll(client.getCgi()->getReadPipe(), POLLIN);
    return true;
}

bool Cluster::streamCgiBuffer(Client &client) {
    std::string &buf = client.getCgi()->getClientBuffer();
    if (buf.empty())
        return true;
    ssize_t sent = send(client.getFd(), buf.c_str(), buf.size(), MSG_NOSIGNAL);
    if (sent <= 0)
        return false;
    buf.erase(0, sent);
    return buf.empty();
}