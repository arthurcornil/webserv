#ifndef CLUSTER_HPP
#define CLUSTER_HPP

#include "../Webserv.hpp"
#include "../client/Client.hpp"
#include "../server/Server.hpp"
#include <sys/wait.h>

enum CgiReadState { CGI_MORE, CGI_EOF, CGI_ERROR };

class Cluster {
	public:
		Cluster();
		Cluster(Cluster const &copy);
		~Cluster();

		Cluster& 		operator=(Cluster const &assignment);

		void			config(std::string &configFile);
		void 			serv();

	private:
		void 			disconnectUnactives();
		bool 			disconnect(Client &client);

		void 			addToPoll(int socketFd);
		void 			updatePoll(int clientFd, int event);
		void			addClient(Server &server);
		Client*			findClient(int clientFd);
		Server*			findServer(int serverFd);
		Client*			findCgi(int fd);

		void			dispatchEvents(ssize_t &readyCount);
		bool			handleClientEvent(size_t &i);
		bool			handleCgiEvent(size_t &i);
		void			handleServerEvent(size_t &i);

		bool			readFromClient(Client &client);
		bool			writeToClient(Client &client);
		void			addCgiPipeToPoll(Client &client);
		bool			CgiWriteProcess(Client &client);
		bool			streamCgiBuffer(Client &client);

		bool			removeFromPoll(int fd);
		bool			removeCgiFd(int fd);

		bool			writeToPipe(size_t &i, Client *client);
		bool			readFromBodyTmp(size_t &i, Client *client);
		bool			readFromPipe(size_t &i, Client *client);

		int				readCgiChunk(Client *client, Cgi *cgi);
		bool			sendCgiHeaders(Client *client, Cgi *cgi, char *buffer, ssize_t bytesRead);
		bool			finishCgi(size_t &i, Client *client, Cgi *cgi);

		std::list<std::string>		_tokens;
		std::vector<Server>			_servers;
		std::vector<struct pollfd>	_pollFds;
		std::list<Client>			_clients;
		std::map<int, Client*>		_cgis;
};

#endif
