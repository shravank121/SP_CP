FROM ubuntu:latest
RUN apt update && apt install -y build-essential
WORKDIR /app
COPY . /app
RUN gcc CP_1.c -o myshell
CMD ["./myshell"]
