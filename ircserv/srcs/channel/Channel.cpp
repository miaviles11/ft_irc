#include "Channel.hpp"
#include "../client/User.hpp"
#include "../client/ClientConnection.hpp"
#include <algorithm>
#include <iostream>
#include <cstdio>

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

Channel::Channel(const std::string& name) : 
    _name(name), _topic(""), _key(""), _limit(0),
    _inviteOnly(false), _topicOpOnly(false), _hasKey(false), _hasLimit(false)
{
}

Channel::~Channel()
{
    // No borramos los usuarios (User*), pertenecen al Server.
    // Solo limpiamos las listas.
    _members.clear();
    _operators.clear();
    _invites.clear();
}

// ============================================================================
// GETTERS BÁSICOS
// ============================================================================

const std::string& Channel::getName() const { return _name; }
const std::string& Channel::getTopic() const { return _topic; }
const std::string& Channel::getKey() const { return _key; }
size_t Channel::getUserCount() const { return _members.size(); }
int Channel::getLimit() const { return _limit; }

// ============================================================================
// MODOS
// ============================================================================

std::string Channel::getModes() const
{
    std::string modes = "+";
    if (_inviteOnly) modes += "i";
    if (_topicOpOnly) modes += "t";
    if (_hasKey) modes += "k";
    if (_hasLimit) modes += "l";
    
    // Añadir argumentos de modos (key y limit)
    if (_hasKey) modes += " " + _key;
    if (_hasLimit) {
        char buff[20];
        sprintf(buff, "%d", _limit);
        modes += " " + std::string(buff);
    }
    return modes;
}

bool Channel::hasMode(char mode) const
{
    if (mode == 'i') return _inviteOnly;
    if (mode == 't') return _topicOpOnly;
    if (mode == 'k') return _hasKey;
    if (mode == 'l') return _hasLimit;
    return false;
}

void Channel::setMode(char mode, bool active)
{
    if (mode == 'i') _inviteOnly = active;
    else if (mode == 't') _topicOpOnly = active;
    // k y l se gestionan con setKey y setLimit específicamente
}

void Channel::setKey(const std::string& key)
{
    if (key.empty()) {
        _hasKey = false;
        _key = "";
    } else {
        _hasKey = true;
        _key = key;
    }
}

void Channel::setLimit(int limit)
{
    if (limit <= 0) {
        _hasLimit = false;
        _limit = 0;
    } else {
        _hasLimit = true;
        _limit = limit;
    }
}

void Channel::setTopic(const std::string& topic)
{
    _topic = topic;
}

// ============================================================================
// GESTIÓN DE MIEMBROS
// ============================================================================

void Channel::addMember(User* user)
{
    if (!isMember(user))
        _members.push_back(user);
    
    // Si estaba invitado, lo sacamos de la lista de pendientes
    if (_invites.count(user->getNickname()))
        _invites.erase(user->getNickname());
}

void Channel::removeMember(User* user)
{
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

User* Channel::getMember(const std::string& nick) const
{
    for (size_t i = 0; i < _members.size(); ++i) {
        if (_members[i]->getNickname() == nick)
            return _members[i];
    }
    return NULL;
}

// [CRÍTICO] Implementación necesaria para el fix de spam en NICK
const std::vector<User*>& Channel::getMembers() const
{
    return _members;
}

// ============================================================================
// GESTIÓN DE OPERADORES
// ============================================================================

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

// ============================================================================
// GESTIÓN DE INVITACIONES
// ============================================================================

void Channel::addInvite(const std::string& nick)
{
    _invites.insert(nick);
}

bool Channel::isInvited(User* user) const
{
    return _invites.find(user->getNickname()) != _invites.end();
}

// ============================================================================
// COMUNICACIÓN
// ============================================================================

void Channel::broadcast(const std::string& msg, User* excludeUser)
{
    for (size_t i = 0; i < _members.size(); ++i)
    {
        if (_members[i] != excludeUser)
        {
            // Asumimos que User tiene getConnection() y ClientConnection tiene queueSend()
            if (_members[i]->getConnection())
                _members[i]->getConnection()->queueSend(msg);
        }
    }
}

std::string Channel::getNamesList() const
{
    std::string list = "";
    for (size_t i = 0; i < _members.size(); ++i)
    {
        if (i > 0) list += " ";
        
        // Prefijo de operador
        if (isOperator(_members[i]))
            list += "@";
        
        list += _members[i]->getNickname();
    }
    return list;
}
