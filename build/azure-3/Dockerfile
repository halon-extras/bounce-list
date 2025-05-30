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

RUN tdnf install -y cmake make build-essential rpm-build pcre2-devel
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

COPY build.sh /build.sh
CMD ["/build.sh"]
