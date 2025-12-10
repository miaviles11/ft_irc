/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: carlsanc <carlsanc@student.42madrid>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/03 15:55:08 by miaviles          #+#    #+#             */
/*   Updated: 2025/12/10 19:14:16 by carlsanc         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <vector>
#include <poll.h>
#include <map>
#include "../irc/Message.hpp"

class ClientConnection;
class Channel;

/**
 * Server: IRC Server main coordinator
 * * Responsibilities:
 * - Main poll() loop (SINGLE poll as required by 42)
 * - ClientConnection lifecycle management
 * - Event routing to appropriate handlers
 * - Channel management
 * - Command execution coordination
 * * Uses:
 * - SocketUtils for low-level socket operations
 * - ClientConnection for connection + User state
 */

class Server {
	public:
		Server(int port, const std::string& password);
		~Server();

		//* MAIN CONTROLLERS
		bool start(); 								//* Create Socket, bind, listen
		void run(); 								//* Loop poll()/select()
		void stop();
	
		//* GETTERS
		const std::string& getPassword() const;
		int getClientCount() const;
		
	private:
		//* CONFIGURATION
		int port_;
		std::string password_;
		int server_fd_; 							//* FD OF THE SERVER'S SOCKET
		bool running_;

		//* COLLECTIONS
		std::vector<ClientConnection*> clients_; 	//* STORAGE THE LIST OF CLIENTS
		std::vector<Channel*> channels_; 			//* STORAGE THE LIST OF CHANNELS
		std::vector<struct pollfd> poll_fds_; 		//* POOLS FUCTION

		//* INITIALIZATION
		bool setupServerSocket();

		//* CONECTION MANAGEMENT
		void acceptNewConnections();
    	void handleClientEvent(size_t poll_index);
   		void disconnectClient(size_t poll_index);

		//* COMMAND PROCESSING (for later)
		void processClientCommands(ClientConnection* client);
		void sendPendingData(ClientConnection* client);
		
		//* UTILITIES
		void addClientToPoll(ClientConnection* client);
		void updatePollEvents(int fd, short events);
		ClientConnection* findClientByFd(int fd);

        //* CHANNEL MANAGEMENT HELPER FUNCTIONS (CRÍTICO: FALTABAN ESTOS)
        Channel* getChannel(const std::string& name);
        Channel* createChannel(const std::string& name);

		/*--------------------------------------------------------------------*/
        /* NUEVO: SISTEMA DE COMANDOS                                         */
        /*--------------------------------------------------------------------*/
        
        // 1. Definición del tipo de función para los comandos
        //    (Recibe el cliente que envió el mensaje y el mensaje parseado)
        typedef void (Server::*CommandHandler)(ClientConnection*, const Message&);

        // 2. Mapa para asociar strings ("JOIN") con funciones (&Server::cmdJoin)
        std::map<std::string, CommandHandler> _commandMap;

        // 3. Función para rellenar el mapa al inicio
        void initCommands();

		/*--------------------------------------------------------------------*/
        /* NUEVO: PROTOTIPOS DE LOS COMANDOS (Implementar en Commands.cpp)    */
        /*--------------------------------------------------------------------*/
        
        // Autenticación
        void cmdPass(ClientConnection* client, const Message& msg);
        void cmdNick(ClientConnection* client, const Message& msg);
        void cmdUser(ClientConnection* client, const Message& msg);
        void cmdPing(ClientConnection* client, const Message& msg);
        void cmdPong(ClientConnection* client, const Message& msg);
        void cmdQuit(ClientConnection* client, const Message& msg);

        // Canales y Comunicación
        void cmdJoin(ClientConnection* client, const Message& msg);
        void cmdPart(ClientConnection* client, const Message& msg);
        void cmdPrivMsg(ClientConnection* client, const Message& msg);
        void cmdNotice(ClientConnection* client, const Message& msg);

        // Operadores
        void cmdKick(ClientConnection* client, const Message& msg);
        void cmdInvite(ClientConnection* client, const Message& msg);
        void cmdTopic(ClientConnection* client, const Message& msg);
        void cmdMode(ClientConnection* client, const Message& msg);
	
		//* NON-COPYABLE
		Server(const Server&);
		Server& operator=(const Server&);
};

#endif
