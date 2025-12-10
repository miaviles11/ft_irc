#include "Parser.hpp"
#include <sstream>
#include <iostream>

Message Parser::parse(const std::string& rawLine) {
    Message msg;
    std::string line = rawLine;

    if (line.empty()) return msg;

    std::stringstream ss(line);
    std::string segment;

    // 2. Detectar Prefijo (empieza por :)
    if (line[0] == ':') {
        ss >> msg.prefix;
        msg.prefix.erase(0, 1); // Quitar el ':'
    }

    // 3. Extraer Comando
    if (ss >> msg.command) {
        // Convertir comando a UPPERCASE (JOIN es igual a join)
        for (size_t i = 0; i < msg.command.length(); ++i)
            msg.command[i] = std::toupper(msg.command[i]);
    }

    // 4. Extraer Parámetros
    while (ss >> segment) {
        if (segment[0] == ':') {
            // buscar la posición del primer ':' después del comando
            size_t pos = line.find(" :", 0); // Busca espacio+dosPuntos
            if (pos != std::string::npos) {
                msg.params.push_back(line.substr(pos + 2)); // +2 para saltar " :"
            }
            break;
        } else {
            msg.params.push_back(segment);
        }
    }
    return msg;
}
