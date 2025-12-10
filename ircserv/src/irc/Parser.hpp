#ifndef PARSER_HPP
#define PARSER_HPP

#include "Message.hpp"
#include <string>

class Parser {
    public:
        // Método estático: entra string sucio, sale estructura limpia
        static Message parse(const std::string& rawLine);
    private:
        Parser(); // No instanciable
};

#endif
