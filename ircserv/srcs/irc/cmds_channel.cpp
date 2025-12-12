/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   cmds_channel.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: carlsanc <carlsanc@student.42madrid>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/10 20:32:20 by carlsanc          #+#    #+#             */
/*   Updated: 2025/12/10 20:32:20 by carlsanc         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../server/Server.hpp"
#include "../client/ClientConnection.hpp"
#include "../client/User.hpp"
#include "../channel/Channel.hpp"
#include "CommandHelpers.hpp"
#include "../irc/NumericReplies.hpp"

// NOTA: Estas funciones son miembros de Server, pero están implementadas aquí
// para organizar el código por temática.

Channel* Server::getChannel(const std::string& name)
{
    for (size_t i = 0; i < channels_.size(); ++i)
    {
        if (channels_[i]->getName() == name)
            return channels_[i];
    }
    return NULL;
}

Channel* Server::createChannel(const std::string& name)
{
    Channel* newChan = new Channel(name);
    channels_.push_back(newChan);
    return newChan;
}

void Server::cmdJoin(ClientConnection* client, const Message& msg)
{
    // CRÍTICO: Verificar que el usuario esté registrado
    if (!client->isRegistered()) {
        sendError(client, ERR_NOTREGISTERED, "");
        return;
    }
    
    if (msg.params.empty()) return sendError(client, ERR_NEEDMOREPARAMS, "JOIN");

    std::vector<std::string> targets = split(msg.params[0], ',');
    std::vector<std::string> keys;
    if (msg.params.size() > 1)
        keys = split(msg.params[1], ',');

    for (size_t i = 0; i < targets.size(); ++i)
    {
        std::string chanName = targets[i];
        std::string key = (i < keys.size()) ? keys[i] : "";

        // Corrección: Asegurar prefijo válido (# o &). Si no tiene, poner #
        if (chanName.empty()) continue;
        if (chanName[0] != '#' && chanName[0] != '&') 
            chanName = "#" + chanName;

        // RFC 2812: Los nombres de canal no pueden contener espacios, comas, o control chars
        bool invalidName = false;
        for (size_t j = 0; j < chanName.length(); ++j) {
            if (chanName[j] == ' ' || chanName[j] == ',' || chanName[j] == '\x07') {
                sendError(client, ERR_BADCHANMASK, chanName);
                invalidName = true;
                break;
            }
        }
        if (invalidName) continue;

        Channel* channel = getChannel(chanName);
        if (!channel)
        {
            channel = createChannel(chanName);
            // El creador se convierte en Operador automáticamente
            channel->addOperator(client->getUser());
        }

        // Si ya está dentro, no hacer nada
        if (channel->isMember(client->getUser()))
            continue;

        // --- VALIDACIONES DE MODOS ---
        if (channel->hasMode('i') && !channel->isInvited(client->getUser()))
        {
            sendError(client, ERR_INVITEONLYCHAN, chanName);
            continue;
        }
        if (channel->hasMode('k') && channel->getKey() != key)
        {
            sendError(client, ERR_BADCHANNELKEY, chanName);
            continue;
        }
        if (channel->hasMode('l') && channel->getUserCount() >= (size_t)channel->getLimit())
        {
            sendError(client, ERR_CHANNELISFULL, chanName);
            continue;
        }

        // Unirse efectivamente
        channel->addMember(client->getUser());
        client->getUser()->joinChannel(channel);

        // Notificar a todos en el canal (incluido el nuevo usuario)
        std::string joinMsg = ":" + client->getUser()->getPrefix() + " JOIN " + chanName + "\r\n";
        channel->broadcast(joinMsg, NULL);

        // Enviar Topic
        if (channel->getTopic().empty())
            sendReply(client, RPL_NOTOPIC, chanName + " :No topic is set");
        else
            sendReply(client, RPL_TOPIC, chanName + " :" + channel->getTopic());

        // Enviar lista de Nombres (RPL_NAMREPLY)
        std::string symbol = "="; // Canal público
        sendReply(client, RPL_NAMREPLY, symbol + " " + chanName + " :" + channel->getNamesList());
        sendReply(client, RPL_ENDOFNAMES, chanName + " :End of /NAMES list");
    }
}

void Server::cmdPart(ClientConnection* client, const Message& msg)
{
    if (!client->isRegistered()) {
        sendError(client, ERR_NOTREGISTERED, "");
        return;
    }
    if (msg.params.empty()) return sendError(client, ERR_NEEDMOREPARAMS, "PART");

    std::vector<std::string> targets = split(msg.params[0], ',');
    std::string reason = (msg.params.size() > 1) ? msg.params[1] : "Leaving";

    for (size_t i = 0; i < targets.size(); ++i)
    {
        std::string chanName = targets[i];
        Channel* channel = getChannel(chanName);
        
        if (!channel)
        {
            sendError(client, ERR_NOSUCHCHANNEL, chanName);
            continue;
        }
        
        if (!channel->isMember(client->getUser()))
        {
            sendError(client, ERR_NOTONCHANNEL, chanName);
            continue;
        }

        std::string partMsg = ":" + client->getUser()->getPrefix() + " PART " + chanName + " :" + reason + "\r\n";
        channel->broadcast(partMsg, NULL); // Enviar a todos

        channel->removeMember(client->getUser());
        client->getUser()->leaveChannel(channel);

        // Borrar canal si se queda vacío
        if (channel->getUserCount() == 0)
        {
            for (std::vector<Channel*>::iterator it = channels_.begin(); it != channels_.end(); )
            {
                if (*it == channel)
                {
                    delete *it;
                    it = channels_.erase(it);
                    break;
                }
                else ++it;
            }
        }
    }
}

void Server::cmdTopic(ClientConnection* client, const Message& msg)
{
    if (!client->isRegistered()) {
        sendError(client, ERR_NOTREGISTERED, "");
        return;
    }
    if (msg.params.empty()) return sendError(client, ERR_NEEDMOREPARAMS, "TOPIC");

    Channel* channel = getChannel(msg.params[0]);
    if (!channel) return sendError(client, ERR_NOSUCHCHANNEL, msg.params[0]);

    // Solo consultar el topic
    if (msg.params.size() == 1)
    {
        if (channel->getTopic().empty())
            sendReply(client, RPL_NOTOPIC, channel->getName() + " :No topic is set");
        else
            sendReply(client, RPL_TOPIC, channel->getName() + " :" + channel->getTopic());
        return;
    }

    // Intentar cambiar el topic
    if (channel->hasMode('t') && !channel->isOperator(client->getUser()))
        return sendError(client, ERR_CHANOPRIVSNEEDED, channel->getName());

    channel->setTopic(msg.params[1]);
    
    // Notificar el cambio a todos
    std::string topicMsg = ":" + client->getUser()->getPrefix() + " TOPIC " + channel->getName() + " :" + msg.params[1] + "\r\n";
    channel->broadcast(topicMsg, NULL);
}
