FROM debian:12-slim AS build

RUN apt-get update && \
    apt-get upgrade -y && \
    apt-get install -y git make gcc g++ cmake pkg-config librtlsdr-dev libsoxr-dev libcurl4-openssl-dev zlib1g-dev libpq-dev

COPY . /root/AIS-catcher

RUN cd /root/AIS-catcher && \
    mkdir build && \
    cd build && \
    cmake .. && \
    make && \
    make install

FROM debian:12-slim

RUN apt-get update && \
    apt-get upgrade -y && \
    apt-get install -y librtlsdr0 libsoxr0 libcurl4 libpq-dev

COPY --from=build /usr/local/bin/AIS-catcher /usr/local/bin/AIS-catcher

ENTRYPOINT ["/usr/local/bin/AIS-catcher"]
