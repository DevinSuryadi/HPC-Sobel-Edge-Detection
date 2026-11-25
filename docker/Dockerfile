# Lingkungan build & eksekusi untuk seluruh versi (V0, V1, V2).
# Satu image dipakai oleh semua container (node) di cluster teremulasi.
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        openmpi-bin libopenmpi-dev \
        openssh-server openssh-client \
        ffmpeg \
        python3 python3-matplotlib \
    && rm -rf /var/lib/apt/lists/*

# User non-root dengan UID 1000 agar cocok dengan pemilik file di WSL (user devin).
RUN userdel -r ubuntu 2>/dev/null || true \
 && useradd -m -u 1000 -s /bin/bash mpi

# SSH tanpa password antar-node: mpirun memakai SSH untuk menjalankan proses di node lain.
# Semua container memakai image yang sama sehingga kuncinya sama dan saling dipercaya.
RUN mkdir -p /run/sshd \
 && su mpi -c "mkdir -p ~/.ssh \
      && ssh-keygen -q -t ed25519 -N '' -f ~/.ssh/id_ed25519 \
      && cp ~/.ssh/id_ed25519.pub ~/.ssh/authorized_keys \
      && printf 'Host *\n  StrictHostKeyChecking no\n  UserKnownHostsFile /dev/null\n  LogLevel ERROR\n' > ~/.ssh/config \
      && chmod 700 ~/.ssh && chmod 600 ~/.ssh/*"

# Open MPI: komunikasi antar-node lewat TCP pada interface jaringan container (eth0).
RUN printf 'btl = self,vader,tcp\npml = ob1\nbtl_tcp_if_include = eth0\noob_tcp_if_include = eth0\n' \
      >> /etc/openmpi/openmpi-mca-params.conf

WORKDIR /work
CMD ["/usr/sbin/sshd", "-D"]
