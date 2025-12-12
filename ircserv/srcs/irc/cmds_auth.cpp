/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   cmds_auth.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: carlsanc <carlsanc@student.42madrid>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/10 20:32:08 by carlsanc          #+#    #+#             */
/*   Updated: 2025/12/10 20:32:08 by carlsanc         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../server/Server.hpp"
#include "../client/ClientConnection.hpp"
#include "../client/User.hpp"
#include "../channel/Channel.hpp"
#include "CommandHelpers.hpp"
#include "../irc/NumericReplies.hpp"
#include <set> // Necesario para evitar spam en NICK

// ============================================================================
// HELPER: Enviar NOTICE informativo desde el servidor
// ============================================================================

static void sendServerNotice(ClientConnection* client, const std::string& msg)
{
    // NOTICE no debe generar respuestas automáticas (RFC 1459)
    std::string notice = ":ft_irc NOTICE * :" + msg + "\r\n";
    client->queueSend(notice);
}

// ============================================================================
// COMANDOS DE AUTENTICACIÓN CON FEEDBACK
// ============================================================================

void Server::cmdPass(ClientConnection* client, const Message& msg)
{
    if (msg.params.empty()) 
    {
        sendServerNotice(client, "*** Syntax: PASS <password>");
        return sendError(client, ERR_NEEDMOREPARAMS, "PASS");
    }
    
    if (client->isRegistered())
    {
        sendServerNotice(client, "*** You are already registered");
        return sendError(client, ERR_ALREADYREGISTRED, "");
    }

    if (msg.params[0] != this->password_)
    {
        sendServerNotice(client, "*** ERROR: Incorrect password. Connection will be closed.");
        sendServerNotice(client, "*** Please reconnect with the correct password.");
        sendError(client, ERR_PASSWDMISMATCH, "");
        client->closeConnection();
        return;
    }

    // Password accepted
    client->markPassReceived();
    sendServerNotice(client, "*** Password accepted. Please identify yourself:");
    sendServerNotice(client, "*** Use: NICK <your_nickname>");
    sendServerNotice(client, "*** Then: USER <username> 0 * :<realname>");
}

void Server::cmdNick(ClientConnection* client, const Message& msg)
{
    if (msg.params.empty())
    {
        sendServerNotice(client, "*** Syntax: NICK <nickname>");
        return sendError(client, ERR_NONICKNAMEGIVEN, "");
    }

    std::string newNick = msg.params[0];

    // Caracteres permitidos (RFC 2812)
    if (newNick.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789[]{}\\|-_^") != std::string::npos)
    {
        sendServerNotice(client, "*** ERROR: Invalid nickname. Use only letters, numbers, and -_[]{}\\|^");
        return sendError(client, ERR_ERRONEUSNICKNAME, newNick);
    }

    // Verificar si ya existe el nickname en el servidor
    for (size_t i = 0; i < clients_.size(); ++i)
    {
        if (clients_[i] != client && clients_[i]->getUser() && clients_[i]->getUser()->getNickname() == newNick)
        {
            sendServerNotice(client, "*** ERROR: Nickname '" + newNick + "' is already in use. Try another.");
            return sendError(client, ERR_NICKNAMEINUSE, newNick);
        }
    }

    // Notificar cambio (si ya estaba registrado)
    if (client->isRegistered())
    {
        std::string oldPrefix = client->getUser()->getPrefix();
        std::string notification = ":" + oldPrefix + " NICK :" + newNick + "\r\n";
        
        // 1. Enviarse la confirmación a uno mismo
        client->queueSend(notification);
        
        // 2. Enviar a los demás usuarios que comparten canal (SIN SPAM)
        std::set<ClientConnection*> uniqueRecipients;
        const std::vector<Channel*>& channels = client->getUser()->getChannels();
        
        for (size_t i = 0; i < channels.size(); ++i)
        {
            const std::vector<User*>& members = channels[i]->getMembers();
            for (size_t j = 0; j < members.size(); ++j) 
            {
                if (members[j]->getConnection() != client) {
                    uniqueRecipients.insert(members[j]->getConnection());
                }
            }
        }

        for (std::set<ClientConnection*>::iterator it = uniqueRecipients.begin(); it != uniqueRecipients.end(); ++it)
        {
            (*it)->queueSend(notification);
        }
        
        sendServerNotice(client, "*** Nickname changed to: " + newNick);
    }
    else if (!client->hasSentPass())
    {
        sendServerNotice(client, "*** Please authenticate first with: PASS <password>");
    }
    else
    {
        sendServerNotice(client, "*** Nickname set to: " + newNick);
        sendServerNotice(client, "*** Next step: USER <username> 0 * :<realname>");
    }

    // Aplicar el cambio
    client->getUser()->setNickname(newNick);
    checkRegistration(client);
}

void Server::cmdUser(ClientConnection* client, const Message& msg)
{
    if (client->isRegistered())
    {
        sendServerNotice(client, "*** You are already registered");
        return sendError(client, ERR_ALREADYREGISTRED, "");
    }

    if (msg.params.size() < 4)
    {
        sendServerNotice(client, "*** Syntax: USER <username> 0 * :<realname>");
        return sendError(client, ERR_NEEDMOREPARAMS, "USER");
    }

    if (!client->hasSentPass())
    {
        sendServerNotice(client, "*** Please authenticate first with: PASS <password>");
        return;
    }

    User* user = client->getUser();
    user->setUsername(msg.params[0]);
    user->setRealname(msg.params[3]);
    
    sendServerNotice(client, "*** Registration complete! Welcome to ft_irc.");
    sendServerNotice(client, "*** Available commands: JOIN, PRIVMSG, PART, TOPIC, MODE, KICK, INVITE, QUIT");
    
    checkRegistration(client);
}

void Server::cmdQuit(ClientConnection* client, const Message& msg)
{
    std::string reason = (msg.params.empty()) ? "Client Quit" : msg.params[0];
    
    // La lógica de desconexión y limpieza de canales se maneja en el bucle principal (Server::run)
    // al detectar que la conexión está cerrada.
    // Solo marcamos para cerrar.
    (void)reason; 
    client->closeConnection();
}

void Server::cmdPing(ClientConnection* client, const Message& msg)
{
    if (msg.params.empty())
        return sendError(client, ERR_NEEDMOREPARAMS, "PING");
    
    std::string token = msg.params[0];
    client->queueSend("PONG ft_irc :" + token + "\r\n");
}

void Server::cmdPong(ClientConnection* client, const Message& msg)
{
    (void)msg;
    // Solo sirve para mantener viva la conexión, actualiza el timestamp de actividad
    client->updateActivity();
}
