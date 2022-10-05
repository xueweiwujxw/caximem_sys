SRC_DIR = .
SRC_SUBDIR += . 
INCLUDE_DIR += .
BUILD_DIR = ./build
OBJ_DIR = $(BUILD_DIR)/obj
ARCH=arm

TARGET = $(addprefix $(BUILD_DIR)/$(ARCH)/, ${notdir $(CURDIR)})

CC = arm-linux-gnueabihf-gcc
C_FLAGS = -g -Wall
LD = $(CC)
INCLUDES += -I$(INCLUDE_DIR)
LD_FLAGS += -lpthread
LD_LIBS =

ifneq ($(findstring $(CC), g++),)
	TYPE = cpp
else
	TYPE = c
endif

SRCS += ${foreach subdir, $(SRC_SUBDIR), ${wildcard $(SRC_DIR)/$(subdir)/*.$(TYPE)}}
OBJS += ${foreach src, $(notdir $(SRCS)), ${patsubst %.$(TYPE), $(OBJ_DIR)/%.o, $(src)}}

vpath %.$(TYPE) $(sort $(dir $(SRCS)))

all : $(TARGET)
	@echo "Builded target:" $^
	@echo "Done"

$(TARGET) : $(OBJS)
	@mkdir -p $(@D)
	@echo "Linking" $@ "from" $^ "..."
	$(LD) -o $@ $^ $(LD_FLAGS) $(LD_LIBS)
	@echo "Link finished\n"

$(OBJS) : $(OBJ_DIR)/%.o:%.$(TYPE)
	@mkdir -p $(@D)
	@echo "Compiling" $@ "from" $< "..."
	$(CC) -c -o $@ $< $(C_FLAGS) $(INCLUDES)
	@echo "Compile finished\n"

.PHONY : clean cleanobj
clean : cleanobj
	@echo "Remove all build files"
	rm -rf $(BUILD_DIR)
cleanobj :
	@echo "Remove object files"
	rm -rf $(OBJ_DIR)/*.o
