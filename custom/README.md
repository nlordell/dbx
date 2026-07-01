# Customizations

This directory includes customizations that are not tracked by Git that can be used to configure the devbox on its first run.

## Skeleton Directory

All files in the `skel` directory will be copied over the devbox user's home directory (overwriting any files that may exist there from `/etc/skel`), allowing for easy customization of the devbox user.

## Extra Packages

A special `extra-packages` file can be added to this directory to include additional packages to install on first run.

## Post Installation Script

A special `post-install` executable can be added to this directory which will be run as a final step when creating the devbox. The post install script is run as the devbox user (with passwordless `sudo` access).
