/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   cmds_op.cpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: carlsanc <carlsanc@student.42madrid>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/10 20:32:35 by carlsanc          #+#    #+#             */
/*   Updated: 2025/12/10 20:32:35 by carlsanc         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../server/Server.hpp"
#include "../client/ClientConnection.hpp"
#include "../client/User.hpp"
#include "../channel/Channel.hpp"
#include "CommandHelpers.hpp"
#include "../irc/NumericReplies.hpp"
#include "../utils/Colors.hpp"
#include <cstdlib>
#include <cstdio>
#include <cctype>

void Server::cmdKick(ClientConnection* client, const Message& msg)
{
    if (!client->isRegistered()) {
        sendError(client, ERR_NOTREGISTERED, "");
        return;
    }
    if (msg.params.size() < 2) return sendError(client, ERR_NEEDMOREPARAMS, "KICK");
    
    std::string chanName = msg.params[0];
    std::string targetNick = msg.params[1];
    std::string comment = (msg.params.size() > 2) ? msg.params[2] : "Kicked";

    Channel* channel = getChannel(chanName);
    if (!channel) return sendError(client, ERR_NOSUCHCHANNEL, chanName);

    // Verificar privilegios
    if (!channel->isOperator(client->getUser()))
        return sendError(client, ERR_CHANOPRIVSNEEDED, chanName);

    // Verificar si el usuario objetivo está en el canal
    User* targetUser = channel->getMember(targetNick);
    if (!targetUser) 
        return sendError(client, ERR_USERNOTINCHANNEL, targetNick + " " + chanName);

    // Obtener timestamp
    std::string timestamp = getCurrentTimestamp();

    // Mensaje KICK
    std::string kickMsg = std::string(BRIGHT_MAGENTA) + "@time=" + timestamp + RESET + " " +
                            BRIGHT_CYAN + ":" + client->getUser()->getPrefix() + RESET +
                            " " + BRIGHT_YELLOW + "KICK" + RESET + " " +
                            CYAN + chanName + RESET + " " +
                            targetNick + " :" + RED + comment + RESET + "\r\n";

    channel->broadcast(kickMsg, NULL);

    // Eliminar efectivamente
    channel->removeMember(targetUser);
    targetUser->leaveChannel(channel);
}

void Server::cmdInvite(ClientConnection* client, const Message& msg)
{
    if (!client->isRegistered()) {
        sendError(client, ERR_NOTREGISTERED, "");
        return;
    }
    if (msg.params.size() < 2) return sendError(client, ERR_NEEDMOREPARAMS, "INVITE");

    std::string targetNick = msg.params[0];
    std::string chanName = msg.params[1];

    Channel* channel = getChannel(chanName);
    if (channel)
    {
        if (!channel->isMember(client->getUser()))
             return sendError(client, ERR_NOTONCHANNEL, chanName);
        
        if (channel->hasMode('i') && !channel->isOperator(client->getUser()))
             return sendError(client, ERR_CHANOPRIVSNEEDED, chanName);
        
        if (channel->getMember(targetNick))
             return sendError(client, ERR_USERONCHANNEL, targetNick + " " + chanName);
        
        channel->addInvite(targetNick);
    }

    // Buscar al usuario destino globalmente en el servidor
    User* dest = NULL;
    for (size_t i = 0; i < clients_.size(); ++i) {
        if (clients_[i]->isRegistered() && clients_[i]->getUser()->getNickname() == targetNick) {
            dest = clients_[i]->getUser();
            break;
        }
    }
    if (!dest) return sendError(client, ERR_NOSUCHNICK, targetNick);

    // Obtener timestamp
    std::string timestamp = getCurrentTimestamp();

    // Mensaje INVITE
    std::string invMsg = std::string(BRIGHT_MAGENTA) + "@time=" + timestamp + RESET + " " +
                            BRIGHT_CYAN + ":" + client->getUser()->getPrefix() + RESET +
                            " " + BRIGHT_YELLOW + "INVITE" + RESET + " " +
                            targetNick + " " + CYAN + chanName + RESET + "\r\n";

    dest->getConnection()->queueSend(invMsg);
    sendReply(client, RPL_INVITING, targetNick + " " + chanName);
}

void Server::cmdMode(ClientConnection* client, const Message& msg)
{
    if (!client->isRegistered()) {
        sendError(client, ERR_NOTREGISTERED, "");
        return;
    }
    if (msg.params.size() < 1) return sendError(client, ERR_NEEDMOREPARAMS, "MODE");

    std::string target = msg.params[0];
    
    // --- MODO USUARIO (Solo +i) ---
    if (target[0] != '#')
    {
        if (target != client->getUser()->getNickname())
        {
            sendError(client, ERR_USERSDONTMATCH, "");
            return;
        }
        
        // Consulta de modos
        if (msg.params.size() == 1)
        {
            std::string modes = "+";
            if (client->getUser()->isInvisible()) modes += "i";
            sendReply(client, RPL_UMODEIS, modes);
            return;
        }

        // Cambio de modos
        std::string modeString = msg.params[1];
        char action = '+';
        std::string appliedModes = "";
        
        for (size_t i = 0; i < modeString.length(); ++i)
        {
            if (modeString[i] == '+' || modeString[i] == '-') {
                action = modeString[i];
                continue;
            }
            if (modeString[i] == 'i') {
                bool newC = (action == '+');
                if (client->getUser()->isInvisible() != newC) {
                    client->getUser()->setInvisible(newC);
                    if (appliedModes.find(action) == std::string::npos) // Evitar duplicar signo
                        appliedModes += action;
                    appliedModes += 'i';
                }
            }
        }
        if (!appliedModes.empty()) {
            // Obtener timestamp
            std::string timestamp = getCurrentTimestamp();

            // Mensaje MODE
            std::string modeMsg = std::string(BRIGHT_MAGENTA) + "@time=" + timestamp + RESET + " " +
                                    BRIGHT_CYAN + ":" + client->getUser()->getPrefix() + RESET +
                                    " " + BRIGHT_YELLOW + "MODE" + RESET + " " +
                                    target + " :" + BRIGHT_GREEN + appliedModes + RESET + "\r\n";
            client->queueSend(modeMsg);
        }
        return;
    }

    // --- MODO CANAL ---
    Channel* channel = getChannel(target);
    if (!channel) return sendError(client, ERR_NOSUCHCHANNEL, target);

    if (msg.params.size() == 1) {
         sendReply(client, RPL_CHANNELMODEIS, target + " " + channel->getModes());
         return;
    }

    if (!channel->isOperator(client->getUser()))
        return sendError(client, ERR_CHANOPRIVSNEEDED, target);

    std::string modeString = msg.params[1];
    size_t paramIdx = 2; // Índice para argumentos extra (claves, usuarios, limites)
    char action = '+';

    for (size_t i = 0; i < modeString.length(); ++i)
    {
        char mode = modeString[i];
        
        if (mode == '+' || mode == '-') {
            action = mode;
            continue;
        }

        // o: Operator
        if (mode == 'o') {
            if (paramIdx >= msg.params.size()) continue;
            std::string targetNick = msg.params[paramIdx++];
            User* targetUser = channel->getMember(targetNick);
            
            // Si el usuario no existe en el canal, ignoramos silenciosamente o podríamos mandar error
            if (targetUser) {
                if (action == '+') channel->addOperator(targetUser);
                else channel->removeOperator(targetUser);
                
                // Obtener timestamp
                std::string timestamp = getCurrentTimestamp();

                // Mensaje MODE +o/-o
                std::string modeMsg = std::string(BRIGHT_MAGENTA) + "@time=" + timestamp + RESET + " " +
                                        BRIGHT_CYAN + ":" + client->getUser()->getPrefix() + RESET +
                                        " " + BRIGHT_YELLOW + "MODE" + RESET + " " +
                                        CYAN + target + RESET + " " +
                                        BRIGHT_GREEN + std::string(1, action) + "o" + RESET + " " +
                                        targetNick + "\r\n";
                channel->broadcast(modeMsg, NULL);
            } else {
                 sendError(client, ERR_USERNOTINCHANNEL, targetNick + " " + target);
            }
        }
        // k: Key
        else if (mode == 'k') {
            if (action == '+') {
                if (paramIdx >= msg.params.size()) continue;
                std::string key = msg.params[paramIdx++];
                
                // [FIX] Validar que la clave no tenga espacios (RFC)
                if (key.find(' ') != std::string::npos) continue;

                channel->setKey(key);
                // Obtener timestamp
                std::string timestamp = getCurrentTimestamp();

                // Mensaje MODE +k
                std::string modeMsg = std::string(BRIGHT_MAGENTA) + "@time=" + timestamp + RESET + " " +
                                        BRIGHT_CYAN + ":" + client->getUser()->getPrefix() + RESET +
                                        " " + BRIGHT_YELLOW + "MODE" + RESET + " " +
                                        CYAN + target + RESET + " " +
                                        BRIGHT_GREEN + std::string(1, action) + "k" + RESET + " " +
                                        key + "\r\n";
                channel->broadcast(modeMsg, NULL);
            } else {
                // [FIX RFC] Modo permisivo: permite -k sin parámetro para OPs
                std::string keyParam = "";
                if (paramIdx < msg.params.size())
                    keyParam = msg.params[paramIdx++];
                
                // Solo verificar si se proporcionó clave
                if (!keyParam.empty() && channel->getKey() != keyParam) {
                    sendError(client, ERR_BADCHANNELKEY, channel->getName());
                    continue;
                }
                
                channel->setKey("");
                // Obtener timestamp
                std::string timestamp = getCurrentTimestamp();

                // Mensaje MODE -k
                std::string modeMsg = std::string(BRIGHT_MAGENTA) + "@time=" + timestamp + RESET + " " +
                                        BRIGHT_CYAN + ":" + client->getUser()->getPrefix() + RESET +
                                        " " + BRIGHT_YELLOW + "MODE" + RESET + " " +
                                        CYAN + target + RESET + " " +
                                        BRIGHT_GREEN + "-k" + RESET + " *\r\n";
                channel->broadcast(modeMsg, NULL);
            }
        }
        // l: Limit
        else if (mode == 'l') {
            if (action == '+') {
                if (paramIdx >= msg.params.size()) continue;
                std::string limitStr = msg.params[paramIdx++];
                
                // [FIX SEGURIDAD] Validar que sea numérico antes de atoi
                bool isNumeric = true;
                size_t start = 0;
                if (!limitStr.empty() && (limitStr[0] == '-' || limitStr[0] == '+')) start = 1;
                
                for (size_t j = start; j < limitStr.length(); ++j) {
                    if (!std::isdigit(limitStr[j])) {
                        isNumeric = false;
                        break;
                    }
                }
                
                // Si no es número o es negativo, ignoramos
                if (!isNumeric || limitStr.empty()) continue;

                int limit = std::atoi(limitStr.c_str());
                // Un límite de 0 o negativo no tiene sentido en este contexto
                if (limit <= 0) continue; 

                channel->setLimit(limit);
                char buff[20];
                std::sprintf(buff, "%d", limit);
                // Obtener timestamp
                std::string timestamp = getCurrentTimestamp();

                // Mensaje MODE +l
                std::string modeMsg = std::string(BRIGHT_MAGENTA) + "@time=" + timestamp + RESET + " " +
                                        BRIGHT_CYAN + ":" + client->getUser()->getPrefix() + RESET +
                                        " " + BRIGHT_YELLOW + "MODE" + RESET + " " +
                                        CYAN + target + RESET + " " +
                                        BRIGHT_GREEN + std::string(1, action) + "l" + RESET + " " +
                                        std::string(buff) + "\r\n";
                channel->broadcast(modeMsg, NULL);
            } else {
                channel->setLimit(0); // 0 significa sin límite
                // Obtener timestamp
                std::string timestamp = getCurrentTimestamp();

                // Mensaje MODE -l
                std::string modeMsg = std::string(BRIGHT_MAGENTA) + "@time=" + timestamp + RESET + " " +
                                        BRIGHT_CYAN + ":" + client->getUser()->getPrefix() + RESET +
                                        " " + BRIGHT_YELLOW + "MODE" + RESET + " " +
                                        CYAN + target + RESET + " " +
                                        BRIGHT_GREEN + std::string(1, action) + "l" + RESET + "\r\n";
                channel->broadcast(modeMsg, NULL);
            }
        }
        // i: Invite Only | t: Topic Restricted
        else if (mode == 'i' || mode == 't') {
            channel->setMode(mode, (action == '+'));
            // Obtener timestamp
            std::string timestamp = getCurrentTimestamp();
            std::string mStr(1, mode);

            // Mensaje MODE +i/+t
            std::string modeMsg = std::string(BRIGHT_MAGENTA) + "@time=" + timestamp + RESET + " " +
                                    BRIGHT_CYAN + ":" + client->getUser()->getPrefix() + RESET +
                                    " " + BRIGHT_YELLOW + "MODE" + RESET + " " +
                                    CYAN + target + RESET + " " +
                                    BRIGHT_GREEN + std::string(1, action) + mStr + RESET + "\r\n";
            channel->broadcast(modeMsg, NULL);
            
        }
    }
}
