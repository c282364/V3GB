TARGET_NAME=gb28181
BUILD_DIR=../build
TARGET_DIR=../lib/linux/lnx_64

OBJ_PATH=$(BUILD_DIR)/obj
LIB_PATH=$(TARGET_DIR)
TARGET=$(LIB_PATH)/$(TARGET_NAME)

SRC_PATH1=../GB28181
SRC_PATH2=../GB28181/psmux
SRC_PATH3=../GB28181/SipUA
SRC_PATH4=../GB28181/Common
SRC_PATH5=../GB28181/tinyxml
SRC_PATH6=../GB28181/ConfigFile

#从源文件(.c .cpp... )中提取目标文件(.o)的文件名
SRC1=$(wildcard $(SRC_PATH1)/*.cpp)		
OBJ1=$(patsubst $(SRC_PATH1)/%.cpp, $(OBJ_PATH)/%.o, $(SRC1))

SRC2=$(wildcard $(SRC_PATH2)/*.cpp)		
OBJ2=$(patsubst $(SRC_PATH2)/%.cpp, $(OBJ_PATH)/%.o, $(SRC2))

SRC3=$(wildcard $(SRC_PATH3)/*.cpp)		
OBJ3=$(patsubst $(SRC_PATH3)/%.cpp, $(OBJ_PATH)/%.o, $(SRC3))

SRC4=$(wildcard $(SRC_PATH4)/*.cpp)		
OBJ4=$(patsubst $(SRC_PATH4)/%.cpp, $(OBJ_PATH)/%.o, $(SRC4))

SRC5=$(wildcard $(SRC_PATH5)/*.cpp)		
OBJ5=$(patsubst $(SRC_PATH5)/%.cpp, $(OBJ_PATH)/%.o, $(SRC5))

SRC6=$(wildcard $(SRC_PATH6)/*.cpp)		
OBJ6=$(patsubst $(SRC_PATH6)/%.cpp, $(OBJ_PATH)/%.o, $(SRC6))



#提取需要链接库的库名
#lib1=$(wildcard ../GB28181/jsoncpp/lib_release/lnx_x64/*.so)
#lso1=$(patsubst ../GB28181/jsoncpp/lib_release/lnx_x64/lib%.so,-l%, $(lib1))

EXOSIP_LIBS = -ljsoncpp -leXosip2 -losip2 -losipparser2

SYS_LIBS = -lpthread -lssl -lm -lrt -lcrypto -lcares

LIBS = $(EXOSIP_LIBS) $(SYS_LIBS)

#汇总需要生成的目标文件名
OBJ+=$(OBJ1)
OBJ+=$(OBJ2)
OBJ+=$(OBJ3)
OBJ+=$(OBJ4)
OBJ+=$(OBJ5)
OBJ+=$(OBJ6)

ALL:$(BUILD_DIR) $(TARGET)

INC=-I../GB28181 \
    -I../GB28181/psmux \
    -I../GB28181/SipUA \
    -I../GB28181/Common \
    -I../GB28181/tinyxml \
    -I../GB28181/jsoncpp/include \
    -I../GB28181/osip/include/osip \

LIB=-L../GB28181/osip/lib \
    -L../GB28181/jsoncpp/lib_release/lnx_x64 \


CC=g++ -std=c++11

$(TARGET):$(OBJ)
		$(CC) -o $@ $^ $(LIB) $(LIBS)
$(BUILD_DIR):
	echo "    Create directory $@ ..."  
	mkdir -p $(OBJ_PATH) $(LIB_PATH) 

$(OBJ1):$(OBJ_PATH)/%.o:$(SRC_PATH1)/%.cpp
	$(CC) -c $< -o $@ $(INC) -fPIC 

$(OBJ2):$(OBJ_PATH)/%.o:$(SRC_PATH2)/%.cpp
	$(CC) -c $< -o $@ $(INC) -fPIC

$(OBJ3):$(OBJ_PATH)/%.o:$(SRC_PATH3)/%.cpp
	$(CC) -c $< -o $@ $(INC) -fPIC
	
$(OBJ4):$(OBJ_PATH)/%.o:$(SRC_PATH4)/%.cpp
	$(CC) -c $< -o $@ $(INC) -fPIC

$(OBJ5):$(OBJ_PATH)/%.o:$(SRC_PATH5)/%.cpp
	$(CC) -c $< -o $@ $(INC) -fPIC

$(OBJ6):$(OBJ_PATH)/%.o:$(SRC_PATH6)/%.cpp
	$(CC) -c $< -o $@ $(INC) -fPIC	


clean:
	rm -rf $(LIB_PATH)/$(TARGET_NAME) $(LIB_PATH)/gb28181 $(OBJ)

.PHONY: clean ALL
