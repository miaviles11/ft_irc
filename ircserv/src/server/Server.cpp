/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: miaviles <miaviles@student.42madrid>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/03 17:15:15 by miaviles          #+#    #+#             */
/*   Updated: 2025/12/09 16:54:38 by miaviles         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include "../client/ClientConnection.hpp"
#include "../client/User.hpp"
#include "../channel/Channel.hpp"
#include "../net/SocketUtils.hpp"

#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <sys/socket.h>

//* ============================================================================
//* CONSTRUCTOR Y DESTRUCTOR
//* ============================================================================

Server::Server(int port, const std::string& password) : port_(port), 
	password_(password), server_fd_(-1), running_(false)
{
    std::cout << "[SERVER] Initializing on port " << port << std::endl;	
}

Server::~Server()
{
    std::cout << "[SERVER] Shutting down..." << std::endl;

	//* CLOSE SERVER SOCKET
	if (server_fd_ >= 0)
		close(server_fd_);

	//* CLEANUP CLIENTS
	for (size_t i = 0; i < clients_.size(); i++)
	{
		if (clients_[i])
		{
			User* user = clients_[i]->getUser();		//* Get associated User before deleting connection

			close(clients_[i]->getFd());
			delete clients_[i];							//* Delete ClientConnection
			
			if (user)								    //* Delete User if exists
				delete user;
		}
	}

	//* CLEANUP CHANNELS
	for (size_t i = 0; i < channels_.size(); ++i)
		delete channels_[i];
}

//* ============================================================================
//* SETUP - Uses SocketUtils for all socket operations
//* ============================================================================

bool Server::setupServerSocket()
{
	server_fd_ = SocketUtils::createServerSocket();               //* Create a non-blocking TCP socket for the server
	if (server_fd_ < 0)
		return (false);
	
	if (!SocketUtils::bindSocket(server_fd_, port_))              //* Bind socket to specified port (associates socket with network address)
	{
		close(server_fd_);
		server_fd_ = -1;
		return (false);
	}

	if (!SocketUtils::listenSocket(server_fd_, SOMAXCONN))        //* Mark socket as passive (ready to accept connections), SOMAXCONN = max queue size
	{
		close(server_fd_);
		server_fd_ = 1;
		return (false);
	}
	return (true);
}

bool Server::start()
{
	std::cout << "[SERVER] Starting..." << std::endl;

	if (!setupServerSocket())
		return (false);
	
	//* ADD SERVER SOCKET TO POLL
	struct pollfd server_pfd;              //* POSIX structure to monitor file descriptors for I/O events
	server_pfd.fd = server_fd_;            //* Tell poll() which socket to monitor (server's listening socket)
	server_pfd.events = POLLIN;            //* Register interest in read events (POLLIN = data available to read = new connection ready)
	server_pfd.revents = 0;                //* Clear "returned events" field (poll() fills this with actual events that occurred)
	poll_fds_.push_back(server_pfd);       //* Add to vector so poll() can monitor server socket + all client sockets together (MULTIPLE USERS!)
	
	running_ = true;
	std::cout << "[SERVER] ✓ Ready on port " << port_ << std::endl;
	return (true);
}

void Server::stop()
{
    running_ = false;
}

//* ============================================================================
//* MAIN LOOP - SINGLE poll() (required by 42)
//* ============================================================================

void Server::run()
{
    std::cout << "[SERVER] Main loop started" << std::endl;

	while (running_)
	{
		//* WAIT FOR ACTIVITY on any socket (server + all clients)
		//* poll() with "-1" blocks here until something happens
		//* Returns: number of sockets with activity, or -1 on error
		int poll_count = poll(&poll_fds_[0], poll_fds_.size(), -1);
		
		//* HANDLE POLL ERRORS
		if (poll_count < 0)
		{
			if (errno == EINTR)              //* Interrupted by signal (e.g., Ctrl+C) - not fatal
				continue;                     //* Restart poll() loop
			std::cerr << "[ERROR] poll() failed" << std::endl; //* If it is other error...
			break;                            //* Fatal error - exit loop
		}
		
		//* CHECK EACH SOCKET for activity
		for (size_t i = 0; i < poll_fds_.size(); ++i)
		{
			//* SKIP if no events on this socket
			if (poll_fds_[i].revents == 0)
				continue;
			
			//* CASE 1: Activity on SERVER SOCKET (new connection incoming)
			if (poll_fds_[i].fd == server_fd_)
			{
				if (poll_fds_[i].revents & POLLIN)    //* Data ready to read = new client waiting
					acceptNewConnections();            //* Accept the new client
			}
			//* CASE 2: Activity on CLIENT SOCKET (existing client sent data)
			else
				handleClientEvent(i);                  //* Process client's message/task
		}
	}
	std::cout << "[SERVER] Main loop ended" << std::endl;
}

//* ============================================================================
//* GETTERS
//* ============================================================================

const std::string& Server::getPassword() const
{
	return (password_);
}

int Server::getClientCount() const
{
	return (clients_.size());
}

//* ============================================================================
//* ACCEPT - Uses SocketUtils
//* ============================================================================

//* ACCEPT NEW CONNECTIONS
//* Core function that handles incoming client connections to the IRC server.
//* Creates ClientConnection and User objects, links them together, and adds
//* the new client to both the clients_ vector and poll monitoring system.
//* Uses non-blocking socket operations to accept multiple pending connections.

void Server::acceptNewConnections()
{
	//* ACCEPT ALL PENDING CONNECTIONS in a loop (non-blocking)
	while (true)
	{
		std::string client_ip;                                             //* Will store client's IP address
		int client_fd = SocketUtils::acceptClient(server_fd_, client_ip);  //* Accept one connection, get client socket fd and IP

		//* BREAK if no more connections pending (non-blocking would return -1)
		if (client_fd < 0)
			break;

		//* CREATE CLIENT CONNECTION OBJECT (manages socket I/O and buffers)
		ClientConnection* connection = new ClientConnection(client_fd);

		//* CREATE USER OBJECT (stores IRC user data: nick, username, channels, etc.)
		User* user = new User();
		user->setHostname(client_ip);                                       //* Store client's IP address in user profile
		user->setConnection(connection);                                    //* Link User -> ClientConnection (bidirectional relationship)
		connection->setUser(user);                                          //* Link ClientConnection -> User

		//* REGISTER CLIENT in server's client list
		clients_.push_back(connection);                                     //* Add to vector for tracking all connected clients
		addClientToPoll(connection);                                        //* Add client's fd to poll_fds_ for I/O monitoring

		std::cout << "[SERVER] ✓ New client from " << client_ip 
				  << " (fd=" << client_fd << ", total=" << clients_.size() << ")" << std::endl;
	}
}

//* ============================================================================
//* HANDLE CLIENT EVENTS
//* ============================================================================

void Server::handleClientEvent(size_t poll_index)
{

}

//* ============================================================================
//* DISCONNECT CLIENT
//* ============================================================================


void Server::disconnectClient(size_t poll_index)
{

}

//* ============================================================================
//* COMMAND PROCESSING
//* ============================================================================

void Server::processClientCommands(ClientConnection* client)
{
	//TODO
	//-C- Process complete IRC command lines from client
	//-C-RESPONSIBILITIES (Parser/Commands team):
 	//TODO: Parser/Commands team - implement full IRC command processing
}

void Server::sendPendingData(ClientConnection* client)
{

}

//* ============================================================================
//* UTILITIES
//* ============================================================================

void Server::addClientToPoll(ClientConnection* client)
{
	struct pollfd pfd;
	pfd.fd = client->getFd();       //* File descriptor of the client socket to monitor
	pfd.events = POLLIN;            //* Register interest in read events (incoming data)
	pfd.revents = 0;                //* Clear returned events field (will be filled by poll())
	poll_fds_.push_back(pfd);       //* Add to poll array for monitoring
}

void Server::updatePollEvents(int fd, short events)
{
	for (size_t i = 0; i < poll_fds_.size(); ++i)
	{
		if (poll_fds_[i].fd == fd)
		{
			poll_fds_[i].events = events; // Found and updated
			break;
		}
	}
}

ClientConnection* Server::findClientByFd(int fd)
{
	for (size_t i = 0; i < clients_.size(); ++i) //* Just looking in the vector<ClientConnection*> if there is that client.
	{
		if (clients_[i] && clients_[i]->getFd() == fd) 
			return (clients_[i]);
	}
	return (NULL);
}
