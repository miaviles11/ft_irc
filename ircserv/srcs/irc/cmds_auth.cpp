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
#include "../utils/Colors.hpp"
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
        sendServerNotice(client, std::string(CYAN) + "*** Syntax: PASS <password>" + RESET);
        return sendError(client, ERR_NEEDMOREPARAMS, "PASS");
    }
    
    if (client->isRegistered())
    {
        sendServerNotice(client, "*** You are already registered");
        return sendError(client, ERR_ALREADYREGISTRED, "");
    }

    if (msg.params[0] != this->password_)
    {
        // ❌ CONTRASEÑA INCORRECTA
        sendServerNotice(client, "");
        sendServerNotice(client, std::string(BRIGHT_RED) + "╔════════════════════════════════════════════════╗" + RESET);
        sendServerNotice(client, std::string(BRIGHT_RED) + "║                                                ║" + RESET);
        sendServerNotice(client, std::string(BRIGHT_RED) + "║  ✗  AUTHENTICATION FAILED                      ║" + RESET);
        sendServerNotice(client, std::string(BRIGHT_RED) + "║                                                ║" + RESET);
        sendServerNotice(client, std::string(BRIGHT_RED) + "║  Password incorrect.                           ║" + RESET);
        sendServerNotice(client, std::string(BRIGHT_RED) + "║  Connection will be closed.                    ║" + RESET);
        sendServerNotice(client, std::string(BRIGHT_RED) + "║                                                ║" + RESET);
        sendServerNotice(client, std::string(BRIGHT_RED) + "╚════════════════════════════════════════════════╝" + RESET);
        sendServerNotice(client, "");
        sendError(client, ERR_PASSWDMISMATCH, "");
        sendPendingData(client);
         // Log del servidor
        std::cout << RED << "[AUTH] ✗ Password incorrect (fd=" 
          << client->getFd() << ")" << RESET << std::endl;
        client->closeConnection();
        return;
    }

    // Password accepted
    client->markPassReceived();
    std::cout << BRIGHT_GREEN << "[AUTH] ✓ Password accepted (fd=" 
              << client->getFd() << ")" << RESET << std::endl;
    sendServerNotice(client, "");
    sendServerNotice(client, std::string(BRIGHT_GREEN) + "╔════════════════════════════════════════════════╗" + RESET);
    sendServerNotice(client, std::string(BRIGHT_GREEN) + "║                                                ║" + RESET);
    sendServerNotice(client, std::string(BRIGHT_GREEN) + "║  ✓  PASSWORD ACCEPTED                          ║" + RESET);
    sendServerNotice(client, std::string(BRIGHT_GREEN) + "║                                                ║" + RESET);
    sendServerNotice(client, std::string(BRIGHT_GREEN) + "╚════════════════════════════════════════════════╝" + RESET);
    sendServerNotice(client, "");
    sendServerNotice(client, std::string(BRIGHT_GREEN) + "*** Password accepted. Please identify yourself:" + RESET);
    sendServerNotice(client, std::string(CYAN) + "*** Use: NICK <your_nickname>" + RESET);
    sendServerNotice(client, std::string(CYAN) + "*** Then: USER <username> 0 * :<realname>" + RESET);
    sendServerNotice(client, "");
}

void Server::cmdNick(ClientConnection* client, const Message& msg)
{
    if (msg.params.empty())
    {
        sendServerNotice(client, std::string(CYAN) + "*** Syntax: NICK <nickname>" + RESET);
        return sendError(client, ERR_NONICKNAMEGIVEN, "");
    }

    std::string newNick = msg.params[0];

    // Verificar longitud máxima (9 caracteres)
    if (newNick.length() > 9)
    {
        sendServerNotice(client, std::string(BRIGHT_RED) + "* ERROR: Nickname too long (max 9 chars)" + RESET);
        return sendError(client, ERR_ERRONEUSNICKNAME, newNick);
    }

    // Verificar que no empiece con dígito o '-'
    if (std::isdigit(newNick[0]) || newNick[0] == '-')
    {
        sendServerNotice(client, std::string(BRIGHT_RED) + "*** ERROR: Nickname cannot start with a digit or '-'" + RESET);
        return sendError(client, ERR_ERRONEUSNICKNAME, newNick);
    }

    // Caracteres permitidos (RFC 2812)
    if (newNick.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789[]{}\\|-_^") != std::string::npos)
    {
        sendServerNotice(client, std::string(BRIGHT_RED) + "*** ERROR: Invalid nickname. Use only letters, numbers, and -_[]{}\\|^" + RESET);
        return sendError(client, ERR_ERRONEUSNICKNAME, newNick);
    }

    // Verificar si ya existe el nickname en el servidor
    for (size_t i = 0; i < clients_.size(); ++i)
    {
        if (clients_[i] != client && clients_[i]->getUser() && clients_[i]->getUser()->getNickname() == newNick)
        {
            sendServerNotice(client, std::string(BRIGHT_RED) + "*** ERROR: Nickname '" + newNick + "' is already in use. Try another." + RESET);
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
        
        sendServerNotice(client, std::string(BRIGHT_GREEN) + "*** Nickname changed to: " + MAGENTA + newNick + RESET);
    }
    else if (!client->hasSentPass())
    {
        sendServerNotice(client, std::string(YELLOW) + "*** Please authenticate first with: PASS <password>" + RESET);
    }
    else
    {
        sendServerNotice(client, std::string(BRIGHT_GREEN) + "*** Nickname set to: " + MAGENTA + newNick + RESET);
        sendServerNotice(client, std::string(CYAN) + "*** Next step: USER <username> 0 * :<realname>" + RESET);
    }

    // Aplicar el cambio
    client->getUser()->setNickname(newNick);
    checkRegistration(client);
}

void Server::cmdUser(ClientConnection* client, const Message& msg)
{
    if (client->isRegistered())
    {
        sendServerNotice(client, std::string(YELLOW) + "*** You are already registered" + RESET);
        return sendError(client, ERR_ALREADYREGISTRED, "");
    }

    if (msg.params.size() < 4)
    {
        sendServerNotice(client, std::string(CYAN) + "*** Syntax: USER <username> 0 * :<realname>" + RESET);
        return sendError(client, ERR_NEEDMOREPARAMS, "USER");
    }

    if (!client->hasSentPass())
    {
        sendServerNotice(client, std::string(YELLOW) + "*** Please authenticate first with: PASS <password>" + RESET);
        return;
    }

    User* user = client->getUser();
    user->setUsername(msg.params[0]);
    user->setRealname(msg.params[3]);
    
    // Mensaje de bienvenida
    sendServerNotice(client, "");
    sendServerNotice(client, std::string(BRIGHT_GREEN) + "╔════════════════════════════════════════════════════════════╗" + RESET);
    sendServerNotice(client, std::string(BRIGHT_GREEN) + "║                                                            ║" + RESET);
    sendServerNotice(client, std::string(BRIGHT_GREEN) + "║  ✓  REGISTRATION COMPLETE!                                 ║" + RESET);
    sendServerNotice(client, std::string(BRIGHT_GREEN) + "║                                                            ║" + RESET);
    sendServerNotice(client, std::string(BRIGHT_GREEN) + "╚════════════════════════════════════════════════════════════╝" + RESET);
    sendServerNotice(client, "");
    sendServerNotice(client, std::string(BRIGHT_CYAN) + "    Welcome to ft_irc, " + BRIGHT_MAGENTA + user->getNickname() + BRIGHT_CYAN + "!" + RESET);
    sendServerNotice(client, "");
    sendServerNotice(client, std::string(CYAN) + "    Your identity:" + RESET);
    sendServerNotice(client, std::string(CYAN) + "      • Nickname : " + BRIGHT_MAGENTA + user->getNickname() + RESET);
    sendServerNotice(client, std::string(CYAN) + "      • Username : " + YELLOW + user->getUsername() + RESET);
    sendServerNotice(client, std::string(CYAN) + "      • Realname : " + YELLOW + user->getRealname() + RESET);
    sendServerNotice(client, "");
    sendServerNotice(client, std::string(BRIGHT_YELLOW) + "    ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" + RESET);
    sendServerNotice(client, "");
    sendServerNotice(client, std::string(BRIGHT_WHITE) + "    Available commands:" + RESET);
    sendServerNotice(client, "");
    sendServerNotice(client, std::string(CYAN) + "      📢  JOIN #channel        " + RESET + "→ Join a channel");
    sendServerNotice(client, std::string(CYAN) + "      💬  PRIVMSG #chan :msg   " + RESET + "→ Send message to channel");
    sendServerNotice(client, std::string(CYAN) + "      💬  PRIVMSG nick :msg    " + RESET + "→ Send private message");
    sendServerNotice(client, std::string(CYAN) + "      🚪  PART #channel        " + RESET + "→ Leave a channel");
    sendServerNotice(client, std::string(CYAN) + "      📝  TOPIC #chan :topic   " + RESET + "→ Change channel topic");
    sendServerNotice(client, std::string(CYAN) + "      ⚙️   MODE #chan +o nick   " + RESET + "→ Give operator status");
    sendServerNotice(client, std::string(CYAN) + "      👢  KICK #chan nick      " + RESET + "→ Kick user from channel");
    sendServerNotice(client, std::string(CYAN) + "      📨  INVITE nick #chan    " + RESET + "→ Invite user to channel");
    sendServerNotice(client, std::string(CYAN) + "      👋  QUIT :reason         " + RESET + "→ Disconnect from server");
    sendServerNotice(client, "");
    sendServerNotice(client, std::string(BRIGHT_YELLOW) + "    ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" + RESET);
    sendServerNotice(client, "");
    sendServerNotice(client, std::string(BRIGHT_GREEN) + "    Type " + BRIGHT_WHITE + "JOIN #general" + BRIGHT_GREEN + " to get started!" + RESET);
    sendServerNotice(client, "");
    
    // Log del servidor
    std::cout << BRIGHT_GREEN << "[REGISTER] ✓ User registered: " 
              << BRIGHT_MAGENTA << user->getNickname() 
              << RESET << " (" << user->getUsername() << ")" 
              << " (fd=" << client->getFd() << ")" << RESET << std::endl;
    
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
