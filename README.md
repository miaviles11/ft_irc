FASE 1: ARRANQUE DEL SERVIDOR
Test 1.1: Inicio básico
bash# Terminal 1 (Servidor)
./ircserv 6667 password123

# ✅ Debes ver:
[SERVER] Initializing on port 6667
[SOCKET] Socket created (fd=3)
[SOCKET] ✓ Socket configured (non-blocking + SO_REUSEADDR)
[SOCKET] ✓ Bound to 0.0.0.0:6667
[SOCKET] ✓ Listening (backlog=4096)
[SERVER] ✓ Ready on port 6667

╔══════════════════════════════════════╗
║   IRC SERVER STARTED                 ║
║   Port: 6667                         ║
║   Press Ctrl+C to exit               ║
╚══════════════════════════════════════╝

[SERVER] Main loop started
Test 1.2: Puerto inválido
bash# Probar puertos inválidos
./ircserv 80 test     # ❌ Puerto privilegiado
./ircserv 70000 test  # ❌ Fuera de rango
./ircserv abc test    # ❌ No numérico

# ✅ Todos deben dar error sin crashear
Test 1.3: Password vacío
bash./ircserv 6667 ""     # ❌ Debe rechazar

# ✅ Debe mostrar: [ERROR] Password cannot be empty

🔐 FASE 2: AUTENTICACIÓN
Test 2.1: Conexión básica
bash# Terminal 2 (USER1)
nc localhost 6667

# ✅ En el servidor debes ver:
[SOCKET] ✓ Accepted connection from 127.0.0.1 (fd=4)
[SERVER] ✓ New client from 127.0.0.1 (fd=4, total=1)
Test 2.2: Password incorrecto
bash# Terminal 2
PASS wrongpassword
NICK TestUser
USER test 0 * :Test

# ✅ Debes ver:
:ft_irc 464 * :Password incorrect
# Y luego se cierra la conexión
Test 2.3: Secuencia correcta (PASS → NICK → USER)
bash# Terminal 2 (Reconectar si se cerró)
nc localhost 6667

PASS password123
NICK Alice
USER alice 0 * :Alice Wonderland

# ✅ Debes ver mensajes de bienvenida:
:ft_irc 001 Alice :Welcome to the FT_IRC Network Alice!alice@127.0.0.1
:ft_irc 002 Alice :Your host is ft_irc, running version 1.0
:ft_irc 003 Alice :This server was created today
:ft_irc 004 Alice ft_irc 1.0 io tkl
Test 2.4: Secuencia incorrecta (sin PASS)
bash# Terminal 3 (nuevo cliente)
nc localhost 6667

NICK Bob
USER bob 0 * :Bob

# ❌ No debe registrarse (sin PASS primero)
# Intenta hacer algo:
JOIN #test

# ✅ Debe ver error de no registrado
Test 2.5: Nickname duplicado
bash# Terminal 3 (con PASS correcto ahora)
nc localhost 6667

PASS password123
NICK Alice    # ❌ Ya existe

# ✅ Debes ver:
:ft_irc 433 * Alice :Nickname is already in use
Test 2.6: Nickname inválido
bash# Terminal 3
PASS password123
NICK Alice@123   # ❌ Carácter inválido
NICK 123Alice    # ❌ Empieza con número (según RFC)  ---> ❌❌❌ ERROR 1

# ✅ Debe rechazar con:
:ft_irc 432 * <nick> :Erroneous nickname
Test 2.7: Cambio de nickname
bash# Terminal 2 (Alice conectada)
NICK AliceNew

# ✅ Debes ver confirmación:
:Alice!alice@127.0.0.1 NICK :AliceNew

# Y el servidor registra el cambio

📢 FASE 3: CANALES BÁSICOS
Test 3.1: Crear y unirse a canal
bash# Terminal 2 (Alice)
JOIN #general

# ✅ Debes ver:
:AliceNew!alice@127.0.0.1 JOIN #general
:ft_irc 331 AliceNew #general :No topic is set
:ft_irc 353 AliceNew = #general :@AliceNew
:ft_irc 366 AliceNew #general :End of /NAMES list

# Nota: @ indica que eres operador (creador del canal)
Test 3.2: Segundo usuario se une
bash# Terminal 3 (Bob - asegúrate de estar registrado)
nc localhost 6667
PASS password123
NICK Bob
USER bob 0 * :Bob Builder
JOIN #general

# ✅ Bob ve:
:Bob!bob@127.0.0.1 JOIN #general
:ft_irc 331 Bob #general :No topic is set
:ft_irc 353 Bob = #general :@AliceNew Bob
:ft_irc 366 Bob #general :End of /NAMES list

# ✅ Alice ve (en Terminal 2):
:Bob!bob@127.0.0.1 JOIN #general
Test 3.3: Enviar mensajes al canal
bash# Terminal 2 (Alice)
PRIVMSG #general :Hello everyone!

# ✅ Bob ve (Terminal 3):
:AliceNew!alice@127.0.0.1 PRIVMSG #general :Hello everyone!

# Terminal 3 (Bob responde)
PRIVMSG #general :Hi Alice!  ---> ❌❌❌ ERROR 2

# ✅ Alice ve:
:Bob!bob@127.0.0.1 PRIVMSG #general :Hi Alice!
Test 3.4: Mensaje privado (usuario a usuario)
bash# Terminal 2 (Alice)
PRIVMSG Bob :This is a private message

# ✅ Bob ve (Terminal 3):
:AliceNew!alice@127.0.0.1 PRIVMSG Bob :This is a private message

# ⚠️ IMPORTANTE: Alice NO debe ver este mensaje reflejado
Test 3.5: Unirse a múltiples canales
bash# Terminal 2 (Alice)
JOIN #random
JOIN #dev

# Terminal 3 (Bob)
JOIN #dev

# Ahora Alice está en: #general, #random, #dev
# Bob está en: #general, #dev
Test 3.6: PART (salir de canal)
bash# Terminal 2 (Alice)
PART #random

# ✅ Alice ve:
:AliceNew!alice@127.0.0.1 PART #random :Leaving

# Probar con razón:
PART #dev :Going to sleep

# ✅ Debes ver:
:AliceNew!alice@127.0.0.1 PART #dev :Going to sleep

# ✅ Bob también ve esto (porque está en #dev)

👑 FASE 4: OPERADORES DE CANAL
Test 4.1: TOPIC (ver y cambiar)
bash# Terminal 2 (Alice - operador de #general)
TOPIC #general

# ✅ Ver topic actual:
:ft_irc 331 AliceNew #general :No topic is set

# Cambiar topic:
TOPIC #general :Welcome to the general channel!

# ✅ Todos en el canal ven:
:AliceNew!alice@127.0.0.1 TOPIC #general :Welcome to the general channel!
Test 4.2: TOPIC (usuario no operador intenta cambiar)
bash# Terminal 3 (Bob - NO es operador)
TOPIC #general :Bob's topic

# ✅ Debe fallar con:
:ft_irc 482 Bob #general :You're not channel operator   ------> ❌❌❌ ERROR 3

# (Solo si el modo +t está activo, que es por defecto)
Test 4.3: MODE +o (dar operador)
bash# Terminal 2 (Alice)
MODE #general +o Bob

# ✅ Todos ven:
:AliceNew!alice@127.0.0.1 MODE #general +o Bob

# Ahora Bob puede cambiar el topic:
# Terminal 3 (Bob)
TOPIC #general :Bob is now OP!

# ✅ Funciona
Test 4.4: KICK (expulsar usuario)
bash# Terminal 4 (Charlie - nuevo usuario)
nc localhost 6667
PASS password123
NICK Charlie
USER charlie 0 * :Charlie
JOIN #general

# Terminal 2 (Alice expulsa a Charlie)
KICK #general Charlie :Bad behavior

# ✅ Todos en el canal ven:
:AliceNew!alice@127.0.0.1 KICK #general Charlie :Bad behavior

# ✅ Charlie (Terminal 4) es expulsado del canal
Test 4.5: INVITE (invitar a canal +i)
bash# Terminal 2 (Alice activa modo invite-only)
MODE #general +i

# ✅ Todos ven:
:AliceNew!alice@127.0.0.1 MODE #general +i

# Terminal 4 (Charlie intenta unirse)
JOIN #general

# ❌ Debe fallar:
:ft_irc 473 Charlie #general :Cannot join channel (+i)

# Terminal 2 (Alice invita a Charlie)
INVITE Charlie #general

# ✅ Alice ve:
:ft_irc 341 AliceNew Charlie #general

# ✅ Charlie ve:
:AliceNew!alice@127.0.0.1 INVITE Charlie #general

# Ahora Charlie puede unirse:
# Terminal 4
JOIN #general

# ✅ Funciona

🔧 FASE 5: MODOS DE CANAL
Test 5.1: MODE +k (canal con contraseña)
bash# Terminal 2 (Alice)
MODE #general +k secretpass

# ✅ Confirmación:
:AliceNew!alice@127.0.0.1 MODE #general +k secretpass

# Terminal 4 (Charlie desconectado, reconecta)
nc localhost 6667
PASS password123
NICK Charlie
USER charlie 0 * :Charlie
JOIN #general

# ❌ Debe fallar:
:ft_irc 475 Charlie #general :Cannot join channel (+k)

# Intentar con clave:
JOIN #general secretpass

# ✅ Funciona
Test 5.2: MODE -k (quitar contraseña)
bash# Terminal 2 (Alice)
MODE #general -k secretpass

# ✅ Confirmación:
:AliceNew!alice@127.0.0.1 MODE #general -k *

# Ahora cualquiera puede unirse sin clave
Test 5.3: MODE +l (límite de usuarios)
bash# Terminal 2 (Alice)
MODE #general +l 3

# ✅ Confirmación:
:AliceNew!alice@127.0.0.1 MODE #general +l 3

# Verificar usuarios actuales:
# Alice, Bob, Charlie = 3 usuarios (límite alcanzado)

# Terminal 5 (nuevo usuario Dave)
nc localhost 6667
PASS password123
NICK Dave
USER dave 0 * :Dave
JOIN #general

# ❌ Debe fallar:
:ft_irc 471 Dave #general :Cannot join channel (+l)
Test 5.4: MODE -l (quitar límite)
bash# Terminal 2 (Alice)
MODE #general -l

# ✅ Confirmación:
:AliceNew!alice@127.0.0.1 MODE #general -l

# Ahora Dave puede unirse:
# Terminal 5
JOIN #general

# ✅ Funciona
Test 5.5: MODE +t (topic restringido a OPs)
bash# Terminal 2 (Alice)
MODE #general -t

# ✅ Ahora Bob (no-OP) puede cambiar el topic:
# Terminal 3 (Bob)
TOPIC #general :Anyone can change this

# ✅ Funciona

# Reactivar restricción:
# Terminal 2 (Alice)
MODE #general +t

# Ahora Bob no puede:
# Terminal 3
TOPIC #general :Bob tries again

# ❌ Falla:
:ft_irc 482 Bob #general :You're not channel operator
Test 5.6: Consultar modos actuales
bash# Cualquier usuario en el canal
MODE #general

# ✅ Debe mostrar:
:ft_irc 324 <nick> #general +it

👤 FASE 6: MODOS DE USUARIO
Test 6.1: MODE +i (invisible)
bash# Terminal 2 (Alice)
MODE Alice +i

# ✅ Confirmación:
:AliceNew!alice@127.0.0.1 MODE AliceNew :+i

# Consultar modo:
MODE Alice

# ✅ Debe mostrar:
:ft_irc 221 AliceNew +i
Test 6.2: MODE -i (visible)
bash# Terminal 2
MODE Alice -i

# ✅ Confirmación:
:AliceNew!alice@127.0.0.1 MODE AliceNew :-i

🔄 FASE 7: MÚLTIPLES USUARIOS Y CANALES
Test 7.1: Broadcast en canal con 3+ usuarios
bash# Setup: Alice, Bob, Charlie en #general

# Terminal 2 (Alice)
PRIVMSG #general :Testing broadcast

# ✅ Bob (Terminal 3) debe ver el mensaje
# ✅ Charlie (Terminal 4) debe ver el mensaje
# ⚠️ Alice NO debe ver su propio mensaje reflejado
Test 7.2: Usuario en múltiples canales recibe solo del canal correcto
bash# Setup:
# Alice: #general, #dev
# Bob: #general
# Charlie: #dev

# Terminal 2 (Alice en #general)
PRIVMSG #general :Message to general

# ✅ Bob lo ve
# ❌ Charlie NO lo ve (no está en #general)

# Terminal 2 (Alice en #dev)
PRIVMSG #dev :Message to dev

# ✅ Charlie lo ve
# ❌ Bob NO lo ve (no está en #dev)
Test 7.3: QUIT propaga a todos los canales
bash# Setup: Alice en #general y #dev con otros usuarios

# Terminal 2 (Alice)
QUIT :Goodbye!

# ✅ Todos los usuarios en #general y #dev ven:
:AliceNew!alice@127.0.0.1 QUIT :Goodbye!

# ✅ En el servidor:
[DISCONNECT] fd=4 (graceful close)

⚠️ FASE 8: CASOS EDGE Y ERRORES
Test 8.1: Comandos sin registro
bash# Terminal nueva sin PASS/NICK/USER
nc localhost 6667

JOIN #test
PRIVMSG #test :Hello
KICK #test Bob

# ✅ Todos deben dar:
:ft_irc 451 * :You have not registered
Test 8.2: Parámetros faltantes
bash# Terminal registrado
JOIN
MODE
KICK #test
INVITE Charlie
TOPIC

# ✅ Todos deben dar:
:ft_irc 461 <nick> <CMD> :Not enough parameters
Test 8.3: Canal inexistente
bashPART #nonexistent
PRIVMSG #nonexistent :Hello

# ✅ Debe dar:
:ft_irc 403 <nick> #nonexistent :No such channel
Test 8.4: Usuario inexistente
bashPRIVMSG NonExistentUser :Hello

# ✅ Debe dar:
:ft_irc 401 <nick> NonExistentUser :No such nick/channel
Test 8.5: Intentar kickear sin ser OP
bash# Terminal 3 (Bob, no-OP)
KICK #general Charlie :Bye

# ✅ Debe dar:
:ft_irc 482 Bob #general :You're not channel operator
Test 8.6: Flood de mensajes (stress test)
bash# Terminal 2
for i in {1..100}; do echo "PRIVMSG #general :Message $i"; done | nc localhost 6667

# ✅ El servidor NO debe crashear
# ✅ Todos los mensajes deben llegar (puede tomar unos segundos)
Test 8.7: Desconexión abrupta (Ctrl+C en cliente)
bash# Terminal 3 (Bob)
# Presionar Ctrl+C sin QUIT

# ✅ En servidor:
[DISCONNECT] fd=X (POLLHUP/ERR)

# ✅ Otros usuarios en los canales de Bob ven:
:Bob!bob@127.0.0.1 QUIT :Connection closed
Test 8.8: Caracteres especiales en mensajes
bashPRIVMSG #general :Test with émojis 🚀 and spëcial çhars

# ✅ Debe transmitirse correctamente

🧪 FASE 9: INTEGRACIÓN CON CLIENTES IRC REALES
Test 9.1: HexChat
bash# 1. Abrir HexChat
# 2. Network List → Add
# 3. Name: "ft_irc_test"
# 4. Servers: localhost/6667
# 5. Edit → Password: password123
# 6. Conectar

# ✅ Debe conectarse exitosamente
# ✅ Debe poder hacer JOIN, PRIVMSG, etc.
Test 9.2: Irssi
bash irssi

# Dentro de irssi:
/server add ft_irc localhost 6667 password123
/connect localhost 6665 pass123 TestUser
/join #general

# ✅ Debe funcionar como servidor real
Test 9.3: WeeChat
bash weechat

# Dentro de weechat:
/server add ft_irc localhost/6667
/set irc.server.ft_irc.password password123
/connect ft_irc
/join #general

# ✅ Debe funcionar



# ❌❌❌ ERROR 1:

Esto deberia ser un error: "NICK 123Alice    # ❌ Empieza con número (según RFC)"
Pero por alguna razón, permite un usuario que empieze con un número.

# ❌❌❌ ERROR 2:

El QUIT ha dejado de funcionar por alguna razón.

# TODO: 
- TIMESTAMPS
- Envio de mensajes bonito
- Mensaje de error cuando se pasa mal la contraseña
- Bot (BONUS)


# USUARIOS DE PRUEBA:

PASS pass123
NICK Alice
USER alice 0 * :Alice

PASS pass123
NICK Bob
USER bob 0 * :Bob

PASS pass123
NICK Charlie
USER charlie 0 * :Charlie

# CANAL DE PRUEBA

JOIN #general

# MENSAJE DE PRUEBA

PRIVMSG #general :HOLAAA