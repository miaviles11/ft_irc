/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   cmds_msg.cpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: carlsanc <carlsanc@student.42madrid>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/10 20:32:52 by carlsanc          #+#    #+#             */
/*   Updated: 2025/12/17 19:05:24 by carlsanc         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../server/Server.hpp"
#include "../client/ClientConnection.hpp"
#include "../client/User.hpp"
#include "../channel/Channel.hpp"
#include "CommandHelpers.hpp"
#include "../irc/NumericReplies.hpp"

void Server::cmdPrivMsg(ClientConnection* client, const Message& msg)
{
    if (!client->isRegistered()) {
        sendError(client, ERR_NOTREGISTERED, "");
        return;
    }
    if (msg.params.size() < 2) return sendError(client, ERR_NEEDMOREPARAMS, "PRIVMSG");

    User* sender = client->getUser();
    std::string target = msg.params[0];
    std::string text = msg.params[1];

    std::string fullMsg = ":" + sender->getPrefix() + " PRIVMSG " + target + " :" + text + "\r\n";

    // CASO 1: Mensaje a canal
    if (target[0] == '#')
    {
        Channel* channel = getChannel(target);
        if (!channel)
            return sendError(client, ERR_NOSUCHCHANNEL, target);
        
        if (!channel->isMember(sender))
            return sendError(client, ERR_CANNOTSENDTOCHAN, target);

        // Broadcast a todos los miembros (excepto sender)
        channel->broadcast(fullMsg, sender);
        
        // FIX: Forzar envío inmediato para todos los receptores
        const std::vector<User*>& members = channel->getMembers();
        for (std::vector<User*>::const_iterator it = members.begin(); it != members.end(); ++it)
        {
            User* member = *it;
            if (member == sender)
                continue;
            
            ClientConnection* conn = member->getConnection();
            if (conn && conn->hasPendingSend())
            {
                sendPendingData(conn);  // Intentar enviar inmediatamente
            }
        }
    }
    // CASO 2: Mensaje privado
    else
    {
        // Buscar usuario por nickname
        User* recipient = NULL;
        for (size_t i = 0; i < clients_.size(); ++i)
        {
            if (clients_[i]->isRegistered() && 
                clients_[i]->getUser()->getNickname() == target)
            {
                recipient = clients_[i]->getUser();
                break;
            }
        }
        
        if (!recipient)
            return sendError(client, ERR_NOSUCHNICK, target);
        
        ClientConnection* recipientConn = recipient->getConnection();
        if (recipientConn)
        {
            recipientConn->queueSend(fullMsg);
            sendPendingData(recipientConn);  // 🔥 FIX: Enviar inmediatamente
        }
    }
}

void Server::cmdNotice(ClientConnection* client, const Message& msg)
{
    // NOTICE no debe enviar respuestas de error según RFC
    if (!client->isRegistered() || msg.params.size() < 2) return;

    std::string target = msg.params[0];
    std::string text = msg.params[1];

    if (target[0] == '#') {
        Channel* channel = getChannel(target);
        if (channel && channel->isMember(client->getUser())) {
            std::string fullMsg = ":" + client->getUser()->getPrefix() + " NOTICE " + target + " :" + text + "\r\n";
            channel->broadcast(fullMsg, client->getUser());
        }
    } else {
        for (size_t i = 0; i < clients_.size(); ++i) {
            if (clients_[i]->isRegistered() && clients_[i]->getUser()->getNickname() == target) {
                std::string fullMsg = ":" + client->getUser()->getPrefix() + " NOTICE " + target + " :" + text + "\r\n";
                clients_[i]->queueSend(fullMsg);
                break;
            }
        }
    }
}
