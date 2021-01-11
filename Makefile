COMMON_FLAGS_D=-Wall -Wextra -g -Iinclude
LD_FLAGS_D=-Llib

COMMON_FLAGS=-Wall -Wextra -DNDEBUG

release: tth master_tth

all: release debug

debug: tth.d master_tth.d

master_tth.d: master_tth.d.o
	gcc -o master_tth.d master_tth.d.o $(LD_FLAGS_D) -lwebsockets

master_tth: master_tth.o
	gcc -o master_tth master_tth.o -lwebsockets

master_tth.d.o: master_tth.c protocol_master_tth.c tth_misc.h config.h
	gcc $(COMMON_FLAGS_D) -c -o master_tth.d.o master_tth.c

master_tth.o: master_tth.c protocol_master_tth.c tth_misc.h config.h
	gcc $(COMMON_FLAGS) -c -o master_tth.o master_tth.c

tth.d: tth.d.o tth_codes.d.o tth_callbacks.d.o tth_signals.d.o tth_json.d.o lib/libcJSON.so tth_timeout.d.o
	gcc -o tth.d tth.d.o tth_codes.d.o tth_callbacks.d.o tth_signals.d.o tth_json.d.o tth_timeout.d.o $(LD_FLAGS_D) -lwebsockets -lcJSON

tth: tth.o tth_codes.o tth_callbacks.o tth_signals.o tth_json.o lib/libcJSON.so tth_timeout.o
	gcc -o tth tth.o tth_codes.o tth_callbacks.o tth_signals.o tth_json.o tth_timeout.o -lwebsockets -Llib -lcJSON

tth_timeout.d.o: tth_timeout.h tth_timeout.c
	gcc $(COMMON_FLAGS_D) -c -o tth_timeout.d.o tth_timeout.c

tth_timeout.o: tth_timeout.h tth_timeout.c
	gcc $(COMMON_FLAGS) -c -o tth_timeout.o tth_timeout.c

tth_json.d.o: tth_structs.h tth_misc.h tth_json.h tth_json.c
	gcc $(COMMON_FLAGS_D) -c -o tth_json.d.o tth_json.c

tth_json.o: tth_structs.h tth_misc.h tth_json.h tth_json.c
	gcc $(COMMON_FLAGS) -c -o tth_json.o tth_json.c

tth_signals.d.o: tth_structs.h tth_codes.h tth_signals.h tth_signals.c tth_misc.h tth_json.h
	gcc $(COMMON_FLAGS_D) -c -o tth_signals.d.o tth_signals.c

tth_signals.o: tth_structs.h tth_codes.h tth_signals.h tth_signals.c tth_misc.h tth_json.h
	gcc $(COMMON_FLAGS) -c -o tth_signals.o tth_signals.c

tth_codes.d.o: tth_codes.h tth_codes.c tth_structs.h
	gcc $(COMMON_FLAGS_D) -c -o tth_codes.d.o tth_codes.c

tth_codes.o: tth_codes.h tth_codes.c tth_structs.h
	gcc $(COMMON_FLAGS) -c -o tth_codes.o tth_codes.c

tth_callbacks.d.o: tth_callbacks.h tth_callbacks.c tth_structs.h tth_misc.h
	gcc $(COMMON_FLAGS_D) -c -o tth_callbacks.d.o tth_callbacks.c

tth_callbacks.o: tth_callbacks.h tth_callbacks.c tth_structs.h tth_misc.h
	gcc $(COMMON_FLAGS) -c -o tth_callbacks.o tth_callbacks.c

tth.d.o: tth.c protocol_tth.c tth_callbacks.h tth_codes.h tth_structs.h tth_signals.h tth_timeout.h
	gcc $(COMMON_FLAGS_D) -c -o tth.d.o tth.c

tth.o: tth.c protocol_tth.c tth_callbacks.h tth_codes.h tth_structs.h tth_signals.h tth_timeout.h
	gcc $(COMMON_FLAGS) -c -o tth.o tth.c

lib/libcJSON.so: cJSON/cJSON.o
	gcc -shared -o lib/libcJSON.so cJSON/cJSON.o
	cp -v lib/libcJSON.so ~/lib

cJSON/cJSON.o: cJSON/cJSON.c
	gcc -c -shared -fpic -fPIC -o cJSON/cJSON.o cJSON/cJSON.c

.PHONY: all cleanAll clean-o clean debug release

clean-o:
	rm -v *.o

clean:
	rm -fv tth
	rm -fv master_tth

cleanAll: clean clean-o
