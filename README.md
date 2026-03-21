# Containerized Development Environments

`dbx` is a set of scripts for managing a local containerized development environment. It works by creating a long-lived container named `dbx` with only access to a development root directory, where you can install various developer tools and dependencies without losing sleep at night.

At its heart, it is extremely similar to Toolbx and Distrobox, but with as reduced permissions as possible for improved security. For example, unlike the aforementioned tools, `dbx` keeps SELinux enforcement; meaning that the `dbx` user cannot access other files if it manages to escape the namespace. In general, it does not try to create a convenient container that is seamlessly integrated with the host, but rather a restricted sandbox where you can install development tools.
