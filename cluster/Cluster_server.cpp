#include "Cluster.hpp"

void Cluster::handleServerEvent(size_t &i) {
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
		Server* server = findServer(_pollFds[i].fd);
		if (server)
		{
			std::cerr << "Error on server FD " << _pollFds[i].fd << ": " << std::strerror(errno) << " — restarting..." << std::endl;
			close(_pollFds[i].fd);
			_pollFds.erase(_pollFds.begin() + i);
			server->setUpListenSocket();
			addToPoll(server->getListenSocket());
		}
	}
}