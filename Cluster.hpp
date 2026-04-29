#ifndef CLUSTER_HPP
#define CLUSTER_HPP

#include "Webserv.hpp"
#include "./client/Client.hpp"
#include "./server/Server.hpp"
#include <sys/wait.h>


class Cluster {
	public:
		Cluster();
		Cluster(Cluster const &copy);
		~Cluster();

		Cluster& 		operator=(Cluster const &assignment);

		void 			serv();
		void			config(std::string &configFile);
	private:
		bool 			readRequest(Client &client);
		bool 			sendResponse(Client &client);
		Client*			findClient(int clientFd);
		Server*			findServer(int serverFd);
		void			addClient(Server &server);
		void 			disconnectTimedOut();
		void 			disconnect(Client &client);
		void 			addToPoll(int socketFd);
		void 			updatePoll(int clientFd, int event);
		bool			isServerFd(int fd);
		
		std::list<std::string>		_tokens;
		std::vector<Server>			_servers;
		std::vector<struct pollfd>	_pollFds;
		std::vector<Client>			_clients;
		std::map<int, Client*>		_cgis;
};

#endif
