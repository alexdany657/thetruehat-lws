COMMON_FLAGS=-g -Wall

tth: tth.o tth_codes.o tth_callbacks.o tth_signals.o tth_json.o lib/libcJSON.so tth_timeout.o
	gcc -o tth tth.o tth_codes.o tth_callbacks.o tth_signals.o tth_json.o tth_timeout.o -Llib -lwebsockets -lcJSON

tth_timeout.o: tth_misc.h tth_timeout.h tth_timeout.c
	gcc $(COMMON_FLAGS) -c -o tth_timeout.o tth_timeout.c

tth_json.o: tth_structs.h tth_misc.h tth_json.h tth_json.c
	gcc $(COMMON_FLAGS) -c -o tth_json.o tth_json.c

tth_signals.o: tth_structs.h tth_codes.h tth_signals.h tth_signals.c tth_misc.h tth_json.h
	gcc $(COMMON_FLAGS) -c -o tth_signals.o tth_signals.c

tth_codes.o: tth_codes.h tth_codes.c tth_structs.h
	gcc $(COMMON_FLAGS) -c -o tth_codes.o tth_codes.c

tth_callbacks.o: tth_callbacks.h tth_callbacks.c tth_structs.h tth_misc.h
	gcc $(COMMON_FLAGS) -c -o tth_callbacks.o tth_callbacks.c

tth.o: tth.c protocol_tth.c tth_callbacks.h tth_codes.h tth_structs.h tth_signals.h tth_timeout.h
	gcc $(COMMON_FLAGS) -c -o tth.o tth.c

lib/libcJSON.so: cJSON/cJSON.o
	gcc -shared -o lib/libcJSON.so cJSON/cJSON.o
	cp -v lib/libcJSON.so ~/lib

cJSON/cJSON.o: cJSON/cJSON.c
	gcc -c -shared -fpic -fPIC -o cJSON/cJSON.o cJSON/cJSON.c

.PHONY: cleanAll clean-o clean

clean-o:
	rm -v *.o

clean:
	rm -v tth

cleanAll: clean clean-o
