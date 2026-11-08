## Mini Server

Instructions to set up the mini server:
- Install all the required dependencies

Building the project:
- Run `make` to build both executables, `bin/pipelined` and `bin/pipectl`.
- Run `make clean` to remove the compiled files.

### Run the server
- Create a work directory first
- Run the server using the command: `./bin/pipelined 8080 4 work`
- Args:
  - The first argument is the port. You can select another port number if the port is already in use. The server will start listening on the specified port.
  - The second argument is the number of agents: the maximum number of jobs that can run at the same time. It must be greater than 0.
  - The third argument is the work directory. It must already exist.

### Run the client
- Run the client using the command: `./bin/pipectl 127.0.0.1 8080 "<COMMAND>"`
- Args:
  - The first argument is the server IP address. You can select another IP address if the server is running on a different machine.
  - The second argument is the server port number where the server is listening.
  - The third argument is the command to send, in quotes.

### Available commands
- `"SUBMIT <file.yaml>"`
  - Example: `./bin/pipectl 127.0.0.1 8080 "SUBMIT yamls/example.yaml"`
- `"STATUS <id>"`
- `"LIST"`
- `"CANCEL <id>"`
- `"LOGS <id> [job] [FOLLOW]"` (TO DO)
- `"SHUTDOWN"`

## Integrantes
- **Miguel Fabricio Arias Vargas** — C5C762
- **Santiago Asunción Marín** — C5C829
- (Agregar nombre y carne de Andres)
