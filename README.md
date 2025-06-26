# Git450: Distributed File Management System  
**EE450 Socket Programming Project**  
**Author:** Shalin Shah  

---

## ✅ Project Summary
This project implements a simplified GitHub-like file management system using TCP and UDP sockets across a distributed client-server architecture.

**Key Features:**
- Encrypted member authentication
- Role-based access: Guest vs Member
- File operations: `lookup`, `push`, `remove`, `deploy`
- Bonus: `log` command for viewing operation history

---

## 📁 Code Structure

### `client.c`
- Implements the client-side interface
- Handles authentication and user commands
- Communicates with `serverM` over TCP

### `serverM.c` (Main Server)
- Receives client requests over TCP
- Forwards them to backend servers over UDP
- Maintains operation logs

### `serverA.c` (Authentication Server)
- Validates user credentials using `members.txt`
- Supports a cyclic +3 encryption scheme for passwords

### `serverR.c` (Repository Server)
- Processes `lookup`, `push`, `remove`, `deploy` operations
- Manages `filenames.txt` for file tracking

### `serverD.c` (Deployment Server)
- Handles deployment requests
- Writes deployed files to `deployed.txt`

### `Makefile`
- Builds all binaries with `make all`
- Supports clean-up with `make clean`

---

## 💬 Communication Protocols

### Between `Client` and `serverM` (TCP)

| Command        | Format                                 | Response                                 |
|----------------|----------------------------------------|------------------------------------------|
| **Auth**       | `auth <username> <password>`           | `auth_success`, `auth_failed`            |
| **Lookup**     | `<username> lookup <target_user>`      | File list or `user not found`            |
| **Push**       | `<username> push <filename>`           | `confirm_overwrite`, `file pushed`       |
| **Remove**     | `<username> remove <filename>`         | `file removed`, `file not found`         |
| **Deploy**     | `<username> deploy`                    | File list or `no files found`            |
| **Log**        | `<username> log`                       | Operation list or `no logs found`        |

### Between `serverM` and Backend Servers (UDP)

#### Authentication (`serverA`)
- `auth <username> <encrypted_password>` → `auth_success` / `auth_failed`

#### Repository (`serverR`)
- `lookup`, `push`, `remove`, `deploy` handled with username & filename context

#### Deployment (`serverD`)
- `deploy <username> <filenames>` → `files deployed`

---

## ⚙️ Project Behavior & Limitations

### Assumptions
- Backend servers are already running before client connects
- Each user has one repository with unique filenames
- Inputs must follow the correct command format

### Failure Conditions
- Missing/corrupted input files (`members.txt`, `filenames.txt`, etc.)
- No concurrency handling (e.g., simultaneous `push`)
- Local file must exist before `push` is accepted

---

## 📚 Reused Code
- **Socket boilerplate code** adapted from [Beej’s Guide to Network Programming](https://beej.us/guide/bgnet/)
- All reused code is clearly attributed with comments in the source files

---

## 🛠️ Compilation & Usage

```bash
# Compile all executables
make all

# Start each component in separate terminals
./serverM
./serverA
./serverR
./serverD

# Run a client
./client <USERNAME> <PASSWORD>

# Clean Up
make clean

# Clean up Zombie processes
ps aux | grep <yourname>
kill -9 <PID>
