NAME = ircserv
CXX = c++

# Detectar todas las carpetas dentro de src/ para los includes (-I)
# Esto permite hacer #include "Server.hpp" desde main.cpp sin errores
INC_DIRS = $(shell find src -type d)
INC_FLAGS = $(addprefix -I,$(INC_DIRS))

CXXFLAGS = -Wall -Wextra -Werror -std=c++98 $(INC_FLAGS) -MMD -MP

# Buscar todos los .cpp automáticamente
SRC = $(shell find src -name '*.cpp')
OBJ = $(SRC:.cpp=.o)
DEPS = $(OBJ:.o=.d)

# Colores ANSI
BLUE := \033[34m
GREEN := \033[32m
YELLOW := \033[33m
CYAN := \033[36m
RESET := \033[0m

# Contador
TOTAL := $(words $(SRC))
CURRENT = 0

.DEFAULT_GOAL := all

all: $(NAME)
	@printf "$(GREEN)\r✅ Compilación completa [$(TOTAL)/$(TOTAL)]$(RESET)\n"

$(NAME): $(OBJ)
	@printf "$(CYAN)\r🔗 Enlazando: $(NAME)                     $(RESET)\n"
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ)

%.o: %.cpp
	@$(eval CURRENT=$(shell echo $$(($(CURRENT)+1))))
	@printf "$(BLUE)\r⚙️  Compilando [$(CURRENT)/$(TOTAL)]: %-40s$(RESET)" "$<"
	@$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	@printf "$(YELLOW)\r🧹 Limpiando objetos...                  $(RESET)\n"
	@rm -f $(OBJ) $(DEPS)

fclean: clean
	@printf "$(YELLOW)\r🗑️  Borrando ejecutable...               $(RESET)\n"
	@rm -f $(NAME)
	@printf "$(GREEN)\r✅ Limpieza completa.                    $(RESET)\n"

re: fclean all

run: $(NAME)
	@./$(NAME) 6667 password123

-include $(DEPS)

.PHONY: all clean fclean re run