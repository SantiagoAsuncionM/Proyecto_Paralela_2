# Concurrent Pipeline Server

Project 2 for CI-0117 Parallel and Concurrent Programming (UCR, II-2026).

## Requirements

- Linux, GCC and `make`.
- libyaml: `make install` (runs `sudo apt install -y libyaml-dev`).

## Build

| Command | Description |
|---|---|
| `make` | Builds `bin/pipelined` and `bin/pipectl`. |
| `make test` | Builds and runs every test in `tests/` with AddressSanitizer. Fails if any test fails. |
| `make DSIN_PROTECCION` | Rebuilds the server with `-DSIN_PROTECCION` |
| `make asan` | Rebuilds both programs with AddressSanitizer. |
| `make clean` | Removes the binaries and the compiled tests. |

## Usage

### Server

```bash
mkdir -p work
./bin/pipelined 8080 4 work
```

1. **Port** where the server listens.
2. **Number of agents:** the maximum number of jobs running at the same time, across all pipelines. Must be greater than 0.
3. **Work directory**, which must already exist. Each run is stored in `run_<id>/`, with the job folders (`jobs/`), artifacts (`artifacts/`) and logs (`logs/`).

### Client

```bash
./bin/pipectl 127.0.0.1 8080 "SUBMIT /absolute/path/pipeline.yaml"
```

1. **Server IP address.**
2. **Server port.**
3. **Command**, in quotes.

The client sends the path of the YAML file, not its content, so client and server must run on the same machine. Relative paths are resolved from the server's directory, so absolute paths are recommended.

### Commands

| Command | Response |
|---|---|
| `SUBMIT <file.yaml>` | `OK <id>`, or `ERROR <n>` followed by each error with its line. |
| `STATUS <id>` | State and duration of the run, each stage, each job and each step. |
| `LOGS <id> [job] [FOLLOW]` | Log of the run or of one job. With `FOLLOW`, keeps printing new lines until the job ends. |
| `CANCEL <id>` | Cancels the run: running steps stop, pending jobs do not run, and the run ends `canceled`. |
| `LIST` | One line per run, with its id and state. |
| `SHUTDOWN` | Orderly shutdown: no new requests, agents finish their current job and resources are freed. |

## Tests

```bash
make test
```

| Test | Description |
|---|---|
| `test_executor` | Full pipelines through the executor: scripts, artifacts, variables and conditions; cancel; timeout; and 50 jobs feeding 1, where the last job must run exactly once. |
| `test_parser` | Valid and invalid YAML: format, repeated names and cycles, with their line. |
| `test_tasks` | The four tasks: same result with 1 and 4 threads, invalid inputs, cancel and timeout. |
| `test_ready_queue` | FIFO order, closing, and 10,000 concurrent operations with no lost or repeated jobs. |
| `test_scheduler` | Agent start and shutdown. |

### Race condition

`-DSIN_PROTECCION` removes the mutex that protects the run id counter. `tests/sin_proteccion.sh` sends 200 simultaneous `SUBMIT`s to a server on port 8080 and counts repeated ids:

```bash
make DSIN_PROTECCION
./bin/pipelined 8080 4 work &
bash tests/sin_proteccion.sh       # reports repeated ids
```

With the normal build (`make`), the same script finds none.

## Repository

```
include/   headers
src/       server, client and their modules
tests/     automated tests
yamls/     example, test and measurement pipelines
scripts/   data generation for the metrics
```

## Team

- **Miguel Fabricio Arias Vargas** — C5C762
- **Santiago Asunción Marín** — C5C829
- **Andrés Oreamuno Varela** — A43842
