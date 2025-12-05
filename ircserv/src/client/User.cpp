/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   User.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rmunoz-c <rmunoz-c@student.42.fr>          #+#  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025-12-05 15:31:58 by rmunoz-c          #+#    #+#             */
/*   Updated: 2025-12-05 15:31:58 by rmunoz-c         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "User.hpp"

User::User() : _nickname(""), _username(""), _realname(""), _hostname(""),
_isOperator(false), _isInvisible(false), _isAway(false), _awayMessage(""),
_connection(NULL)
{
}

User::User(const std::string& nickname): _nickname(nickname), _username(""),
_realname(""), _hostname(""),_isOperator(false), _isInvisible(false),
_isAway(false), _awayMessage(""), _connection(NULL)
{
}

User::~User()
{
	//? Don't delete _connection (managed by Server)
    //? Don't delete channels (managed by Server)
}

// ========================================================================
// 							  Identity Getters
// ========================================================================

const std::string& User::getNickname() const
{
	return _nickname;
}

const std::string& User::getUsername()  const
{
	return _username;
}

const std::string& User::getRealname() const
{
	return _realname;
}

const std::string& User::getHostname() const
{
	return _hostname;
}

// ========================================================================
// 							  Identity Setters
// ========================================================================

void User::setNickname(const std::string& nick)
{
	_nickname = nick;
}

void User::setUsername(const std::string& user)
{
	_username = user;
}

void User::setRealname(const std::string& real)
{
	_realname = real;
}

void User::setHostname(const std::string& host)
{
	_hostname = host;
}
