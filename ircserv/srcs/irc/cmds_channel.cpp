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
#include "../utils/Colors.hpp"
#include <sstream>

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
    newChan->setMode('t', true); //-R- Added to ensure the topic is protected by default (+t mode)
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

        // Obtener timestamp
        std::string timestamp = getCurrentTimestamp();
        
        // Notificar a todos en el canal (incluido el nuevo usuario)
        std::string joinMsg = std::string(BRIGHT_MAGENTA) + "@time=" + timestamp + RESET + " " +
                                    BRIGHT_CYAN + ":" + client->getUser()->getPrefix() + RESET +
                                    " " + BRIGHT_YELLOW + "JOIN" + RESET + " " +
                                    CYAN + chanName + RESET + "\r\n";
        channel->broadcast(joinMsg, NULL);

        // Enviar Topic
        if (channel->getTopic().empty())
            sendReply(client, RPL_NOTOPIC, chanName + std::string(" :") + YELLOW + "No topic is set" + RESET);
        else
            sendReply(client, RPL_TOPIC, chanName + std::string(" :") + CYAN + channel->getTopic() + RESET);

        // Enviar lista de Nombres (RPL_NAMREPLY)
        std::string symbol = "="; // Canal público
        sendReply(client, RPL_NAMREPLY, symbol + " " + chanName + std::string(" :") + GREEN + channel->getNamesList() + RESET);
        sendReply(client, RPL_ENDOFNAMES, chanName + std::string(" :") + CYAN + "End of /NAMES list" + RESET);
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

        // Obtener timestamp
        std::string timestamp = getCurrentTimestamp();

        std::string partMsg = std::string(BRIGHT_MAGENTA) + "@time=" + timestamp + RESET + " " +
                                BRIGHT_CYAN + ":" + client->getUser()->getPrefix() + RESET +
                                " " + BRIGHT_YELLOW + "PART" + RESET + " " +
                                CYAN + chanName + RESET + " :" + reason + "\r\n";
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
            sendReply(client, RPL_NOTOPIC, channel->getName() + std::string(" :") + YELLOW + "No topic is set" + RESET);
        else
            sendReply(client, RPL_TOPIC, channel->getName() + std::string(" :") + CYAN + channel->getTopic() + RESET);
        return;
    }

    // Intentar cambiar el topic
    if (channel->hasMode('t') && !channel->isOperator(client->getUser()))
        return sendError(client, ERR_CHANOPRIVSNEEDED, channel->getName());

    channel->setTopic(msg.params[1]);
    
    // Obtener timestamp
    std::string timestamp = getCurrentTimestamp();

    // Notificar el cambio a todos
    std::string topicMsg = std::string(BRIGHT_MAGENTA) + "@time=" + timestamp + RESET + " " +
                            BRIGHT_CYAN + ":" + client->getUser()->getPrefix() + RESET +
                            " " + BRIGHT_YELLOW + "TOPIC" + RESET + " " +
                            CYAN + channel->getName() + RESET + " :" +
                            BRIGHT_GREEN + msg.params[1] + RESET + "\r\n";
channel->broadcast(topicMsg, NULL);
}

void Server::cmdNames(ClientConnection* client, const Message& msg)
{
    // CRÍTICO: Verificar que el usuario esté registrado
    if (!client->isRegistered()) {
        sendError(client, ERR_NOTREGISTERED, "");
        return;
    }

    // NAMES sin parámetros: listar TODOS los canales visibles
    if (msg.params.empty())
    {
        // Iterar por todos los canales
        for (size_t i = 0; i < channels_.size(); ++i)
        {
            Channel* chan = channels_[i];
            std::string symbol = "="; // = público, @ secreto, * privado
            
            sendReply(client, RPL_NAMREPLY, 
                     symbol + " " + chan->getName() + " :" + 
                     GREEN + chan->getNamesList() + RESET);
        }
        
        sendReply(client, RPL_ENDOFNAMES, "* :" + std::string(CYAN) + "End of /NAMES list" + RESET);
        return;
    }

    // NAMES #canal: listar usuarios de un canal específico
    std::string chanName = msg.params[0];
    
    // Corrección: Asegurar prefijo válido (# o &). Si no tiene, poner #
    if (chanName[0] != '#' && chanName[0] != '&') 
        chanName = "#" + chanName;

    Channel* channel = getChannel(chanName);
    
    if (!channel)
        return sendError(client, ERR_NOSUCHCHANNEL, chanName);

    // Enviar lista de nombres (igual que en JOIN)
    std::string symbol = "="; // Canal público
    sendReply(client, RPL_NAMREPLY, 
             symbol + " " + chanName + " :" + 
             GREEN + channel->getNamesList() + RESET);
    
    sendReply(client, RPL_ENDOFNAMES, 
             chanName + std::string(" :") + CYAN + "End of /NAMES list" + RESET);
}

void Server::cmdWho(ClientConnection* client, const Message& msg)
{
    // CRÍTICO: Verificar que el usuario esté registrado
    if (!client->isRegistered()) {
        sendError(client, ERR_NOTREGISTERED, "");
        return;
    }

    // WHO sin parámetros: opcional, podríamos listar todos los usuarios visibles
    // Por simplicidad, enviamos solo END OF WHO
    if (msg.params.empty())
    {
        sendReply(client, RPL_ENDOFWHO, "* :" + std::string(CYAN) + "End of /WHO list" + RESET);
        return;
    }

    std::string target = msg.params[0];

    // ¿Es un canal? (empieza con # o &)
    if (target[0] == '#' || target[0] == '&')
    {
        Channel* channel = getChannel(target);
        
        if (!channel)
            return sendError(client, ERR_NOSUCHCHANNEL, target);

        // Enviar RPL_WHOREPLY (352) para cada miembro del canal
        const std::vector<User*>& members = channel->getMembers();
        
        for (size_t i = 0; i < members.size(); ++i)
        {
            User* member = members[i];
            
            // Flags: H = here (presente), G = gone (away)
            // @ = operador del canal, + = voice
            std::string flags = std::string(GREEN) + "H" + RESET; // Here
            
            if (channel->isOperator(member))
                flags = std::string(BRIGHT_YELLOW) + "H@" + RESET; // Operador
            
            // Formato RFC 2812 con colores:
            // <channel> <username> <host> <server> <nick> <flags> :<hopcount> <realname>
            std::string whoReply = CYAN + target + RESET + " " +
                                   BRIGHT_BLUE + member->getUsername() + RESET + " " +
                                   YELLOW + member->getHostname() + RESET + " " +
                                   MAGENTA + "ft_irc" + RESET + " " +
                                   BRIGHT_GREEN + member->getNickname() + RESET + " " +
                                   flags + " " +
                                   ":" + BRIGHT_MAGENTA + "0" + RESET + " " +
                                   BRIGHT_CYAN + member->getRealname() + RESET;
            
            sendReply(client, RPL_WHOREPLY, whoReply);
        }
        
        sendReply(client, RPL_ENDOFWHO, target + " :" + std::string(CYAN) + "End of /WHO list" + RESET);
        return;
    }

    // ¿Es un usuario específico? (buscar por nickname)
    User* targetUser = NULL;
    
    for (size_t i = 0; i < clients_.size(); ++i)
    {
        if (clients_[i]->isRegistered() && 
            clients_[i]->getUser()->getNickname() == target)
        {
            targetUser = clients_[i]->getUser();
            break;
        }
    }
    
    if (!targetUser)
        return sendError(client, ERR_NOSUCHNICK, target);

    // Enviar info del usuario con colores
    std::string flags = std::string(GREEN) + "H" + RESET; // Here
    
    // Formato: * = no canal en común
    std::string whoReply = std::string(YELLOW) + "*" + RESET + " " +
                       BRIGHT_BLUE + targetUser->getUsername() + RESET + " " +
                       YELLOW + targetUser->getHostname() + RESET + " " +
                       MAGENTA + "ft_irc" + RESET + " " +
                       BRIGHT_GREEN + targetUser->getNickname() + RESET + " " +
                       flags + " " +
                       ":" + BRIGHT_MAGENTA + "0" + RESET + " " +
                       BRIGHT_CYAN + targetUser->getRealname() + RESET;
    
    sendReply(client, RPL_WHOREPLY, whoReply);
    sendReply(client, RPL_ENDOFWHO, target + " :" + std::string(CYAN) + "End of /WHO list" + RESET);
}

std::string Server::getChannelsForUser(User* user) const
{
    std::string result;
    
    for (size_t i = 0; i < channels_.size(); ++i)
    {
        Channel* chan = channels_[i];
        
        if (chan->isMember(user))
        {
            if (!result.empty())
                result += " ";
            
            // Añadir prefijo @ si es operador del canal
            if (chan->isOperator(user))
                result += "@";
            
            result += chan->getName();
        }
    }
    
    return result;
}

void Server::cmdWhois(ClientConnection* client, const Message& msg)
{
    // CRÍTICO: Verificar que el usuario esté registrado
    if (!client->isRegistered()) {
        sendError(client, ERR_NOTREGISTERED, "");
        return;
    }

    // WHOIS requiere al menos un parámetro (nickname)
    if (msg.params.empty()) {
        sendError(client, ERR_NONICKNAMEGIVEN, "");
        return;
    }

    std::string targetNick = msg.params[0];

    // Buscar el usuario por nickname
    User* targetUser = NULL;
    
    for (size_t i = 0; i < clients_.size(); ++i)
    {
        if (clients_[i]->isRegistered() && 
            clients_[i]->getUser()->getNickname() == targetNick)
        {
            targetUser = clients_[i]->getUser();
            break;
        }
    }
    
    if (!targetUser)
        return sendError(client, ERR_NOSUCHNICK, targetNick);

    // ============================================================================
    // RPL_WHOISUSER (311) - Información básica del usuario
    // ============================================================================
    std::string whoisUser = BRIGHT_GREEN + targetNick + RESET + " " +
                           BRIGHT_BLUE + targetUser->getUsername() + RESET + " " +
                           YELLOW + targetUser->getHostname() + RESET + " * :" +
                           BRIGHT_CYAN + targetUser->getRealname() + RESET;
    sendReply(client, RPL_WHOISUSER, whoisUser);

    // ============================================================================
    // RPL_WHOISCHANNELS (319) - Canales donde está el usuario
    // ============================================================================
    std::string channels = getChannelsForUser(targetUser);
    if (!channels.empty()) {
        std::string whoisChannels = BRIGHT_GREEN + targetNick + RESET + " :" + 
                                   CYAN + channels + RESET;
        sendReply(client, RPL_WHOISCHANNELS, whoisChannels);
    }

    // ============================================================================
    // RPL_WHOISSERVER (312) - Información del servidor
    // ============================================================================
    std::string whoisServer = BRIGHT_GREEN + targetNick + RESET + " " +
                             MAGENTA + "ft_irc" + RESET + " :" +
                             BRIGHT_MAGENTA + "FT IRC Server" + RESET;
    sendReply(client, RPL_WHOISSERVER, whoisServer);

    // ============================================================================
    // RPL_WHOISIDLE (317) - Tiempo inactivo y conexión (opcional)
    // ============================================================================
    // Buscar el ClientConnection del targetUser
    ClientConnection* targetClient = NULL;
    for (size_t i = 0; i < clients_.size(); ++i)
    {
        if (clients_[i]->isRegistered() && 
            clients_[i]->getUser() == targetUser)
        {
            targetClient = clients_[i];
            break;
        }
    }

    time_t currentTime = time(NULL);
    long idleSeconds = 0;
    long signonTime = (long)currentTime;

    if (targetClient) {
        idleSeconds = (long)(currentTime - targetClient->getLastActivity());
        signonTime = (long)targetClient->getConnectTime();
    }

    std::ostringstream idleStream;
    idleStream << idleSeconds << " " << signonTime;

    std::string whoisIdle = BRIGHT_GREEN + targetNick + RESET + " " +
                        BRIGHT_MAGENTA + idleStream.str() + RESET + " :" +
                        YELLOW + "seconds idle, signon time" + RESET;
    sendReply(client, RPL_WHOISIDLE, whoisIdle);

    // ============================================================================
    // RPL_ENDOFWHOIS (318) - Fin de WHOIS
    // ============================================================================
    std::string endWhois = BRIGHT_GREEN + targetNick + RESET + " :" +
                          CYAN + "End of /WHOIS list" + RESET;
    sendReply(client, RPL_ENDOFWHOIS, endWhois);
}

