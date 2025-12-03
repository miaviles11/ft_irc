/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: miaviles <miaviles@student.42madrid>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/03 15:55:08 by miaviles          #+#    #+#             */
/*   Updated: 2025/12/03 18:07:44 by miaviles         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <vector>
#include <poll.h>

class ClientConnection;
class Channel;

class Server {
	public:
		Server(int port, const std::string& password);
		~Server();

		//* NOT ALLOWED COPIES
		Server(const Server&);
    	Server& operator=(const Server&);

		bool start(); //* Create Socket, bind, listen
		void run(); //* Loop poll()/select()

	
		const std::string& getPassword() const { return password_; } //* GETTER

	private:
		//* CONFIGURATION
		int port_;
		std::string password_;
		int server_fd_; //* FD OF THE SERVER'S SOCKET

		//* COLLECTIONS
		std::vector<ClientConnection*> clients_; //* STORAGE THE LIST OF CLIENTS
		std::vector<Channel*> channels_; //* STORAGE THE LIST OF CHANNELS
		std::vector<struct pollfd> poll_fds_; //* POOLS FUCTION

		//* PRIVATE METHODS
		//* INITIALIZATION
		bool createSocket();
		bool bindSocket();
		bool listenSocket();
		bool setNonBlocking(int fd);

		//* CONECTION MANAGEMENT
		void acceptNewConnections();
		void handleClientData(int client_index);
		void disconnectClient(int client_index);

		//* UTILS
		void updatePollFds();
		ClientConnection* findClientByFd(int fd);
};


#endif