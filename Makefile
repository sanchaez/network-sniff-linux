BUILD_DIR= build

# Link targets
DAEMON_LINK_TARGET= $(BUILD_DIR)/netsniffd.app
CONTROL_LINK_TARGET= $(BUILD_DIR)/netsniff.app
DAEMON_SRC_DIR= daemon
CONTROL_SRC_DIR= control
SHARED_DIR= shared
DAEMON_PCH_H = $(DAEMON_SRC_DIR)/stdafx.h
DAEMON_PCH = $(DAEMON_SRC_DIR)/stdafx.h.gch
DAEMON_PCH_INCLUDES = $(SHARED_DIR)/custom_com_def.h $(DAEMON_SRC_DIR)/capture_module.h

# Store object files in the BUILD_DIR
DAEMON_OBJ_DIR= $(BUILD_DIR)/$(DAEMON_SRC_DIR)_obj
CONTROL_OBJ_DIR= $(BUILD_DIR)/$(CONTROL_SRC_DIR)_obj
DAEMON_OBJ= $(addprefix $(DAEMON_OBJ_DIR)/, main.o capture_module.o)
CONTROL_OBJ= $(addprefix $(CONTROL_OBJ_DIR)/, main.o)

# Compiler options
CC= gcc
CFLAGS= -Wall -Werror -g -pthread -I$(SHARED_DIR)

# phony targets
.PHONY: all daemon control run clean

all: daemon control
	@echo All targets built.

daemon: $(DAEMON_PCH) $(DAEMON_OBJ_DIR) $(DAEMON_LINK_TARGET)
	@echo $(DAEMON_LINK_TARGET) - daemon build successful.

control: $(CONTROL_OBJ_DIR) $(CONTROL_LINK_TARGET)
	@echo $(CONTROL_LINK_TARGET) - CLI app build successful.

# Run program stack
run:
	$(DAEMON_LINK_TARGET)
	$(CONTROL_LINK_TARGET)

clean:
	@echo Build cleaned.
	@rm -rf $(BUILD_DIR)
	@rm -f $(DAEMON_PCH)

$(BUILD_DIR) $(DAEMON_OBJ_DIR) $(CONTROL_OBJ_DIR):
	@echo Creating $@ directory...
	@mkdir -p $@

# Linking executables
$(DAEMON_LINK_TARGET): $(DAEMON_OBJ) 
	@echo Linking $@...
	@$(CC) $(CFLAGS) -o $@ $^

$(CONTROL_LINK_TARGET): $(CONTROL_OBJ)
	@echo Linking $@...
	@$(CC) $(CFLAGS) -o $@ $^

# Outputting obj files to right directory
$(DAEMON_OBJ_DIR)/%.o: $(DAEMON_SRC_DIR)/%.c
	@echo Compiling $@...
	@$(CC) -c $(CFLAGS) $< -o $@

$(CONTROL_OBJ_DIR)/%.o: $(CONTROL_SRC_DIR)/%.c
	@echo Compiling $@...
	@$(CC) -c $(CFLAGS) $< -o $@

# PCH
$(DAEMON_PCH): $(DAEMON_PCH_H) $(DAEMON_PCH_INCLUDES) 
	@echo Creating PCH for $@
	@$(CC) -c $(CFLAGS) $< -o $@


