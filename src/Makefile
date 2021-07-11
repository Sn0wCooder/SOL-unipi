CC = gcc
CFLAGS = -g -lpthread
CONFIGFILE1 = ./configs/config1.txt
CONFIGFILE1 = ./configs/config2.txt
CLIENTOUTPUT_DIR1 = ./outputTest1
CLIENTOUTPUT_DIR2 = ./outputTest2
FLAG_INCLUDE_DIR = -I ./includes
SOCKNAME = ./sock.sk
TARGETS = server client server.dSYM client.dSYM
VALGRIND_OPTIONS = valgrind --leak-check=full
#QUEUE_DATA_TYPES_DEP = ./Queue_Data_Types/
#CONFIG_UTILITY_DEP = ./ConfigAndUtilities/

#COMMON_DEPS = $(QUEUE_DATA_TYPES_DEP)generic_queue.c $(CONFIG_UTILITY_DEP)utility.c \
 	$(QUEUE_DATA_TYPES_DEP)request.c $(QUEUE_DATA_TYPES_DEP)file.c

SERVER_DEPS = server.c util.c queue.c

#fileStorageServer.c $(QUEUE_DATA_TYPES_DEP)descriptor.c \
 $(CONFIG_UTILITY_DEP)serverInfo.c $(COMMON_DEPS)

CLIENT_DEPS = client.c util.c queue.c parser.c

#fileStorageClient.c $(CONFIG_UTILITY_DEP)clientConfig.c \
 serverAPI.c $(COMMON_DEPS)

CLIENTCONFIGOPTIONS = -f $(SOCKNAME) -p -t 200

#OBJ_FOLDER = ./build/objs
#LIB_FOLDER = ./libs


#configurazioni di un client per testare singolarmente
# le funzioni dell'API


.PHONY : all, cleanall, test1, test2

all:
	make -B server
	make -B client

cleanall:
	rm -rf $(SOCKNAME) $(TARGETS) $(CLIENTOUTPUT_DIR1) $(CLIENTOUTPUT_DIR2)
	#rm -f $(OBJ_FOLDER)/*.o
	#rm -f $(LIB_FOLDER)/*.so
	#rm -f ./SaveReadFileDirectory/AllCacheFilesTest2/*
	#rm -f ./SaveReadFileDirectory/AllCacheFilesTest1/*


#rm $(CLIENTOUTPUT_DIR1)/*
#rmdir $(CLIENTOUTPUT_DIR1)
#rm $(CLIENTOUTPUT_DIR2)/*
#rmdir $(CLIENTOUTPUT_DIR2)

#targets per generare gli eseguibili
server: $(SERVER_DEPS)
	#make cleanall
	$(CC) $(CFLAGS) $(SERVER_DEPS) -o server $(FLAG_INCLUDE_DIR)
				   #-Wl,-rpath,./libs -L ./libs -lcommon -lserv -lpthread

client: $(CLIENT_DEPS)
	#make cleanall
	$(CC) $(CFLAGS) $(CLIENT_DEPS) -o client $(FLAG_INCLUDE_DIR)
				 # -Wl,-rpath,./libs -L ./libs -lcommon -lclient -lapi

test1:
	chmod +x ./testscripts/serverTest1.sh
	./testscripts/serverTest1.sh $(CLIENTOUTPUT_DIR1)

test2:
	chmod +x ./testscripts/serverTest2.sh
	./testscripts/serverTest2.sh $(CLIENTOUTPUT_DIR2)
