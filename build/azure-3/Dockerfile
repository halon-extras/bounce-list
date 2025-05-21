FROM --platform=linux/amd64 mcr.microsoft.com/azurelinux/base/core:3.0
LABEL org.opencontainers.image.authors="support@halon.io"

ARG HALON_REPO_USER
ARG HALON_REPO_PASS

RUN tdnf install -y ca-certificates azurelinux-repos-extended

RUN echo "[halon]" >> /etc/yum.repos.d/Halon.repo
RUN echo "name=Halon repository" >> /etc/yum.repos.d/Halon.repo
RUN echo "baseurl=https://${HALON_REPO_USER}:${HALON_REPO_PASS}@repo.halon.io/dists/azure3/amd64/stable" >> /etc/yum.repos.d/Halon.repo
RUN echo "gpgcheck=1" >> /etc/yum.repos.d/Halon.repo
RUN echo "gpgkey=https://raw.githubusercontent.com/halon/changelog/master/pgp-keys/7F0A73B5.asc" >> /etc/yum.repos.d/Halon.repo
RUN echo "enabled=1" >> /etc/yum.repos.d/Halon.repo
RUN tdnf install -y halon-6.7.0

RUN tdnf install -y cmake make build-essential rpm-build
RUN echo -n "AZURE3" > /OSRELEASE.txt

RUN mkdir /tmp/halon

RUN cd /tmp/halon \
    && curl -O -L https://sourceforge.net/projects/libcsv/files/libcsv/libcsv-3.0.3/libcsv-3.0.3.tar.gz \
    # https://github.com/freebsd/freebsd-ports/commit/adf0fa2deb7b3d3d00ed40e92aebe9f2715b567d
    && echo "d9c0431cb803ceb9896ce74f683e6e5a0954e96ae1d9e4028d6e0f967bebd7e4 libcsv-3.0.3.tar.gz" | sha256sum -c \
    && tar zxf libcsv-3.0.3.tar.gz \
    && cd libcsv-3.0.3 \
    && ./configure --prefix /external --enable-shared --disable-static \
    && make && make install

RUN cd /tmp/halon \
    && curl -O -L https://sourceforge.net/projects/pcre/files/pcre/8.45/pcre-8.45.tar.bz2 \
    # https://github.com/freebsd/freebsd-ports/commit/6613bc9e03f175306a03f17dc6e520a7c69c8108
    # https://github.com/microsoft/azurelinux/blob/mariner-2/SPECS/pcre/pcre.signatures.json
    && echo "4dae6fdcd2bb0bb6c37b5f97c33c2be954da743985369cddac3546e3218bffb8 pcre-8.45.tar.bz2" | sha256sum -c \
    && bunzip2 pcre-8.45.tar.bz2 && tar xf pcre-8.45.tar \
    && cd pcre-8.45 \
    # https://github.com/microsoft/azurelinux/blob/mariner-2/SPECS/pcre/pcre.spec
    && ./configure \
        --prefix /external \
        --enable-jit \
        --enable-unicode-properties \
        --enable-pcre16 \
        --enable-pcre32 \
        --with-match-limit-recursion=16000 \
    && make install

COPY build.sh /build.sh
CMD ["/build.sh"]
