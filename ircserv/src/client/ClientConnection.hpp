/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ClientConnection.hpp                               :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rmunoz-c <rmunoz-c@student.42.fr>          #+#  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025-12-03 15:44:16 by rmunoz-c          #+#    #+#             */
/*   Updated: 2025-12-03 15:44:16 by rmunoz-c         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CLIENT_CONNECTION_HPP
# define CLIENT_CONNECTION_HPP

#include <iostream>
#include <string>
#include <set>
#include <vector>
#include <ctime>

/* Fordward declarations */
class Server; 
class Channel;

/** 
 * -R- This is the client class. Its main function is to store the user's state,
 * -R- manage the input and output buffer, and provide the server with all the
 * -R- information necessary to processs commands and send responses.
**/

class ClientConnection
{
	public:
		ClientConnection(int fd);
		~ClientConnection();

		bool	isRegistered() const;
		
		/* Getters */
		int		getFd() const;
		const	std::string& getNickname() const;
		const	std::string& getUsername() const;
		const	std::string& getRealname() const;
		const	std::string& getHostname() const;

		/* Setters */
		void	setNickname(const std::string& nick);
		void	setUsername(const std::string& user);
		void	setRealname(const std::string& real);
		void	setHostname(const std::string& host);
		void	setRegistered(bool r);

		void	markPassReceived();

		/* IO helpers */
		void	appendRecvData(const std::string& data);
		bool	hasCompleteLine() const;
		void	queueSend(const std::string& data);
		bool	hasPendingSend() const;

		std::string	popLine();

		/* Channel gestor */
		void	joinChannel(const std::string& channelName);
		void	leaveChannel(const std::string& chaannelName);
		const	std::set<std::string>& getChannels() const;

		/* State methods */
		void	closeConnections();
		bool	isClosed() const;

	private:
		int	id = -1;						//*
		const int _fd;						//* Socket descriptor for this client

		std::string	_recvBuffer;			//* Buffer to accumulate incoming data
		std::string _sendBuffer;			//* Buffer for data pending to send
		std::string _nickname;				//* Client's nickname
		std::string _username;				//* Client's username
		std::string _realname;				//* Client's real name
		std::string _hostname;				//* Client's hostname

		bool _registered;					//* True if client completed PASS+NICK+USER
		bool _hasSentPass;					//* True if client sent PASS command
		bool _isOperator;					//* 

		std::set<std::string> _channels;	//* Names of channels the client has joined
		time_t _lastActivity;				//* Timestamp of last activity

		ClientConnection(const ClientConnection&);				//* Disabled copy constructor
		ClientConnection& operator=(const ClientConnection&);	//* Disabled assignment operator

};

#endif
