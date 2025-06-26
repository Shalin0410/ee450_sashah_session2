README
a. Full Name
Shalin Shah

b. Student ID
3837311928

c. What I Have Done in the Assignment

Implemented the required socket programming project using TCP and UDP communication between clients and servers as outlined in the assignment specifications.
Completed the optional part: Log Functionality for retrieving and displaying user-specific logs of operations performed.

d. Code Files and Their Purpose

client.c:
Implements the client-side application. This includes:
Sending authentication requests to the main server (serverM).
Sending commands (lookup, push, remove, deploy, log) to the main server.
Receiving responses from serverM and displaying the results.

serverM.c:
Implements the main server:
Handles client connections via TCP.
Routes commands to appropriate backend servers (serverA, serverR, serverD) via UDP.
Logs operations and forwards responses back to clients.

serverA.c:
Implements authentication services:
Validates user credentials (username and encrypted password) against a members.txt file.

serverR.c
Handles repository-related operations:
Processes lookup, push, remove, and deploy commands.
Maintains the file repository using filenames.txt.

serverD.c:
Manages deployment functionality:
Handles deployment of files from serverR.
Stores deployment logs in deployed.txt.

Makefile:
Facilitates compilation of all code files and cleans up generated binaries.

e. Format of All Messages Exchanged

Between Client and ServerM (TCP)
- Authentication: auth <username> <encrypted_password>
	Response: auth_success or auth_failed

Commands:

- Lookup: <username> lookup <target_username>
	Response: <List of filenames> or "user not found".
- Push: <username> push <filename>
	Response: "confirm_overwrite" or "file pushed".
- Remove: <username> remove <filename>
	Response: "file removed" or "file not found".
- Deploy: <username> deploy
	Response: <List of deployed files> or "no files found".
- Log: <username> log
	Response: <List of operations performed> or "No operations have been logged".

Between ServerM and Backend Servers (UDP):

- Authentication (to serverA): auth <username> <encrypted_password>
	Response: "auth_success" or "auth_failed".

Repository Commands (to serverR):

- Lookup: <username> lookup <target_username>
	Response: <List of filenames> or "user not found".
- Push: <username> push <filename>
	Response: "duplicate file" or "file pushed".
- Remove: <username> remove <filename>
	Response: file removed or file not found.
- Deploy: <username> deploy <username>
	Response: <List of filenames> or "user not found".

Deployment Commands (to serverD):
- Deploy: <username> <List of filenames>
	Response: "files deployed".

g. Idiosyncrasies of the Project

Failure Conditions:

If the members.txt, filenames.txt, or deployed.txt files are missing or corrupted, related functionalities will fail.
The program does not handle concurrent modifications to repository files well (e.g., simultaneous push operations).
The project assumes all filenames are unique for each user.

Assumptions:
The backend servers (serverA, serverR, serverD) are running and reachable.
The client must use correct commands; malformed inputs may lead to undefined behavior.
For "push" commands, the file is to be present in the directory if not it will not push the file to filenames.txt

h. Reused Code

To setup the TCP/UDP connection, code from the textbook "Beej’s Guide to Network Programming" was taken. All the remaining implementation and logic are original and developed based on the project specifications.
All reused codes, the specific functions are commented within the source code with appropriate attributions.