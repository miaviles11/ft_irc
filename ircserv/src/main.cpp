/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: miaviles <miaviles@student.42madrid>       +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/03 15:52:03 by miaviles          #+#    #+#             */
/*   Updated: 2025/12/03 18:17:10 by miaviles         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include <iostream>
#include <cstdlib>
#include <csignal>

Server* g_server = NULL; //* GLOBAL VARIABLE TO HANDLE SIGNALS

void signalHandler(int signum)
{
    std::cout << "\n[SIGNAL] Received signal " << signum << std::endl;
    if (g_server) 
	{
        delete g_server;
        g_server = NULL;
    }
    exit(signum);
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
    signal(SIGINT, signalHandler);   //* Ctrl+C
    signal(SIGTERM, signalHandler);  //* kill
    signal(SIGPIPE, SIG_IGN);        //* Ignore SIGPIPE (closed connection)
    
    //* CREATE AND START SERVER
    g_server = new Server(port, password);
    
    if (!g_server->start()) {
        std::cerr << "[FATAL] Could not start server\n";
        delete g_server;
        return (1);
    }
    
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════╗\n";
    std::cout << "║   IRC SERVER STARTED                 ║\n";
    std::cout << "║   Port: " << port << "               ║\n";
    std::cout << "║   Press Ctrl+C to exit               ║\n";
    std::cout << "╚══════════════════════════════════════╝\n\n";
    
    g_server->run(); //* Main loop
    
    delete g_server; //* Cleanup
    g_server = NULL;
    
    return (0);
}