# httpserver-in-c
an HTTP server in C/C++ made for educational purposes.

## Disclaimer
Even though this codebase is public, it is not intended for recreation by others for educational use. This is public to serve as an example of my work, and not intended to be recreated for a grade. If you wish to use parts of my code for your own school project, please cite me as a source to avoid academic dishonesty.

## Contents:
* `Dog`: Dog is a simple program to replace the functionality of cat in the unix bash. its usage is the same as cat, and was created as an exercise to learn about how file descriptors work in the C-unix interaction.
* `single-threaded-http-server`: is a simple http server to learn about socket programming. It supports GET and PUT requests only.
* `multi-threaded-http-server`: is a more complex http server, which has the functionality of the single-threaded version but runs on a user-defined number of threads, and can handle concurrent requests, with a queue system to handle overflows of requests. It also has redundancy built in, and fault detection in the case of inconsistent file stores
* `Multi-threaded-http-server-backup-recovery`: is a feature expansion of the previous server. This one adds backup and recovery functionality through the use of different endpoints. The redundancy functionality of the previous server was removed.
