/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   cmds_msg.cpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: carlsanc <carlsanc@student.42madrid>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/10 20:32:52 by carlsanc          #+#    #+#             */
/*   Updated: 2025/12/10 20:32:52 by carlsanc         ###   ########.fr       */
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
    if (!client->isRegistered()) return;
    if (msg.params.size() < 2) return sendError(client, ERR_NEEDMOREPARAMS, "PRIVMSG");

    std::string target = msg.params[0];
    std::string text = msg.params[1];

    if (target[0] == '#')
    {
        Channel* channel = getChannel(target);
        if (!channel) return sendError(client, ERR_NOSUCHCHANNEL, target);
        
        // Verificación de si el canal permite mensajes externos (modo n, opcional)
        // Por defecto en esta implementación, cualquiera puede hablar si no implementas +n explícitamente.
        // Si implementaste +n:
        // if (channel->hasMode('n') && !channel->isMember(client->getUser()))
        //      return sendError(client, ERR_CANNOTSENDTOCHAN, target);

        std::string fullMsg = ":" + client->getUser()->getPrefix() + " PRIVMSG " + target + " :" + text + "\r\n";
        
        // Excluimos al emisor (el cliente ya sabe lo que escribió)
        channel->broadcast(fullMsg, client->getUser());
    }
    else
    {
        User* dest = NULL;
        for (size_t i = 0; i < clients_.size(); ++i) {
            if (clients_[i]->isRegistered() && clients_[i]->getUser()->getNickname() == target) {
                dest = clients_[i]->getUser();
                break;
            }
        }
        if (!dest) return sendError(client, ERR_NOSUCHNICK, target);

        std::string fullMsg = ":" + client->getUser()->getPrefix() + " PRIVMSG " + target + " :" + text + "\r\n";
        dest->getConnection()->queueSend(fullMsg);
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
