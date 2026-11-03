CROSS_COMPILE ?=
# Suggested command-line variable: aarch64-none-linux-gnu-

CC  ?= $(CROSS_COMPILE)gcc
CXX ?= $(CROSS_COMPILE)g++
LD  ?= $(CROSS_COMPILE)ld

SRC_DIR = .
OBJ_DIR = .
BIN_DIR = .

TARGET = $(BIN_DIR)/aesdsocket

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

CFLAGS ?= -g -Wall -Werror
#CFLAGS ?= -g -Wall -Werror -fsanitize=address

LDFLAGS ?=

all : $(TARGET)

# Linking the final executable
$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(OBJS) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean : 
	rm -f $(TARGET) $(OBJS)

