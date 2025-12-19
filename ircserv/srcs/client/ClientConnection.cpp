/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ClientConnection.cpp                               :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rmunoz-c <rmunoz-c@student.42.fr>          #+#  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025-12-03 17:49:07 by rmunoz-c          #+#    #+#             */
/*   Updated: 2025-12-03 17:49:07 by rmunoz-c         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ClientConnection.hpp"
#include <ctime>

ClientConnection::ClientConnection(int fd): _fd(fd), _recvBuffer(""),
_sendBuffer(""), _registered(false), _hasSentPass(false), _closed(false),
_lastActivity(std::time(NULL)), _connectTime(std::time(NULL)), _user(NULL)
{
}

ClientConnection::~ClientConnection()
{
	//? Don't delete _user (managed by Server)
}

// ========================================================================
// 							 Connection State
// ========================================================================

bool ClientConnection::isRegistered() const
{
	return _registered;
}

void ClientConnection::setRegistered(bool r)
{
	_registered = r;
}

void ClientConnection::markPassReceived()
{
	_hasSentPass = true;
}

bool ClientConnection::hasSentPass() const
{
	return _hasSentPass;
}

// ========================================================================
// 							 	  Socket Info
// ========================================================================

int ClientConnection::getFd() const
{
	return _fd;
}

// ========================================================================
// 							  IO Operations
// ========================================================================

void ClientConnection::appendRecvData(const std::string& data)
{
	_recvBuffer += data; 
}

bool ClientConnection::hasCompleteLine() const
{
	// Aceptar tanto \r\n (IRC estándar) como \n (telnet/nc)
	return (_recvBuffer.find("\r\n") != std::string::npos || 
	        _recvBuffer.find("\n") != std::string::npos);
}

std::string ClientConnection::popLine()
{
	std::string line;
	
	// Buscar primero \r\n (protocolo IRC estándar)
	size_t pos = _recvBuffer.find("\r\n");
	
	if (pos != std::string::npos) 
	{
		line = _recvBuffer.substr(0, pos);
		_recvBuffer.erase(0, pos + 2); // Eliminar línea + \r\n
	}
	else 
	{
		// Fallback: buscar solo \n (telnet, netcat sin -C)
		pos = _recvBuffer.find("\n");
		if (pos != std::string::npos) 
		{
			line = _recvBuffer.substr(0, pos);
			_recvBuffer.erase(0, pos + 1); // Eliminar línea + \n
			
			// Limpiar posible \r residual al final
			if (!line.empty() && line[line.length() - 1] == '\r')
				line.erase(line.length() - 1);
		}
	}
	
	return line;
}

void ClientConnection::queueSend(const std::string& data)
{
	_sendBuffer += data;
}

bool ClientConnection::hasPendingSend() const
{
	return !_sendBuffer.empty();
}

const std::string& ClientConnection::getSendBuffer() const
{
	return _sendBuffer;
}

void ClientConnection::clearSentData(size_t bytes)
{
	_sendBuffer.erase(0, bytes);
}

// ========================================================================
// 							Activity Tracking
// ========================================================================

void ClientConnection::updateActivity()
{
	_lastActivity = std::time(NULL);
}

time_t ClientConnection::getLastActivity() const
{
	return _lastActivity;
}

time_t ClientConnection::getConnectTime() const
{
    return _connectTime;
}

// ========================================================================
// 						  Connection Management
// ========================================================================

void ClientConnection::closeConnection()
{
	_closed = true;
}

bool ClientConnection::isClosed() const
{
	return _closed;
}

// ========================================================================
// 						   User Association
// ========================================================================

void ClientConnection::setUser(User* user)
{
	_user = user;
}

User* ClientConnection::getUser() const
{
	return _user;
}