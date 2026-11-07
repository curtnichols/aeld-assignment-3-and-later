CROSS_COMPILE ?=
# Suggested command-line variable: aarch64-none-linux-gnu-

CC  ?= $(CROSS_COMPILE)gcc
CXX ?= $(CROSS_COMPILE)g++
LD  ?= $(CROSS_COMPILE)ld

SRC_DIR ?= .
OBJ_DIR ?= .
BIN_DIR ?= .

TARGET = $(BIN_DIR)/aesdsocket

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

CFLAGS += -g -Wall -Werror
#CFLAGS += -g -Wall -Werror -fsanitize=address

LDFLAGS ?=

$(info TARGET is $(TARGET))

# Explicitly declare phony targets so Make doesn't get confused by local files/folders
.PHONY: all clean

# Point 'all' to $(TARGET). This forces Make to evaluate the $(TARGET) rule block below.
all : $(TARGET)

# Create a list of .d dependency files mapping to each object file
DEPS = $(OBJS:.o=.d)

# Linking the final executable
$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $(OBJS) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

-include $(DEPS)

clean :
	rm -f $(TARGET) $(OBJS) $(DEPS)

