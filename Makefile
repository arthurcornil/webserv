NAME =		webserv
CXX =		c++
CXXFLAGS =	-Wall -Wextra -Werror -std=c++98 -g -O3
SRCS =		main.cpp \
			utils.cpp \
			cluster/Cluster.cpp \
			cluster/Cluster_poll.cpp \
			cluster/Cluster_client.cpp \
			cluster/Cluster_cgi.cpp \
			cluster/Cluster_server.cpp \
			server/Server.cpp \
			server/Location.cpp \
			client/Client.cpp \
			client/HttpRequest.cpp \
			client/HttpResponse.cpp \
			client/Cgi.cpp
OBJDIR =	obj/
OBJS =		$(SRCS:%.cpp=$(OBJDIR)%.o)

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $@
	@echo "🚀 Houston, we have a $(NAME)!"

$(OBJDIR)%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean: 
	rm -rf $(OBJDIR)
	@echo "🧹 Cleaning up the mess!"

fclean: clean 
	rm -f $(NAME)
	@echo "🧨 Obliterating all traces!"

re: fclean all

.PHONY:  all clean fclean re
