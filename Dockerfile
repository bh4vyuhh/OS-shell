FROM ubuntu:22.04

RUN apt-get update && \
    apt-get install -y build-essential && \
    apt-get install -y libreadline-dev && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY cell.c .

RUN gcc cell.c -o shell -lreadline

CMD ["./shell"]


