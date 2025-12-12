/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Parser.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: carlsanc <carlsanc@student.42madrid>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/10 19:57:10 by carlsanc          #+#    #+#             */
/*   Updated: 2025/12/10 19:57:10 by carlsanc         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Parser.hpp"
#include <iostream>
#include <algorithm> // para transform

std::string Parser::trim(const std::string& str) {
    std::string result = str;
    // Eliminar \r y \n del final (común en IRC)
    size_t end = result.find_last_not_of("\r\n");
    if (end != std::string::npos)
        result = result.substr(0, end + 1);
    else
        return ""; // String vacío o solo saltos de línea
    return result;
}

std::string Parser::toUpper(const std::string& str) {
    std::string upper = str;
    for (size_t i = 0; i < upper.length(); ++i)
        upper[i] = std::toupper(upper[i]);
    return upper;
}

Message Parser::parse(const std::string& rawLine) {
    Message msg;
    
    // 1. Limpieza básica
    std::string line = trim(rawLine);
    if (line.empty()) return msg;

    size_t pos = 0;
    size_t len = line.length();

    // 2. Parsear Prefijo (Opcional)
    // El prefijo empieza por ':' pero solo si es el PRIMER caracter de la línea
    if (pos < len && line[pos] == ':') {
        size_t spacePos = line.find(' ', pos);
        if (spacePos != std::string::npos) {
            msg.prefix = line.substr(pos + 1, spacePos - pos - 1); // +1 para saltar el ':'
            pos = line.find_first_not_of(' ', spacePos); // Saltar espacios
        } else {
            // Caso raro: Línea solo contiene ":algo" (inválido pero no debe crashear)
            return msg; 
        }
    }

    // Si llegamos al final solo con prefijo, retornamos
    if (pos == std::string::npos || pos >= len) return msg;

    // 3. Parsear Comando
    size_t spacePos = line.find(' ', pos);
    if (spacePos != std::string::npos) {
        msg.command = toUpper(line.substr(pos, spacePos - pos));
        pos = line.find_first_not_of(' ', spacePos);
    } else {
        // Caso: Comando sin parámetros (ej: "QUIT")
        msg.command = toUpper(line.substr(pos));
        return msg;
    }

    // 4. Parsear Parámetros
    while (pos != std::string::npos && pos < len) {
        // Chequear Trailing Parameter (empieza por ':')
        if (line[pos] == ':') {
            // Tomamos TODO el resto de la línea tal cual (sin el ':')
            msg.params.push_back(line.substr(pos + 1));
            break; // No hay más parámetros después del trailing
        }
        
        // Parámetro normal (separado por espacio)
        spacePos = line.find(' ', pos);
        if (spacePos != std::string::npos) {
            msg.params.push_back(line.substr(pos, spacePos - pos));
            pos = line.find_first_not_of(' ', spacePos);
        } else {
            // Último parámetro (sin ':' previo)
            msg.params.push_back(line.substr(pos));
            break;
        }
    }

    return msg;
}
