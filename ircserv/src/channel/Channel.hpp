/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Channel.hpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: carlsanc <carlsanc@student.42madrid>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/10 19:31:06 by carlsanc          #+#    #+#             */
/*   Updated: 2025/12/10 19:31:07 by carlsanc         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <string>
#include <vector>
#include <algorithm>
#include <iostream>

class User;

/**
 * Channel: Represents an IRC Channel
 * * Responsibilities:
 * - Manage list of members (users inside the channel)
 * - Manage list of operators (users with privileges)
 * - Manage channel modes (+i, +t, +k, +l)
 * - Broadcast messages to members
 */
class Channel {
    public:
        Channel(const std::string& name);
        ~Channel();

        /* Basic Info */
        const std::string& getName() const;
        const std::string& getTopic() const;
        void setTopic(const std::string& topic);
        
        /* Key / Password (+k) */
        const std::string& getKey() const;
        void setKey(const std::string& key);

        /* User Limit (+l) */
        void setLimit(int limit);
        int getLimit() const;
        size_t getUserCount() const;

        /* Modes Management */
        void setMode(char mode, bool active);
        bool hasMode(char mode) const;
        std::string getModes() const; // Returns string like "+itk"

        /* Membership Management */
        void addMember(User* user);
        void removeMember(User* user);
        bool isMember(User* user) const;
        User* getMember(const std::string& nickname); // Find user by nick inside channel

        /* Operator Management (+o) */
        void addOperator(User* user);
        void removeOperator(User* user);
        bool isOperator(User* user) const;

        /* Invite List (+i) */
        void addInvite(const std::string& nickname);
        bool isInvited(User* user) const; // Checks if user's nick is in invite list

        /* Communication */
        // Sends a message to all members EXCEPT 'excludeUser' (usually the sender)
        void broadcast(const std::string& message, User* excludeUser);

        /* Utils */
        // Returns string for RPL_NAMREPLY (e.g. "@Admin User1 User2")
        std::string getNamesList() const;

    private:
        std::string _name;
        std::string _topic;
        std::string _key;
        int _limit;

        /* Modes State */
        bool _inviteOnly;       // +i
        bool _topicRestricted;  // +t
        bool _keyMode;          // +k
        bool _limitMode;        // +l

        /* Lists */
        std::vector<User*> _members;
        std::vector<User*> _operators;
        std::vector<std::string> _invites; // List of invited nicknames

        /* Helpers */
        // Forbidden to copy channels
        Channel();
        Channel(const Channel&);
        Channel& operator=(const Channel&);
};

#endif
