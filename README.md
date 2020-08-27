# TheTrueHat-lws

This is TheTrueHat server implementation in C. Now only one room is supported.

## Build
1. Install [libwebsockets](https://libwebsockets.org/)
1. Create directory `lib`
1. Add `~/lib` directory (or another path, but rewrite it in Makefile) to `LD_LIBRARY_PATH` environment variable
1. Run `make`

## Run
1. Run `./tth 5000` and go to http://localhost:5000 (port 5000 can be changed)

## License
MIT
