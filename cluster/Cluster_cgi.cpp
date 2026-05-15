#include "Cluster.hpp"

bool Cluster::handleCgiEvent(size_t &i) {
    Client *client = findCgi(_pollFds[i].fd);
    Cgi *cgi = client->getCgi();
    int fd = _pollFds[i].fd;

    if (fd == cgi->getWritePipe())
        return writeToPipe(i, client);
    if (fd == cgi->getReadPipe())
        return readFromPipe(i, client);
    if (fd == cgi->getBodyTmpFd())
        return readFromBodyTmp(i, client);
    return true;
}

bool Cluster::removeCgiFd(int fd) {
    close(fd);
    _cgis.erase(fd);
    return removeFromPoll(fd);
}

bool Cluster::readFromBodyTmp(size_t &i, Client *client) {
    Cgi *cgi = client->getCgi();
    if (_pollFds[i].revents & (POLLERR | POLLNVAL)) {
        int fd = _pollFds[i].fd;
        cgi->setBodyTmpDone(true);
        cgi->setBodyTmpFd(-1);
        return removeCgiFd(fd);
    }
    if (!(_pollFds[i].revents & POLLIN))
        return true;
    char buf[BUFFER_SIZE];
    ssize_t bytesRead = read(_pollFds[i].fd, buf, sizeof(buf));
    if (bytesRead <= 0)
	{
        int fd = _pollFds[i].fd;
        cgi->setBodyTmpDone(true);
        cgi->setBodyTmpFd(-1);
        return removeCgiFd(fd);
    }
    cgi->getWriteBuffer().append(buf, bytesRead);
    return true;
}

bool Cluster::writeToPipe(size_t &i, Client *client) {
    Cgi *cgi = client->getCgi();
    if (_pollFds[i].revents & (POLLERR | POLLHUP | POLLNVAL))
        return removeCgiFd(_pollFds[i].fd);
    if (!(_pollFds[i].revents & POLLOUT))
        return true;
    if (cgi->getWriteBuffer().empty())
	{
        if (cgi->getBodyTmpDone())
            return removeCgiFd(_pollFds[i].fd);
        return true;
    }
    ssize_t written = write(_pollFds[i].fd, cgi->getWriteBuffer().c_str(), cgi->getWriteBuffer().size());
    if (written <= 0)
        return removeCgiFd(_pollFds[i].fd);
    cgi->getWriteBuffer().erase(0, written);
    client->updateLastActivity();
    return true;
}

bool Cluster::readFromPipe(size_t &i, Client *client) {
    Cgi *cgi = client->getCgi();
    bool pipeEnded = false;
    if ((_pollFds[i].revents & (POLLIN | POLLHUP)) && cgi->getClientBuffer().empty())
	{
        int state = readCgiChunk(client, cgi);
        if (state == CGI_EOF)
            pipeEnded = true;
        else if (state == CGI_ERROR)
		{
            removeCgiFd(_pollFds[i].fd);
            return disconnect(*client);
        }
    }
    bool isHangupOrError = _pollFds[i].revents & (POLLHUP | POLLERR | POLLNVAL);
    if (pipeEnded || (isHangupOrError && cgi->getClientBuffer().empty()))
        return finishCgi(i, client, cgi);
    if (isHangupOrError)
        updatePoll(client->getFd(), POLLOUT);
    return true;
}

int Cluster::readCgiChunk(Client *client, Cgi *cgi) {
    char buffer[BUFFER_SIZE];
    ssize_t bytesRead = read(cgi->getReadPipe(), buffer, sizeof(buffer));
    if (bytesRead == 0)
        return CGI_EOF;
    if (bytesRead < 0)
        return CGI_ERROR;
    if (!cgi->getHeadersSent()) {
        if (!sendCgiHeaders(client, cgi, buffer, bytesRead))
            return CGI_ERROR;
    } else {
        cgi->getClientBuffer().append(buffer, bytesRead);
        if (!streamCgiBuffer(*client))
            updatePoll(client->getFd(), POLLOUT);
        client->updateLastActivity();
    }
    return CGI_MORE;
}

bool Cluster::sendCgiHeaders(Client *client, Cgi *cgi, char *buffer, ssize_t bytesRead) {
    cgi->appendToBuffer(buffer, bytesRead);
    size_t bodyStart = client->getResponse().buildCgiHeaders(cgi->getBuffer(), *client);
    if (bodyStart == std::string::npos)
        return true;

    const std::string &raw = client->getResponse().getRaw();
    ssize_t sent = send(client->getFd(), raw.c_str(), raw.length(), MSG_NOSIGNAL);
    if (sent <= 0)
        return false;

    std::string &buf = cgi->getBuffer();
    if (bodyStart < buf.size())
        cgi->getClientBuffer().append(buf.c_str() + bodyStart, buf.size() - bodyStart);
    buf.clear();
    cgi->setHeadersSent(true);
    if (!streamCgiBuffer(*client))
        updatePoll(client->getFd(), POLLOUT);
    client->updateLastActivity();
    return true;
}

bool Cluster::finishCgi(size_t &i, Client *client, Cgi *cgi) {
	int status;
    waitpid(cgi->getPid(), &status, WNOHANG);
    removeCgiFd(_pollFds[i].fd);
    if (!cgi->getHeadersSent())
        client->getResponse().setCgiResponse(cgi->getBuffer(), *client);
	else
        cgi->setCgiPipeEnded(true);
    updatePoll(client->getFd(), POLLOUT);
    return false;
}