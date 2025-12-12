/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: carlsanc <carlsanc@student.42madrid>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/03 17:15:15 by miaviles          #+#    #+#             */
/*   Updated: 2025/12/10 20:33:59 by carlsanc         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include "../client/ClientConnection.hpp"
#include "../client/User.hpp"
#include "../channel/Channel.hpp"
#include "../net/SocketUtils.hpp"
#include "../irc/Parser.hpp"

#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <algorithm>
#include <iostream>
#include <sys/socket.h>

//* ============================================================================
//* CONSTRUCTOR Y DESTRUCTOR
//* ============================================================================

Server::Server(int port, const std::string& password) : port_(port), 
	password_(password), server_fd_(-1), running_(false)
{
	initCommands();
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
		// No incrementamos 'i' automáticamente en el for loop.
        // Solo incrementamos si NO borramos el cliente actual.
        // Si borramos (handleClientEvent devuelve false), el vector se desplaza
        // y el siguiente elemento ocupa la posición actual 'i'.
		for (size_t i = 0; i < poll_fds_.size(); /* vacío */)
        {
            // Caso 1: Server Socket (Nuevas conexiones)
            if (poll_fds_[i].fd == server_fd_)
            {
                if (poll_fds_[i].revents & POLLIN)
                    acceptNewConnections();
                i++; // El server socket nunca se borra aquí
            }
            // Caso 2: Client Socket
            else
            {
                // Si retorna false, el cliente fue borrado y 'poll_fds_' se redujo.
                // No incrementamos 'i' porque el siguiente cliente ahora está en 'i'.
                if (!handleClientEvent(i))
                    continue; 
                
                i++; // Cliente sigue vivo, pasamos al siguiente
            }
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

//* HANDLE CLIENT EVENT
//* Core event handler that processes all socket activity for connected clients.
//* Called by main loop when poll() detects activity on a client socket.
//* Handles three main scenarios:
//* 1. Socket errors/disconnections (POLLERR, POLLHUP, POLLNVAL)
//* 2. Incoming data ready to read (POLLIN)
//* 3. Socket ready for writing (POLLOUT)
//* Manages the complete client I/O lifecycle: receive -> buffer -> parse -> respond

bool Server::handleClientEvent(size_t poll_index)
{
    int fd = poll_fds_[poll_index].fd;
    short revents = poll_fds_[poll_index].revents;
    ClientConnection* client = findClientByFd(fd);

    // Seguridad: Si no encontramos el objeto cliente pero está en poll, lo sacamos
    if (!client)
    {
        // Eliminar directamente del poll para evitar bucles infinitos
        poll_fds_.erase(poll_fds_.begin() + poll_index);
        close(fd);
        return false; // Cliente eliminado
    }

    // 1. GESTIÓN DE ERRORES DE POLL
    if (revents & (POLLERR | POLLHUP | POLLNVAL))
    {
        std::cout << "[SERVER] Client fd=" << fd << " disconnected (POLLHUP/ERR)" << std::endl;
        
        // [FIX RACE] Procesar datos pendientes antes de desconectar
        if (client->hasCompleteLine()) {
            processClientCommands(client);
        }
        
        disconnectClient(poll_index);
        return false; // Cliente eliminado
    }

    // 2. LECTURA (POLLIN)
    if (revents & POLLIN)
    {
        char buffer[4096];
        ssize_t bytes = recv(fd, buffer, sizeof(buffer) - 1, 0);

        if (bytes > 0)
        {
            buffer[bytes] = '\0';
            
            // Debug: Escapar caracteres especiales para log legible
            std::string displayData(buffer, bytes);
            for (size_t i = 0; i < displayData.length(); ++i) {
                if (displayData[i] == '\r') {
                    displayData.replace(i, 1, "\\r");
                    i++; // Saltar el carácter añadido
                } else if (displayData[i] == '\n') {
                    displayData.replace(i, 1, "\\n");
                    i++;
                }
            }
            
            std::cout << "[RECV] fd=" << fd << " (" << bytes << " bytes): " 
                      << displayData << std::endl;
            
            client->appendRecvData(std::string(buffer, bytes));
            client->updateActivity();
            processClientCommands(client);
            
            // [FIX ZOMBIE] Verificar si un comando (ej: QUIT) marcó la conexión para cierre
            // CRÍTICO: Debe estar INMEDIATAMENTE después de processClientCommands()
            if (client->isClosed())
            {
                disconnectClient(poll_index);
                return false; // Cliente eliminado
            }
        }
        else if (bytes == 0) // Conexión cerrada por el par
        {
            // [FIX RACE] Procesar buffer antes de desconectar
            std::cout << "[SERVER] Client fd=" << fd << " closed connection" << std::endl;
            
            // Si hay datos pendientes en el buffer, procesarlos primero
            if (client->hasCompleteLine()) {
                std::cout << "[SERVER] Processing pending commands before disconnect..." << std::endl;
                processClientCommands(client);
            }
            
            disconnectClient(poll_index);
            return false; // Cliente eliminado
        }
        else // Error en recv
        {
            if (errno != EAGAIN && errno != EWOULDBLOCK)
            {
                std::cerr << "[SERVER] recv() error on fd=" << fd << ": " << strerror(errno) << std::endl;
                disconnectClient(poll_index);
                return false; // Cliente eliminado
            }
        }
    }

    // 3. ESCRITURA (POLLOUT)
    // Solo intentamos escribir si el socket está listo Y hay datos pendientes
    if ((revents & POLLOUT) && client->hasPendingSend())
    {
        sendPendingData(client);
        
        // Verificar de nuevo si hubo error fatal durante el envío
        if (client->isClosed())
        {
            disconnectClient(poll_index);
            return false;
        }
    }

    // [ACTUALIZAR POLL EVENTS]
    // Si el cliente sigue vivo, actualizamos qué eventos nos interesan.
    // Siempre POLLIN. Solo POLLOUT si hay datos en el buffer de salida.
    if (client->hasPendingSend())
        poll_fds_[poll_index].events = POLLIN | POLLOUT;
    else
        poll_fds_[poll_index].events = POLLIN;

    return true; // Cliente sigue vivo
}

//* ============================================================================
//* DISCONNECT CLIENT
//* ============================================================================


void Server::disconnectClient(size_t poll_index)
{
    // 1. Obtener información básica antes de borrar nada
    int fd = poll_fds_[poll_index].fd;
    ClientConnection* client = findClientByFd(fd);

    std::cout << "[SERVER] Disconnecting client fd=" << fd << std::endl;

    // 2. Si el cliente existe, limpiar lógica de IRC y objetos
    if (client)
    {
        User* user = client->getUser();
        if (user)
        {
            // A. LIMPIEZA DE CANALES
            // Hacemos una COPIA del vector de canales porque vamos a modificar
            std::vector<Channel*> userChannels = user->getChannels();

            for (std::vector<Channel*>::iterator it = userChannels.begin(); it != userChannels.end(); ++it)
            {
                Channel* channel = *it;

                // 1. Notificar a los demás (QUIT message)
                std::string quitMsg = ":" + user->getPrefix() + " QUIT :Connection closed\r\n";
                channel->broadcast(quitMsg, user);

                // 2. Eliminar al usuario del canal
                channel->removeMember(user);

                // 3. Gestionar canales vacíos (Evitar fugas de memoria en canales)
                if (channel->getUserCount() == 0)
                {
                    // Buscar y borrar el canal de la lista global del servidor
                    for (std::vector<Channel*>::iterator chanIt = channels_.begin(); chanIt != channels_.end(); ++chanIt)
                    {
                        if (*chanIt == channel)
                        {
                            delete *chanIt;
                            channels_.erase(chanIt);
                            break; 
                        }
                    }
                }
            }
        }

        // B. ELIMINAR DE LA LISTA DE CLIENTES DEL SERVIDOR
        // (Usamos un bucle manual para encontrar y borrar el puntero en el vector)
        for (std::vector<ClientConnection*>::iterator it = clients_.begin(); it != clients_.end(); ++it)
        {
            if (*it == client)
            {
                clients_.erase(it);
                break;
            }
        }

        // C. CERRAR SOCKET Y LIBERAR MEMORIA
        close(fd);
        if (user)
            delete user; // El User debe borrarse manualmente
        delete client;   // Borramos la conexión
    }
    else
    {
        // Si no encontramos el objeto cliente, cerramos el fd por seguridad
        close(fd);
    }

    // 3. ACTUALIZAR POLL_FDS
    // Eliminamos el fd del vector de monitoreo.
    if (poll_index < poll_fds_.size())
        poll_fds_.erase(poll_fds_.begin() + poll_index);
}

//* ============================================================================
//* COMMAND PROCESSING
//* ============================================================================

void Server::processClientCommands(ClientConnection* client)
{
    // Procesamos TODAS las líneas completas que haya en el buffer
    // (Importante por si llegaron varios comandos pegados)
    while (client->hasCompleteLine())
    {
        std::string rawLine = client->popLine();
        
        // Debug opcional
        // std::cout << "[DEBUG] < " << rawLine << std::endl;

        // 1. Parseamos la línea
        Message msg = Parser::parse(rawLine);

        // 2. Si el comando está vacío (línea en blanco o solo espacios), ignoramos
        if (msg.command.empty())
            continue;

        // 3. Buscamos el comando en el mapa
        std::map<std::string, CommandHandler>::iterator it = _commandMap.find(msg.command);

        if (it != _commandMap.end())
        {
            // Encontrado = Ejecutamos la función asociada
            (this->*(it->second))(client, msg);
        }
        else
        {
            // COMANDO NO ENCONTRADO
            // Deberia enviar ERR_UNKNOWNCOMMAND (421)
            // Por ahora, un log simple:
            std::cerr << "[SERVER] Unknown command: " << msg.command << std::endl;
        }
    }
}

void Server::sendPendingData(ClientConnection* client)
{
    const std::string& data = client->getSendBuffer();
    if (data.empty()) return;

    // Enviar datos usando SocketUtils o send directamente
    ssize_t bytesSent = send(client->getFd(), data.c_str(), data.length(), 0);

    if (bytesSent > 0)
    {
        // Limpiar del buffer los bytes que ya se enviaron
        client->clearSentData(bytesSent);
    }
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

void Server::initCommands()
{
    // Mapeamos el string del comando a la función miembro correspondiente
    _commandMap["PASS"] = &Server::cmdPass;
    _commandMap["NICK"] = &Server::cmdNick;
    _commandMap["USER"] = &Server::cmdUser;
    _commandMap["PING"] = &Server::cmdPing;
    _commandMap["PONG"] = &Server::cmdPong;
    _commandMap["QUIT"] = &Server::cmdQuit;
    _commandMap["JOIN"] = &Server::cmdJoin;
    _commandMap["PART"] = &Server::cmdPart;
    _commandMap["PRIVMSG"] = &Server::cmdPrivMsg;
    _commandMap["NOTICE"] = &Server::cmdNotice;
    _commandMap["KICK"] = &Server::cmdKick;
    _commandMap["INVITE"] = &Server::cmdInvite;
    _commandMap["TOPIC"] = &Server::cmdTopic;
    _commandMap["MODE"] = &Server::cmdMode;
    
    // El Parser ya se encarga de poner el comando en mayúsculas
}