NAME = ircserv
CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98 -Iinclude
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
	@printf "$(GREEN)\n✅ Compilación completa [$(TOTAL)/$(TOTAL)]$(RESET)\n"

$(NAME): $(OBJ)
	@printf "$(CYAN)\r🔗 Enlazando: $(NAME)                     $(RESET)\n"
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ)

%.o: %.cpp
	@$(eval CURRENT=$(shell echo $$(($(CURRENT)+1))))
	@printf "$(BLUE)\r⚙️  Compilando [$(CURRENT)/$(TOTAL)]: %-40s$(RESET)" "$<"
	@$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

clean:
	@printf "$(YELLOW)\r🧹 Limpiando objetos...                $(RESET)\n"
	@rm -f $(OBJ) $(DEPS)

fclean: clean
	@printf "$(YELLOW)\r🗑️  Borrando ejecutable...              $(RESET)\n"
	rm -f $(NAME)
	@printf "$(GREEN)\r✅  Limpieza completa.                  $(RESET)\n"

re: fclean all

run: $(NAME)
	@./$(NAME)

-include $(DEPS)

.PHONY: all clean fclean re run
