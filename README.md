# Academia - Course Registration Portal

A C-based client-server application for course registration using socket programming, multithreading, and file locking.

## Features

- **Admin**: Add students/faculty, activate/deactivate students, update details
- **Student**: Enroll/unenroll courses, view enrollments, change password
- **Faculty**: Add/remove courses, view enrollments, change password

## System Calls Used

- Socket programming (TCP)
- POSIX threads (pthreads)
- File I/O with system calls (open, read, write, lseek, close)
- File locking (fcntl)
- Process management (fork-like via pthreads)

## Build

```bash
make clean
make
```

## Initialize Data

```bash
./init
```

Default admin credentials:
- Username: Admin
- Password: admin123

## Run

### Terminal 1 - Start Server
```bash
./server
```

### Terminal 2 - Connect Client
```bash
./client
```

You can open multiple terminals to simulate multiple concurrent clients.

## Data Files

- `data/students.dat` - Student records
- `data/faculty.dat` - Faculty records (includes admin)
- `data/courses.dat` - Course records
- `data/enrollments.dat` - Enrollment records

## Architecture

- Server handles multiple clients concurrently using pthreads
- File locking ensures data consistency with read/write locks
- All data persisted in binary files using system calls
