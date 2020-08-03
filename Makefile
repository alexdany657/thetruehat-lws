tth: tth.o tth_codes.o tth_callbacks.o tth_signals.o tth_json.o lib/libcJSON.so
	gcc -o tth tth.o tth_codes.o tth_callbacks.o tth_signals.o tth_json.o -Llib -lwebsockets -lcJSON

tth_json.o: tth_structs.h tth_misc.h tth_json.h tth_json.c
	gcc -g -Wall -c -o tth_json.o tth_json.c

tth_signals.o: tth_structs.h tth_codes.h tth_signals.h tth_signals.c tth_misc.h tth_json.h
	gcc -g -Wall -c -o tth_signals.o tth_signals.c

tth_codes.o: tth_codes.h tth_codes.c tth_structs.h
	gcc -g -Wall -c -o tth_codes.o tth_codes.c

tth_callbacks.o: tth_callbacks.h tth_callbacks.c tth_structs.h tth_misc.h
	gcc -g -Wall -c -o tth_callbacks.o tth_callbacks.c

tth.o: tth.c protocol_tth.c tth_callbacks.h tth_codes.h tth_structs.h tth_signals.h
	gcc -g -Wall -c -o tth.o tth.c

lib/libcJSON.so: cJSON/cJSON.o
	gcc -shared -o lib/libcJSON.so cJSON/cJSON.o
	cp -v lib/libcJSON.so ~/lib

cJSON/cJSON.o: cJSON/cJSON.c
	gcc -c -shared -fpic -fPIC -o cJSON/cJSON.o cJSON/cJSON.c

.PHONY: cleanAll clean-o clean

clean-o:
	rm -v *.o

claen:
	rm -v tth

cleanAll: clean clean-o
