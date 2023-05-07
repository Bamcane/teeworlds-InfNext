# build
FROM alpine:latest AS build_env

RUN apk update && apk upgrade 
RUN apk add --no-cache openssl gcc g++ make cmake python3 zlib-dev icu-dev curl-dev

COPY . sources
WORKDIR /souces
RUN cmake /sources -DCMAKE_INSTALL_PREFIX=/install
RUN cmake --build . -t install

# runtime
FROM alpine:latest AS runtime_env
WORKDIR /InfNext-Server/
RUN apk update && apk upgrade
RUN apk add --no-cache libstdc++ icu libmaxminddb
COPY --from=build_env /install .

EXPOSE 8303/udp
ENTRYPOINT ["./InfNext-Server"]
