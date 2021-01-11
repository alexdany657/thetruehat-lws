# TheTrueHat-lws

This is TheTrueHat server implementation in C. Now only one room is supported.

## Build
1. Install [libwebsockets](https://libwebsockets.org/) (tested with v4.1.6.)
1. Create directory `lib`
1. Add `~/lib` directory (or another path, but rewrite it in Makefile) to `LD_LIBRARY_PATH` environment variable
1. Create `config.h` with following variables:
```c
const char *EXEC_PATH = "<path to tth executable>";
const char *USERNAME = "<username>";
```
Path should be absolute. `master_tth` will call `ssh` with that username (rsa key or other auth method recommended).
1. Run `make`

## Run
1. Run `./master_tth 5000` and go to http://localhost:5000 (port 5000 can be changed)

## License
MIT
