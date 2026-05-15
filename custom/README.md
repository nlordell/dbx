# Customizations

This directory includes customizations that are not tracked by Git. All files in this directory will be copied over the devbox user's home directory (overwriting any files that may exist there from `/etc/skel`), allowing for easy customization of the devbox user.

## Extra Packages

A special `extra-packages` file can be added to this directory to include additional packages to install when building the container.

## Post Installation Script

A special `post-install` executable can be added to this directory which will be run as a final step when building the container. This allows you to include a one-time setup with the development container image. The post install script is run as the devbox user (with `sudo` access).
