BUILD_DIR= build

# Link targets
DAEMON_LINK_TARGET= $(BUILD_DIR)/netsniffd.app
CONTROL_LINK_TARGET= $(BUILD_DIR)/netsniff.app
DAEMON_SRC_DIR= daemon
CONTROL_SRC_DIR= control

# Store object files in the BUILD_DIR
DAEMON_OBJ_DIR= $(BUILD_DIR)/$(DAEMON_SRC_DIR)_obj
CONTROL_OBJ_DIR= $(BUILD_DIR)/$(CONTROL_SRC_DIR)_obj
DAEMON_OBJ= $(addprefix $(DAEMON_OBJ_DIR)/, main.o)
CONTROL_OBJ= $(addprefix $(CONTROL_OBJ_DIR)/, main.o)

# Compiler options
CC= gcc
CFLAGS= -Wall -Werror -g

# phony targets
.PHONY: all daemon control run clean

all: daemon control
	@echo All targets built.

daemon: $(DAEMON_OBJ_DIR) $(DAEMON_LINK_TARGET)
	@echo $(DAEMON_LINK_TARGET) - daemon build successful.

control: $(CONTROL_OBJ_DIR) $(CONTROL_LINK_TARGET)
	@echo $(CONTROL_LINK_TARGET) - CLI app build successful.

# Run program stack
# FIXME: do a proper daemon and cli modules
run:
	$(DAEMON_LINK_TARGET)
	$(CONTROL_LINK_TARGET)

clean:
	@echo Build directory removed.
	@rm -rf $(BUILD_DIR)

$(BUILD_DIR) $(DAEMON_OBJ_DIR) $(CONTROL_OBJ_DIR):
	@echo Creating $@ directory...
	@mkdir -p $@

# Linking executables
$(DAEMON_LINK_TARGET): $(DAEMON_OBJ)
	@echo Linking $@...
	@$(CC) $(CDEBUGFLAGS) -o $@ $^

$(CONTROL_LINK_TARGET): $(CONTROL_OBJ)
	@echo Linking $@...
	@$(CC) $(CDEBUGFLAGS) -o $@ $^

# Outputting obj files to right directory
$(DAEMON_OBJ_DIR)/%.o: $(DAEMON_SRC_DIR)/%.c
	@echo Compiling $@...
	@$(CC) -c $(CFLAGS) $< -o $@

$(CONTROL_OBJ_DIR)/%.o: $(CONTROL_SRC_DIR)/%.c
	@echo Compiling $@...
	@$(CC) -c $(CFLAGS) $< -o $@

# Dependancy checks
# TODO: Add dependancy targets
