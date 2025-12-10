#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <string>
#include <vector>

struct Message {
    std::string prefix;      // Opcional (ej: :nick!user@host)
    std::string command;     // El comando (ej: PRIVMSG, JOIN)
    std::vector<std::string> params; // Argumentos (ej: #canal)

    // Helper para depuración
    bool isValid() const { return !command.empty(); }
};

#endif
