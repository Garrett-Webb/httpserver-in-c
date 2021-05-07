# Multi-threaded HTTP Server with backup and recovery


## Setup the Environment
* Install Ubuntu 18.04 LTS or any other linux distribution.
* Install gcc/g++ with `sudo apt install build-essential`
* Specifically, the G++ version should be: `g++ (Ubuntu 7.5.0-3ubuntu1~18.04) 7.5.0`
* and the curl version should be `7.58`
* compile using `make`
* remove build files using `make clean`
* Begin by opening two separate terminal windows.
* In the first, we will run the server. in the second, we will use `Curl` as a client.

## Running the Server
* Navigate to the directory that stores the executable compiled with `make`.
* `./httpserver serveraddress portnumber` will start the server with the specified address on the specified port. The server will also be multithreaded with N=4 default threads and will not use redundancy.
* `./httpserver serveraddress` will start the server with the specfied address on default port `80` Port `80` however is reserved. For the default port to work, `sudo` must be appended to the beginning of this command.
* EXAMPLE: `./httpserver localhost 8080 -N 6 -r`
* press `ctrl+C` to shut down the server.

## Connecting to the Server with a Client
* For the purposes of this assignment, we used `curl` as the client with which to connect to our server.
* `Curl` can do many things, but the server can only respond to `GET` and `PUT` requests.
* GET: `curl http://localhost:PORT/file -v`
* PUT: `curl -T sourcefile http://localhost:PORT/destinationfile -v`
* There are some extra functionalities built into the `GET` request.
* BACKUP: `curl http://localhost:PORT/b` will create a backup of the server at its current state.
* LIST: `curl http://localhost:PORT/l` will list all of the timestamps of available backups to restore.
* RECOVERY: `curl http://localhost:PORT/r` will restore the server to its most recent backup.
* RECOVERY: `curl http://localhost:PORT/r/timestamp` will restore the server to a specific backup specified by replacing `timestamp` with a 10 digit number gotten from the list request.


## Constraints
* Only supports `PUT` and `GET` requests.
* Filename must be 10 ASCII alphanumeric characters. no `.`, `/`, `_`, `-`, etc...
* Only status codes supported are `200 OK`, `201 Created`, `400 Bad Request`, `403 Forbidden`, `404 Not Found`, `500 Internal Server Error`. Other HTTP status codes are not implemented.
