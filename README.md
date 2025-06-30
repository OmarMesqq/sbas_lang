# The SBas Language

SBas is a tremendously simple language targeting the x86-64 architecture.
You need a Linux machine, `gcc`, `make`, and optionally, `valgrind` to detect memory errors and leaks.

## Compile the debug variant compiler:
```
make debug
```

## Compile the release variant compiler:
```
make release
```

## Run tests:
```
make test
/tmp/sbas_test
```

## Run memory leak tests:
```
make memleak-check
```

The compiler will be saved on `/tmp/sbas` by default.
