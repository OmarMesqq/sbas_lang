# The SBas Language

SBas is a tremendously simple language targeting the x86-64 architecture.
You need a Linux machine, `gcc`, `make`, and optionally, `valgrind` to detect memory errors and leaks.


## The compiler:
### building the debug variant:
```
make debug
```

### building the release variant:
```
make release
```

The compiler will be saved on `/tmp/sbas` by default.

### usage:
```
./sbas foo.sbas <arg1> <arg2> <arg3>
```
where the arguments are between 0 and 3.

The calculation result will be printed to `stdout`

## Run tests:
```
make test
/tmp/sbas_test
```

## Run memory leak tests:
```
make memleak-check
```


