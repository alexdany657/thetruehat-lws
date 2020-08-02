# TheTrueHat-lws

This is TheTrueHat server implementation in C. Now only one room is supported, and only logging in and logging out. WIP.

## Build
1. Install [libwebsockets](https://libwebsockets.org/)
1. Create directory `lib`
1. Add `~/lib` directory (or another path, but rewrite it in Makefile)
1. Add that path to `LD_LIBRARY_PATH` environment variable
1. Run `make`

## Run
1. Run `./tth` and go to http://localhost:5000.

## License
MIT
