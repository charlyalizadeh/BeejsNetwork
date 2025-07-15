# Network project inspired by Beej's guide to Network Programming

## 1. Echo Server

### Usage

```bash
> gcc main.cpp -o echo_server
> echo_server 2902
[MAIN] Listening from port: 2902
```

You can test it in localhost by connecting to the specified port:
```bash
> nc -C localhost 2902
TEST
```

You should get the following output (use CTRL-D to send data without a newline):
```bash
[MAIN] Listening from port: 2902
[MAIN] Revieved connection from 127.0.0.1
[127.0.0.1]: Recieved: TEST
```
