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
_lastActivity(std::time(NULL)), _user(NULL)
{
}

ClientConnection::~ClientConnection()
{
	//? Don't delete _user (managed by Server)
}

// ========================================================================
// 							 	  Socket Info
// ========================================================================

int ClientConnection::getFd() const
{
	return _fd;
}