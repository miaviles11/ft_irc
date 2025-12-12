/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: carlsanc <carlsanc@student.42madrid>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/03 15:52:03 by miaviles          #+#    #+#             */
/*   Updated: 2025/12/10 19:51:36 by carlsanc         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "server/Server.hpp"
#include <iostream>
#include <cstdlib>
#include <csignal>

// Puntero global para acceder al servidor desde el handler de señales.
// Se usa SOLO para llamar a stop(), no para borrar memoria.
Server* g_server = NULL;

// Manejador de señales seguro (Async-Signal-Safe)
// No debe contener 'delete', 'cout', 'malloc', etc.
void signalHandler(int signum)
{
    (void)signum; // Silenciar warning de variable no usada
    if (g_server) 
    {
        // Solo indicamos al servidor que detenga su bucle principal.
        // La memoria se liberará en el main() después de que run() retorne.
        g_server->stop();
    }
}

bool isValidPort(int port)
{
    return (port > 1024 && port < 65536);
}

int main(int argc, char **argv)
{
    //* ARGUMENT VALIDATION
    if (argc != 3) 
    {
        std::cerr << "Usage: " << argv[0] << " <port> <password>\n";
        std::cerr << "  port: 1025-65535\n";
        std::cerr << "  password: connection password\n";
        return (1);
    }
    
    int port = std::atoi(argv[1]);
    if (!isValidPort(port)) {
        std::cerr << "[ERROR] Invalid port. Use 1025-65535\n";
        return (1);
    }
    
    std::string password = argv[2];
    if (password.empty()) {
        std::cerr << "[ERROR] Password cannot be empty\n";
        return (1);
    }
    
    //* CONFIGURE SIGNALS
    // SIGINT (Ctrl+C) y SIGTERM son las señales estándar de terminación
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // SIGPIPE es crucial en servidores de red. Si un cliente cierra la conexión
    // mientras intentamos escribirle, el OS envía SIGPIPE que crashea el programa
    // por defecto. SIG_IGN hace que send() devuelva error (EPIPE) en su lugar.
    signal(SIGPIPE, SIG_IGN);
    
    //* CREATE AND START SERVER
    g_server = new Server(port, password);
    
    if (!g_server->start()) {
        std::cerr << "[FATAL] Could not start server\n";
        delete g_server; // Limpieza temprana si falla el inicio
        return (1);
    }
    
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════╗\n";
    std::cout << "║   IRC SERVER STARTED                 ║\n";
    std::cout << "║   Port: " << port << "               ║\n";
    std::cout << "║   Press Ctrl+C to exit               ║\n";
    std::cout << "╚══════════════════════════════════════╝\n\n";
    
    // El programa se bloqueará aquí dentro del bucle while(running_)
    g_server->run(); 
    
    //* CLEANUP
    // Cuando g_server->stop() es llamado (por señal), run() termina y llegamos aquí.
    // Es seguro hacer delete y cout aquí porque estamos en el hilo principal,
    // no dentro de la interrupción de la señal.
    std::cout << "\n[MAIN] Stopping server..." << std::endl;
    delete g_server;
    g_server = NULL;
    
    std::cout << "[MAIN] Server stopped cleanly." << std::endl;
    return (0);
}