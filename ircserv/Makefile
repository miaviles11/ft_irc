NAME = ircserv
BOT_NAME = bot
CXX = c++

# Detectar todas las carpetas dentro de srcs/ para los includes (-I)
# Esto permite hacer #include "Server.hpp" desde main.cpp sin errores
INC_DIRS = $(shell find srcs -type d)
INC_FLAGS = $(addprefix -I,$(INC_DIRS))

CXXFLAGS = -Wall -Wextra -Werror -std=c++98 $(INC_FLAGS) -MMD -MP

# Buscar todos los .cpp automáticamente
SRC = $(shell find srcs -name '*.cpp' ! -path '*/bot/*')
OBJ = $(SRC:.cpp=.o)
DEPS = $(OBJ:.o=.d)

# Archivos del BOT
BOT_SRC = srcs/bot/HelpBot.cpp srcs/bot/main_bot.cpp
BOT_OBJ = $(BOT_SRC:.cpp=.o)
BOT_DEPS = $(BOT_OBJ:.o=.d)

# Colores ANSI
BLUE := \033[34m
GREEN := \033[32m
YELLOW := \033[33m
CYAN := \033[36m
MAGENTA := \033[35m
RESET := \033[0m

# Contador
TOTAL := $(words $(SRC))
BOT_TOTAL := $(words $(BOT_SRC))
CURRENT = 0

.DEFAULT_GOAL := all

all: $(NAME) $(BOT_NAME)
	@printf "$(GREEN)\r✅ Compilación completa [$(TOTAL)/$(TOTAL)]$(RESET)\n"

# Compilar servidor
$(NAME): $(OBJ)
	@printf "$(CYAN)\r🔗 Enlazando servidor: $(NAME)                     $(RESET)\n"
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ)
	@printf "$(GREEN)\r✅ Servidor compilado [$(TOTAL)/$(TOTAL)]         $(RESET)\n"

# Compilar bot
$(BOT_NAME): $(BOT_OBJ)
	@printf "$(MAGENTA)\r🤖 Enlazando bot: $(BOT_NAME)                        $(RESET)\n"
	@$(CXX) $(CXXFLAGS) -o $@ $(BOT_OBJ)
	@printf "$(GREEN)\r✅ Bot compilado [$(BOT_TOTAL)/$(BOT_TOTAL)]           $(RESET)\n"

%.o: %.cpp
	@$(eval CURRENT=$(shell echo $$(($(CURRENT)+1))))
	@printf "$(BLUE)\r⚙️  Compilando [$(CURRENT)/$(TOTAL)]: %-50s$(RESET)" "$<"
	@$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	@printf "$(YELLOW)\r🧹 Limpiando objetos...                  $(RESET)\n"
	@rm -f $(OBJ) $(DEPS) $(BOT_OBJ) $(BOT_DEPS)

fclean: clean
	@printf "$(YELLOW)\r🗑️  Borrando ejecutable...               $(RESET)\n"
	@rm -f $(NAME) $(BOT_NAME)
	@printf "$(GREEN)\r✅ Limpieza completa.                    $(RESET)\n"

re: fclean all

# Ejecutar servidor
run: $(NAME)
	@./$(NAME) 6667 password123

# Ejecutar bot (asume que el servidor ya está corriendo)
run-bot: $(BOT_NAME)
	@./$(BOT_NAME) 127.0.0.1 6667 password123

# Compilar solo el servidor
server: $(NAME)

# Compilar solo el bot
bot: $(BOT_NAME)

-include $(DEPS) $(BOT_DEPS)

.PHONY: all clean fclean re run run-bot server bot