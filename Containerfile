FROM docker.io/library/debian:13

ARG USER=dbx
EXPOSE 22/tcp

# Flag that we are a devbox
RUN touch /run/.dbxenv

# Remove container-specific APT configurations, and install required packages
RUN rm /etc/apt/apt.conf.d/docker-* && \
    apt-get update && \
    apt-get install -y \
        bash-completion \
        bzip2 \
        curl \
        git \
        gnupg \
        htop \
        less \
        man-db \
        manpages \
        nano \
        openssh-client \
        openssh-server \
        procps \
        sudo \
        tini \
        tree \
        vim \
        wget \
        zip \
        && \
    apt-get clean && \
    rm -dr /var/lib/apt/lists/*

# Passwordless sudo
RUN sed -i -e 's/ ALL$/ NOPASSWD:ALL/' /etc/sudoers

# Configure automatic removal of stale forwarded sockets
RUN echo "StreamLocalBindUnlink yes" >> /etc/ssh/sshd_config

# Create and configure the devbox user
RUN useradd \
        --create-home \
        --groups sudo \
        --shell /usr/bin/bash \
        --uid 1000 \
        --user-group \
        ${USER}

# Configure SSH login
RUN mkdir -m 700 /home/${USER}/.ssh && chown 1000:1000 /home/${USER}/.ssh
COPY --chmod=600 --chown=1000:1000 ssh/id_ed25519.pub /home/${USER}/.ssh/authorized_keys

# User customizations
COPY --chown=1000:1000 custom/ /home/${USER}/
RUN rm /home/${USER}/README.md
RUN if [ -f /home/${USER}/extra-packages ]; then \
        apt-get update && \
        apt-get install -y $(cat /home/${USER}/extra-packages | xargs) && \
        apt-get clean && \
        rm -dr /var/lib/apt/lists/* /home/${USER}/extra-packages; \
    fi
RUN if [ -x /home/${USER}/post-install ]; then \
        su - ${USER} /home/${USER}/post-install && \
        rm /home/${USER}/post-install; \
    fi

# Set up the entrypoint
COPY dbx-init /usr/local/bin/dbx-init
ENTRYPOINT ["dbx-init"]
