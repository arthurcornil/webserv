#include "Cluster.hpp"

int main(int ac, char **av)
{
	Cluster webserv;

	if (ac > 2)
		return std::cerr << "Error: Too many arguments" << std::endl, -1;
	try {
		std::string configPath = ac == 2 ? av[1] : "default.conf";
		webserv.config(configPath);
		webserv.serv();
	} catch (std::exception &e) {
		return std::cerr << "Error:" << e.what() << std::endl, -1;
	}
}
