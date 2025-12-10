/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Commands.cpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: carlsanc <carlsanc@student.42madrid>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/10 19:22:47 by carlsanc          #+#    #+#             */
/*   Updated: 2025/12/10 19:23:04 by carlsanc         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../server/Server.hpp"
#include "../client/ClientConnection.hpp"
#include "../client/User.hpp"
#include "../channel/Channel.hpp"
#include "../irc/NumericReplies.hpp"
#include <sstream>
#include <vector>
#include <algorithm>

/* -------------------------------------------------------------------------- */
/* UTILIDADES                                                                 */
/* -------------------------------------------------------------------------- */

// Helper para enviar respuestas numéricas formateadas
static void sendReply(ClientConnection* client, std::string num, std::string msg)
{
    std::string finalMsg = ":ft_irc " + num + " " + client->getUser()->getNickname() + " " + msg + "\r\n";
    client->queueSend(finalMsg);
}

// Helper para enviar errores simples
static void sendError(ClientConnection* client, std::string num, std::string arg)
{
    std::string msg = arg + " :Error check RFC"; // Mensaje genérico, se puede refinar
    sendReply(client, num, msg);
}

// Helper para dividir strings (para JOIN #a,#b)
static std::vector<std::string> split(const std::string &s, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

// Helper para verificar registro completo
static void checkRegistration(ClientConnection* client)
{
    if (client->isRegistered()) return;
    
    User* user = client->getUser();
    if (client->hasSentPass() && !user->getNickname().empty() && !user->getUsername().empty())
    {
        client->setRegistered(true);
        sendReply(client, RPL_WELCOME, ":Welcome to the FT_IRC Network " + user->getPrefix());
        std::cout << "[SERVER] User registered: " << user->getNickname() << std::endl;
    }
}

/* ========================================================================== */
/* AUTENTICACIÓN Y REGISTRO                          */
/* ========================================================================== */

void Server::cmdPass(ClientConnection* client, const Message& msg)
{
    if (msg.params.empty()) 
        return sendError(client, ERR_NEEDMOREPARAMS, "PASS");
    
    if (client->isRegistered())
        return sendError(client, ERR_ALREADYREGISTRED, ":You may not reregister");

    if (msg.params[0] != this->password_)
    {
        sendError(client, ERR_PASSWDMISMATCH, ":Password incorrect");
        client->closeConnection(); // Desconexión por seguridad
        return;
    }

    client->markPassReceived();
}

void Server::cmdNick(ClientConnection* client, const Message& msg)
{
    if (msg.params.empty())
        return sendError(client, ERR_NONICKNAMEGIVEN, ":No nickname given");

    std::string newNick = msg.params[0];

    // Validar caracteres (letras, numeros, especiales permitidos)
    if (newNick.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789[]{}\\|-_^") != std::string::npos)
        return sendError(client, ERR_ERRONEUSNICKNAME, newNick + " :Erroneous nickname");

    // Verificar colisiones
    for (size_t i = 0; i < clients_.size(); ++i)
    {
        if (clients_[i] != client && clients_[i]->getUser() && clients_[i]->getUser()->getNickname() == newNick)
            return sendError(client, ERR_NICKNAMEINUSE, newNick + " :Nickname is already in use");
    }

    // Si ya estaba registrado, notificar el cambio a todos los canales
    if (client->isRegistered())
    {
        std::string oldPrefix = client->getUser()->getPrefix();
        std::string notification = ":" + oldPrefix + " NICK :" + newNick + "\r\n";
        client->queueSend(notification);
        
        // Broadcast a canales compartidos (TODO: Implementar broadcast real a canales)
        // Por ahora solo al usuario
    }

    client->getUser()->setNickname(newNick);
    checkRegistration(client);
}

void Server::cmdUser(ClientConnection* client, const Message& msg)
{
    if (client->isRegistered())
        return sendError(client, ERR_ALREADYREGISTRED, ":You may not reregister");

    if (msg.params.size() < 4)
        return sendError(client, ERR_NEEDMOREPARAMS, "USER");

    User* user = client->getUser();
    user->setUsername(msg.params[0]);
    user->setRealname(msg.params[3]); // El parámetro 3 suele ser el realname (con espacios si fue parseado correctamente)
    
    checkRegistration(client);
}

void Server::cmdQuit(ClientConnection* client, const Message& msg)
{
    std::string reason = (msg.params.empty()) ? "Client Quit" : msg.params[0];
    // La lógica de desconexión real ocurre en el Server loop cuando detecta cierre
    // Pero aquí podemos enviar el mensaje de error o cerrar el socket.
    // Lo ideal es marcar el cliente para cierre.
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
    (void)msg; // Ignorar, solo actualiza actividad (ya hecho en receiveData)
    client->updateActivity();
}

/* ========================================================================== */
/* CANALES                                     */
/* ========================================================================== */

// Busca un canal existente por nombre
Channel* Server::getChannel(const std::string& name)
{
    for (size_t i = 0; i < channels_.size(); ++i)
    {
        if (channels_[i]->getName() == name)
            return channels_[i];
    }
    return NULL;
}

// Crea un nuevo canal
Channel* Server::createChannel(const std::string& name)
{
    Channel* newChan = new Channel(name);
    channels_.push_back(newChan);
    return newChan;
}

void Server::cmdJoin(ClientConnection* client, const Message& msg)
{
    if (!client->isRegistered()) return;
    if (msg.params.empty()) return sendError(client, ERR_NEEDMOREPARAMS, "JOIN");

    std::vector<std::string> targets = split(msg.params[0], ',');
    std::vector<std::string> keys;
    if (msg.params.size() > 1)
        keys = split(msg.params[1], ',');

    for (size_t i = 0; i < targets.size(); ++i)
    {
        std::string chanName = targets[i];
        std::string key = (i < keys.size()) ? keys[i] : "";

        if (chanName[0] != '#') chanName = "#" + chanName; // Asegurar formato

        Channel* channel = getChannel(chanName);
        if (!channel)
        {
            channel = createChannel(chanName);
            channel->addOperator(client->getUser()); // Primer usuario es Operador
        }

        // Validaciones de Modos
        if (channel->hasMode('i') && !channel->isInvited(client->getUser()))
        {
            sendError(client, ERR_INVITEONLYCHAN, chanName + " :Cannot join channel (+i)");
            continue;
        }
        if (channel->hasMode('k') && channel->getKey() != key)
        {
            sendError(client, ERR_BADCHANNELKEY, chanName + " :Cannot join channel (+k)");
            continue;
        }
        if (channel->hasMode('l') && channel->getUserCount() >= channel->getLimit())
        {
            sendError(client, ERR_CHANNELISFULL, chanName + " :Cannot join channel (+l)");
            continue;
        }

        // Unirse
        channel->addMember(client->getUser());
        client->getUser()->joinChannel(channel);

        // Notificar JOIN a todos
        std::string joinMsg = ":" + client->getUser()->getPrefix() + " JOIN " + chanName + "\r\n";
        channel->broadcast(joinMsg, NULL); // NULL = enviar a todos

        // Enviar Topic
        if (channel->getTopic().empty())
            sendReply(client, RPL_NOTOPIC, chanName + " :No topic is set");
        else
            sendReply(client, RPL_TOPIC, chanName + " :" + channel->getTopic());

        // Enviar lista de nombres (RPL_NAMREPLY)
        // Formato: :Server 353 Nick = #channel :Nick1 @Nick2
        std::string names = channel->getNamesList(); 
        sendReply(client, RPL_NAMREPLY, "= " + chanName + " :" + names);
        sendReply(client, RPL_ENDOFNAMES, chanName + " :End of /NAMES list");
    }
}

void Server::cmdPart(ClientConnection* client, const Message& msg)
{
    if (!client->isRegistered()) return;
    if (msg.params.empty()) return sendError(client, ERR_NEEDMOREPARAMS, "PART");

    std::vector<std::string> targets = split(msg.params[0], ',');
    std::string reason = (msg.params.size() > 1) ? msg.params[1] : "Leaving";

    for (size_t i = 0; i < targets.size(); ++i)
    {
        Channel* channel = getChannel(targets[i]);
        if (!channel)
        {
            sendError(client, ERR_NOSUCHCHANNEL, targets[i] + " :No such channel");
            continue;
        }
        
        if (!channel->isMember(client->getUser()))
        {
            sendError(client, ERR_NOTONCHANNEL, targets[i] + " :You're not on that channel");
            continue;
        }

        // 1. Notificar a todos (Broadcast)
        std::string partMsg = ":" + client->getUser()->getPrefix() + " PART " + targets[i] + " :" + reason + "\r\n";
        channel->broadcast(partMsg, NULL);

        // 2. Desvincular usuario y canal
        channel->removeMember(client->getUser());
        client->getUser()->leaveChannel(channel);

        // 3. Borrado seguro del canal si está vacío
        if (channel->getUserCount() == 0)
        {
            for (std::vector<Channel*>::iterator it = channels_.begin(); it != channels_.end(); )
            {
                if (*it == channel)
                {
                    delete *it;            // Liberar memoria
                    it = channels_.erase(it); // Actualizar iterador
                    break;                 // Salimos, ya que borramos el canal buscado
                }
                else
                {
                    ++it;                  // Avanzar solo si no borramos
                }
            }
        }
    }
}

void Server::cmdTopic(ClientConnection* client, const Message& msg)
{
    if (!client->isRegistered()) return;
    if (msg.params.empty()) return sendError(client, ERR_NEEDMOREPARAMS, "TOPIC");

    Channel* channel = getChannel(msg.params[0]);
    if (!channel) return sendError(client, ERR_NOSUCHCHANNEL, msg.params[0]);

    // VIEW TOPIC
    if (msg.params.size() == 1)
    {
        if (channel->getTopic().empty())
            sendReply(client, RPL_NOTOPIC, channel->getName() + " :No topic is set");
        else
            sendReply(client, RPL_TOPIC, channel->getName() + " :" + channel->getTopic());
        return;
    }

    // SET TOPIC
    if (channel->hasMode('t') && !channel->isOperator(client->getUser()))
        return sendError(client, ERR_CHANOPRIVSNEEDED, channel->getName() + " :You're not channel operator");

    channel->setTopic(msg.params[1]);
    std::string topicMsg = ":" + client->getUser()->getPrefix() + " TOPIC " + channel->getName() + " :" + msg.params[1] + "\r\n";
    channel->broadcast(topicMsg, NULL);
}

/* ========================================================================== */
/* COMUNICACIÓN                                      */
/* ========================================================================== */

void Server::cmdPrivMsg(ClientConnection* client, const Message& msg)
{
    if (!client->isRegistered()) return;
    if (msg.params.size() < 2) return sendError(client, ERR_NEEDMOREPARAMS, "PRIVMSG");

    std::string target = msg.params[0];
    std::string text = msg.params[1];

    // Mensaje a CANAL
    if (target[0] == '#')
    {
        Channel* channel = getChannel(target);
        if (!channel) return sendError(client, ERR_NOSUCHCHANNEL, target);
        
        // OPCIONAL: Check si usuario está en canal (algunos servidores lo requieren)
        if (!channel->isMember(client->getUser()))
             return sendError(client, ERR_CANNOTSENDTOCHAN, target + " :Cannot send to channel");

        std::string fullMsg = ":" + client->getUser()->getPrefix() + " PRIVMSG " + target + " :" + text + "\r\n";
        channel->broadcast(fullMsg, client->getUser()); // No enviar a uno mismo
    }
    // Mensaje a USUARIO
    else
    {
        // Buscar usuario en lista de clientes
        User* dest = NULL;
        for (size_t i = 0; i < clients_.size(); ++i) {
            if (clients_[i]->isRegistered() && clients_[i]->getUser()->getNickname() == target) {
                dest = clients_[i]->getUser();
                break;
            }
        }
        if (!dest) return sendError(client, ERR_NOSUCHNICK, target + " :No such nick/channel");

        std::string fullMsg = ":" + client->getUser()->getPrefix() + " PRIVMSG " + target + " :" + text + "\r\n";
        dest->getConnection()->queueSend(fullMsg);
    }
}

void Server::cmdNotice(ClientConnection* client, const Message& msg)
{
    // NOTICE es igual que PRIVMSG pero NUNCA envía respuestas de error automática
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

/* ========================================================================== */
/* OPERADORES (MODE, KICK, INVITE)                   */
/* ========================================================================== */

void Server::cmdKick(ClientConnection* client, const Message& msg)
{
    if (msg.params.size() < 2) return sendError(client, ERR_NEEDMOREPARAMS, "KICK");
    
    std::string chanName = msg.params[0];
    std::string targetNick = msg.params[1];
    std::string comment = (msg.params.size() > 2) ? msg.params[2] : "Kicked";

    Channel* channel = getChannel(chanName);
    if (!channel) return sendError(client, ERR_NOSUCHCHANNEL, chanName);

    if (!channel->isOperator(client->getUser()))
        return sendError(client, ERR_CHANOPRIVSNEEDED, chanName + " :You're not channel operator");

    User* targetUser = channel->getMember(targetNick);
    if (!targetUser) return sendError(client, ERR_USERNOTINCHANNEL, targetNick + " " + chanName + " :They aren't on that channel");

    // Enviar mensaje KICK a todos (incluido el kickeado)
    std::string kickMsg = ":" + client->getUser()->getPrefix() + " KICK " + chanName + " " + targetNick + " :" + comment + "\r\n";
    channel->broadcast(kickMsg, NULL);

    channel->removeMember(targetUser);
    targetUser->leaveChannel(channel);
}

void Server::cmdInvite(ClientConnection* client, const Message& msg)
{
    if (msg.params.size() < 2) return sendError(client, ERR_NEEDMOREPARAMS, "INVITE");

    std::string targetNick = msg.params[0];
    std::string chanName = msg.params[1];

    Channel* channel = getChannel(chanName);
    if (channel)
    {
        if (!channel->isMember(client->getUser()))
             return sendError(client, ERR_NOTONCHANNEL, chanName + " :You're not on that channel");
        
        if (channel->hasMode('i') && !channel->isOperator(client->getUser()))
             return sendError(client, ERR_CHANOPRIVSNEEDED, chanName + " :You're not channel operator");
        
        if (channel->getMember(targetNick))
             return sendError(client, ERR_USERONCHANNEL, targetNick + " " + chanName + " :is already on channel");
        
        // Agregar a la lista de invitados
        channel->addInvite(targetNick);
    }

    // Buscar usuario objetivo
    User* dest = NULL;
    for (size_t i = 0; i < clients_.size(); ++i) {
        if (clients_[i]->isRegistered() && clients_[i]->getUser()->getNickname() == targetNick) {
            dest = clients_[i]->getUser();
            break;
        }
    }
    if (!dest) return sendError(client, ERR_NOSUCHNICK, targetNick);

    // Enviar notificación INVITE al destino
    std::string invMsg = ":" + client->getUser()->getPrefix() + " INVITE " + targetNick + " " + chanName + "\r\n";
    dest->getConnection()->queueSend(invMsg);
    
    // Confirmar al emisor
    sendReply(client, RPL_INVITING, targetNick + " " + chanName);
}

void Server::cmdMode(ClientConnection* client, const Message& msg)
{
    if (msg.params.size() < 2) return sendError(client, ERR_NEEDMOREPARAMS, "MODE");

    std::string target = msg.params[0];
    
    // Modo de canal
    if (target[0] == '#')
    {
        Channel* channel = getChannel(target);
        if (!channel) return sendError(client, ERR_NOSUCHCHANNEL, target);

        // Si solo envían "MODE #channel", devolver los modos actuales
        if (msg.params.size() == 1) {
             sendReply(client, "324", target + " " + channel->getModes());
             return;
        }

        if (!channel->isOperator(client->getUser()))
            return sendError(client, ERR_CHANOPRIVSNEEDED, target + " :You're not channel operator");

        std::string modeString = msg.params[1];
        size_t paramIdx = 2; // Índice para argumentos extra (clave, usuario, limite)
        
        char action = '+'; // Default add

        for (size_t i = 0; i < modeString.length(); ++i)
        {
            char mode = modeString[i];
            
            if (mode == '+' || mode == '-') {
                action = mode;
                continue;
            }

            // MODOS QUE REQUIEREN ARGUMENTOS
            
            // o: Operator
            if (mode == 'o') {
                if (paramIdx >= msg.params.size()) continue; // Ignorar si falta arg
                std::string targetNick = msg.params[paramIdx++];
                User* targetUser = channel->getMember(targetNick);
                
                if (targetUser) {
                    if (action == '+') channel->addOperator(targetUser);
                    else channel->removeOperator(targetUser);
                    
                    // Broadcast cambio
                    channel->broadcast(":" + client->getUser()->getPrefix() + " MODE " + target + " " + action + "o " + targetNick + "\r\n", NULL);
                }
            }
            // k: Key
            else if (mode == 'k') {
                if (action == '+') {
                    if (paramIdx >= msg.params.size()) continue;
                    std::string key = msg.params[paramIdx++];
                    channel->setKey(key);
                } else {
                    channel->setKey(""); // Quitar clave
                }
                 channel->broadcast(":" + client->getUser()->getPrefix() + " MODE " + target + " " + action + "k" + (action == '+' ? " *" : "") + "\r\n", NULL);
            }
            // l: Limit
            else if (mode == 'l') {
                if (action == '+') {
                    if (paramIdx >= msg.params.size()) continue;
                    // C++98 string to int conversion
                    int limit = std::atoi(msg.params[paramIdx++].c_str());
                    channel->setLimit(limit);
                     channel->broadcast(":" + client->getUser()->getPrefix() + " MODE " + target + " " + action + "l " + msg.params[paramIdx-1] + "\r\n", NULL);
                } else {
                    channel->setLimit(0); // 0 = sin limite
                     channel->broadcast(":" + client->getUser()->getPrefix() + " MODE " + target + " " + action + "l" + "\r\n", NULL);
                }
            }
            // i: Invite Only | t: Topic Restricted
            else if (mode == 'i' || mode == 't') {
                channel->setMode(mode, (action == '+'));
                std::string mStr(1, mode);
                channel->broadcast(":" + client->getUser()->getPrefix() + " MODE " + target + " " + action + mStr + "\r\n", NULL);
            }
        }
    }
}
