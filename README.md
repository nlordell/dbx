# Containerized Development Environments

`dbx` is a set of scripts for managing a local containerized development environment. It works by creating a long-lived container machine named `dbx` that that has no access to the host where you can install various developer tools and dependencies without losing sleep at night. It uses Apple `container` tool, which creates a dedicated virtual machine for the `dbx` container machine, providing increased isolation and security. The host connects to the container via SSH, which allows any development tools with remote editing capabilities to be used (such as Emacs, Zed or Visual Studio Code).

At its core, it is similar to Toolbx and Distrobox, but with improved isolation and security. In general, it does not try to create a convenient container that is seamlessly integrated with the host, but rather a restricted sandbox where you can install development CLI tools, and reduce `npm install` risk.
