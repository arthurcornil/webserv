*This project has been created as part of the 42 curriculum by sabansac, arcornil.*

# Webserv

## Description

Webserv is a networking project where the goal is to develop a fully working, HTTP/1.1-compliant web server in C++98. The server has to be non-blocking, support multiple servers/ports via the config and CGI.

## Instructions

### Requirements

You will need a `.conf` file, describing the configuration of the server. A default `default.conf` file is provided.

### Compilation

```bash
make
```

### Execution

```bash
./webserv [CONFIG FILE]
```

> [!NOTE]
> If no argument is given, webserv will try and open a `default.conf` file and use it as config.

## Resources

### References

- [RFC 9112](https://www.rfc-editor.org/rfc/rfc9112.html) - The Bible of the HTTP protocol
- [RFC 3875](https://datatracker.ietf.org/doc/html/rfc3875) - The CGI/1.1 specification
- [NGINX](https://github.com/nginx/nginx) - The source code of NGINX, a trusted and widely used web server

### AI Usage

AI tools (Anthropic's Sonnet 4.6) were used during this project for the following tasks:

- **Upload & DELETE method webpage** - Generated a webpage in order to have a practical interface to test out the Upload and DELETE method features.
- **Debug / Streaming** - Used LLM as assistance in order to debug streaming issues
- **Theoretical explanation** - Get a deeper and clearer understanding of the project's requirements
