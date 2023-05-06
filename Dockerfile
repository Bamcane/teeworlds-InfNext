FROM ubuntu:jammy

RUN apt-get upgrade && apt-get update && apt-get install -y git \
        build-essential \
        python3 \
        libcurl4-openssl-dev \
        zlib1g-dev \
        libpnglite-dev \
        openssl \
        libssl-dev \
        libicu-dev \
        cmake

COPY . sources
WORKDIR /sources

RUN printf '#!/bin/bash\n \
            cmake . && make' \
          >> build-all.sh

RUN chmod +x ./build-all.sh && bash ./build-all.sh


EXPOSE 8303/udp

ENTRYPOINT ["./InfNext-Server"]
