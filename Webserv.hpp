#ifndef WEBSERV_HPP
#define WEBSERV_HPP

#define BUFFER_SIZE (64 * 1024)
#define TIMEOUT 10
#define MAX_HEADER_SIZE 8192
#define MAX_BODY_SIZE_GO 4
#define LARGE_BODY_THRESHOLD (1 * 1024 * 1024)
#define PATH_MAX 4096


#define BOLD_ORANGE  "\033[1;33m"  // client
#define BOLD_CYAN    "\033[1;36m"  // request
#define REQ_KEY      "\033[0;34m"  // bleu
#define BOLD_GREEN   "\033[1;32m"  // Config
#define BOLD_MAGENTA "\033[1;35m"  // locations
#define BOLD_PINK    "\033[1;95m"  // config :p
// GENERAL
#define RESET       "\033[0m"
#define BOLD        "\033[1m"
#define DIM         "\033[2m"

#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <list>
#include <vector>
#include <map>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <algorithm>
#include <signal.h>

//UTILS

void        			removeSpacesAround(std::string &str);
bool       				isSpacesInside(std::string &str);
void       				addSpaces(std::string &line, char c);
std::string 			lexer(std::string &file);
std::list<std::string>	tokenizer(std::string &buffer);
void					ipIsValid(std::string ip);
std::string				sizetostr(int num);
long					ft_strhexatoul(std::string str);
double					now();

template <typename T>
std::string toString(T n) {
    std::ostringstream ss;
    ss << n;
    return ss.str();
}

#endif
