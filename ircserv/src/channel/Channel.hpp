#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <string>
#include <vector>
#include <set>
#include <algorithm>

// Forward declaration para evitar dependencias circulares
class User;

class Channel
{
    public:
        // Constructor y Destructor
        Channel(const std::string& name);
        ~Channel();

        // ------------------------------------------------------------------
        // GETTERS BÁSICOS
        // ------------------------------------------------------------------
        const std::string& getName() const;
        const std::string& getTopic() const;
        const std::string& getKey() const;
        
        // Límites y Conteo
        size_t getUserCount() const;
        int    getLimit() const;
        
        // ------------------------------------------------------------------
        // MODOS DEL CANAL (+i, +t, +k, +l)
        // ------------------------------------------------------------------
        std::string getModes() const;           // Devuelve string tipo "+itk" para RPL_CHANNELMODEIS
        bool        hasMode(char mode) const;
        void        setMode(char mode, bool active);
        
        void        setKey(const std::string& key);
        void        setLimit(int limit);
        void        setTopic(const std::string& topic);

        // ------------------------------------------------------------------
        // GESTIÓN DE MIEMBROS
        // ------------------------------------------------------------------
        void    addMember(User* user);
        void    removeMember(User* user);
        bool    isMember(User* user) const;
        User* getMember(const std::string& nick) const;

        /**
         * [IMPORTANTE] NECESARIO PARA EL COMANDO NICK (Evitar Spam)
         * Devuelve la lista completa de usuarios para poder iterar y filtrar
         * a quién enviamos notificaciones globales.
         * Que nadie toque esto >:(
         */
        const std::vector<User*>& getMembers() const;

        // ------------------------------------------------------------------
        // GESTIÓN DE OPERADORES (+o)
        // ------------------------------------------------------------------
        void    addOperator(User* user);
        void    removeOperator(User* user);
        bool    isOperator(User* user) const;

        // ------------------------------------------------------------------
        // GESTIÓN DE INVITACIONES (+i)
        // ------------------------------------------------------------------
        void    addInvite(const std::string& nick);
        bool    isInvited(User* user) const; // Verifica si el usuario está en la lista blanca

        // ------------------------------------------------------------------
        // COMUNICACIÓN
        // ------------------------------------------------------------------
        /**
         * Broadcast a message to all channel members.
         * 
         * Sends the message to all members in the channel.
         * If excludeUser is NULL, the message is sent to everyone.
         * If excludeUser is a valid User pointer, that user is skipped.
         * 
         * @param msg Message to send (must end with \r\n for IRC protocol)
         * @param excludeUser User to skip sending to (NULL = send to everyone)
         */
        void    broadcast(const std::string& msg, User* excludeUser);
        
        // Genera la lista de nombres para RPL_NAMREPLY (ej: "@Admin +User1 User2")
        std::string getNamesList() const;

    private:
        std::string _name;
        std::string _topic;
        std::string _key;       // Contraseña del canal (+k)
        int         _limit;     // Límite de usuarios (+l), 0 = sin límite

        // Flags de modos booleanos
        bool _inviteOnly;       // +i
        bool _topicOpOnly;      // +t
        bool _hasKey;           // +k activado
        bool _hasLimit;         // +l activado

        // Listas internas
        std::vector<User*>    _members;   // Todos los usuarios dentro
        std::vector<User*>    _operators; // Subconjunto de usuarios que son OP
        std::set<std::string> _invites;   // Nicks invitados (whitelist para +i)

        // Constructor privado para prohibir canales sin nombre
        Channel(); 
};

#endif