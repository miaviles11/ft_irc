/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Channel.cpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: carlsanc <carlsanc@student.42madrid>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/10 19:16:51 by carlsanc          #+#    #+#             */
/*   Updated: 2025/12/10 19:16:51 by carlsanc         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Channel.hpp"
#include "../client/User.hpp"
#include "../client/ClientConnection.hpp"

// ========================================================================
// 							Constructor / Destructor
// ========================================================================

Channel::Channel(const std::string& name) : 
    _name(name), _topic(""), _key(""), _limit(0),
    _inviteOnly(false), _topicRestricted(true), _keyMode(false), _limitMode(false)
{
    // Por defecto +t está activo en muchos servidores, pero puedes cambiarlo a false si prefieres
}

Channel::~Channel()
{
    // El canal NO es dueño de los usuarios, así que no hacemos 'delete' de los punteros User*
    _members.clear();
    _operators.clear();
    _invites.clear();
}

// ========================================================================
// 							Basic Info
// ========================================================================

const std::string& Channel::getName() const
{
    return _name;
}

const std::string& Channel::getTopic() const
{
    return _topic;
}

void Channel::setTopic(const std::string& topic)
{
    _topic = topic;
}

// ========================================================================
// 							Modes: Key & Limit
// ========================================================================

const std::string& Channel::getKey() const
{
    return _key;
}

void Channel::setKey(const std::string& key)
{
    _key = key;
    _keyMode = !key.empty();
}

void Channel::setLimit(int limit)
{
    _limit = limit;
    _limitMode = (limit > 0);
}

int Channel::getLimit() const
{
    return _limit;
}

size_t Channel::getUserCount() const
{
    return _members.size();
}

// ========================================================================
// 							Modes: General Management
// ========================================================================

void Channel::setMode(char mode, bool active)
{
    switch (mode) {
        case 'i': _inviteOnly = active; break;
        case 't': _topicRestricted = active; break;
        case 'k': _keyMode = active; if (!active) _key = ""; break;
        case 'l': _limitMode = active; if (!active) _limit = 0; break;
    }
}

bool Channel::hasMode(char mode) const
{
    switch (mode) {
        case 'i': return _inviteOnly;
        case 't': return _topicRestricted;
        case 'k': return _keyMode;
        case 'l': return _limitMode;
        default: return false;
    }
}

std::string Channel::getModes() const
{
    std::string modes = "+";
    if (_inviteOnly) modes += "i";
    if (_topicRestricted) modes += "t";
    if (_keyMode) modes += "k";
    if (_limitMode) modes += "l";
    return modes;
}

// ========================================================================
// 							Membership Management
// ========================================================================

void Channel::addMember(User* user)
{
    if (!isMember(user))
        _members.push_back(user);
}

void Channel::removeMember(User* user)
{
    // C++98 Erase-Remove idiom manually implemented
    std::vector<User*>::iterator it = std::find(_members.begin(), _members.end(), user);
    if (it != _members.end())
        _members.erase(it);
    
    // Si era operador, quitarlo también
    removeOperator(user);
}

bool Channel::isMember(User* user) const
{
    return std::find(_members.begin(), _members.end(), user) != _members.end();
}

User* Channel::getMember(const std::string& nickname)
{
    for (size_t i = 0; i < _members.size(); ++i)
    {
        if (_members[i]->getNickname() == nickname)
            return _members[i];
    }
    return NULL;
}

// ========================================================================
// 							Operator Management
// ========================================================================

void Channel::addOperator(User* user)
{
    if (!isOperator(user))
        _operators.push_back(user);
}

void Channel::removeOperator(User* user)
{
    std::vector<User*>::iterator it = std::find(_operators.begin(), _operators.end(), user);
    if (it != _operators.end())
        _operators.erase(it);
}

bool Channel::isOperator(User* user) const
{
    return std::find(_operators.begin(), _operators.end(), user) != _operators.end();
}

// ========================================================================
// 							Invite List
// ========================================================================

void Channel::addInvite(const std::string& nickname)
{
    // Evitar duplicados
    if (std::find(_invites.begin(), _invites.end(), nickname) == _invites.end())
        _invites.push_back(nickname);
}

bool Channel::isInvited(User* user) const
{
    return std::find(_invites.begin(), _invites.end(), user->getNickname()) != _invites.end();
}

// ========================================================================
// 							Communication
// ========================================================================

void Channel::broadcast(const std::string& message, User* excludeUser)
{
    for (size_t i = 0; i < _members.size(); ++i)
    {
        User* target = _members[i];
        
        if (target == excludeUser)
            continue;
        
        if (target->getConnection())
            target->getConnection()->queueSend(message);
    }
}

// ========================================================================
// 							Utils
// ========================================================================

std::string Channel::getNamesList() const
{
    std::string list;
    
    for (size_t i = 0; i < _members.size(); ++i)
    {
        User* user = _members[i];
        if (i > 0) list += " ";
        
        if (isOperator(user))
            list += "@";
        
        list += user->getNickname();
    }
    return list;
}
