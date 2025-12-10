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

void Server::cmdPass(ClientConnection* client, const Message& msg)
{
    if (msg.params.empty()) 
        return sendError(client, ERR_NEEDMOREPARAMS, "PASS");
    
    if (client->isRegistered())
        return sendError(client, ERR_ALREADYREGISTRED, "");

    if (msg.params[0] != this->password_)
    {
        sendError(client, ERR_PASSWDMISMATCH, "");
        client->closeConnection();
        return;
    }

    client->markPassReceived();
}

void Server::cmdNick(ClientConnection* client, const Message& msg)
{
    if (msg.params.empty())
        return sendError(client, ERR_NONICKNAMEGIVEN, "");

    std::string newNick = msg.params[0];

    // Caracteres permitidos (RFC 2812)
    if (newNick.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789[]{}\\|-_^") != std::string::npos)
        return sendError(client, ERR_ERRONEUSNICKNAME, newNick);

    // Verificar si ya existe
    for (size_t i = 0; i < clients_.size(); ++i)
    {
        if (clients_[i] != client && clients_[i]->getUser() && clients_[i]->getUser()->getNickname() == newNick)
            return sendError(client, ERR_NICKNAMEINUSE, newNick);
    }

    // Notificar cambio (si ya estaba registrado)
    if (client->isRegistered())
    {
        std::string oldPrefix = client->getUser()->getPrefix();
        std::string notification = ":" + oldPrefix + " NICK :" + newNick + "\r\n";
        
        client->queueSend(notification);
        
        const std::vector<Channel*>& channels = client->getUser()->getChannels();
        for (size_t i = 0; i < channels.size(); ++i)
        {
            // Pasar 'client->getUser()' como excepción para no enviárselo dos veces si broadcast lo maneja
            // Pero NICK es especial, mejor que todos lo reciban.
            channels[i]->broadcast(notification, NULL); 
        }
    }

    client->getUser()->setNickname(newNick);
    checkRegistration(client);
}

void Server::cmdUser(ClientConnection* client, const Message& msg)
{
    if (client->isRegistered())
        return sendError(client, ERR_ALREADYREGISTRED, "");

    if (msg.params.size() < 4)
        return sendError(client, ERR_NEEDMOREPARAMS, "USER");

    User* user = client->getUser();
    user->setUsername(msg.params[0]);
    user->setRealname(msg.params[3]);
    
    checkRegistration(client);
}

void Server::cmdQuit(ClientConnection* client, const Message& msg)
{
    std::string reason = (msg.params.empty()) ? "Client Quit" : msg.params[0];
    // La desconexión real sucede en el bucle principal cuando detecta el flag closed
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
    client->updateActivity();
}
