FROM docker.io/library/debian:13

# Flag that we are a devbox
RUN touch /run/.dbxenv

# Remove container-specific APT configurations, and install required packages
RUN rm /etc/apt/apt.conf.d/docker-* && \
    apt-get update && \
    apt-get install -y \
        openssh-server \
        sudo \
        tini \
        && \
    apt-get clean && \
    rm -dr /var/lib/apt/lists/*

# Configure SSH server
COPY ssh/id_ed25519.pub /etc/ssh/authorized_keys
RUN echo "StreamLocalBindUnlink yes" >> /etc/ssh/sshd_config && \
    echo "AuthorizedKeysFile /etc/ssh/authorized_keys" >> /etc/ssh/sshd_config

# Set up the entrypoint
COPY dbx-init /sbin/init
ENTRYPOINT ["/sbin/init"]
