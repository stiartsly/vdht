# this dockerfile uses the ubuntu image
# auth tzl

FROM ubuntu

MAINTAINER tang.zhilong tang.zhilong@kortide.com

RUN apt-get update && apt-get install -y build-essential
RUN apt-get install -y libsqlite3-dev
RUN apt-get install -y wget
RUN apt-get install -y git

WORKDIR /root
RUN git clone https://github.com/miniupnp/miniupnp.git
WORKDIR /root/miniupnp/miniupnpc
RUN make && make install

WORKDIR /root
RUN git clone https://github.com/stiartsly/vdht.git
WORKDIR /root/vdht
RUN make

RUN mv /root/vdht/docker/vrun.sh /run.sh
RUN chmod 755 /run.sh

CMD ["/run.sh"]
