# Farm

## Installation

```bash
wget https://raw.githubusercontent.com/simonepassera/Farm/main/farm.tar.gz
```

```bash
tar xzf farm.tar.gz
```

```bash
cd farm
```

```bash
make
```

## Usage

```
Usage: farm [OPTION]... [FILE]...
Read binary FILEs and return for each of them a calculation based on the numbers present in the FILEs.
The printing of the results is sorted in ascending order.

  -h                display this help and exit
  -n threads        number of Worker threads (default value 4)
  -q size           length of the concurrent queue between the Master thread and the Worker threads (default value 8)
  -d dirname        calculate the result for each binary FILE in the dirname directory and subdirectories
  -t delay          time in milliseconds between the sending of two successive requests to
                    the Worker threads by the Master thread (default value 0)

  Signals:

  Signal                                   Action
  ──────────────────────────────────────────────────────────────────────────────────────────────────
  SIGUSR1                                  print the results calculated up to that moment
  SIGHUP - SIGINT - SIGQUIT - SIGTERM      complete any tasks in the queue and terminate the process
```
