#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <pthread.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

#define PORT 8080
#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024

#define STUDENTS_FILE "data/students.dat"
#define FACULTY_FILE "data/faculty.dat"
#define COURSES_FILE "data/courses.dat"
#define ENROLLMENTS_FILE "data/enrollments.dat"

typedef struct {
    int id;
    char name[100];
    char password[50];
    int active;
} Student;

typedef struct {
    int id;
    char name[100];
    char password[50];
} Faculty;

typedef struct {
    int id;
    char name[100];
    int faculty_id;
    int max_seats;
    int active;
} Course;

typedef struct {
    int student_id;
    int course_id;
} Enrollment;

int server_fd;

pthread_mutex_t enrollment_mutex = PTHREAD_MUTEX_INITIALIZER;

void sendToClient(int client_fd, const char *msg) {
    write(client_fd, msg, strlen(msg));
}

int readFromClient(int client_fd, char *buffer, int size) {
    memset(buffer, 0, size);
    int bytes = read(client_fd, buffer, size - 1);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        if (bytes > 0 && buffer[bytes-1] == '\n')
            buffer[bytes-1] = '\0';
    }
    return bytes;
}

void lockFile(int fd, int lockType) {
    struct flock lock;
    lock.l_type = lockType;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    lock.l_pid = getpid();
    fcntl(fd, F_SETLKW, &lock);
}

void unlockFile(int fd) {
    struct flock lock;
    lock.l_type = F_UNLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    lock.l_pid = getpid();
    fcntl(fd, F_SETLK, &lock);
}

int authenticate(const char *username, const char *password, int *user_id, char *role) {
    int fd;

    fd = open(FACULTY_FILE, O_RDONLY);
    if (fd < 0) return 0;

    lockFile(fd, F_RDLCK);
    Faculty fac;
    while (read(fd, &fac, sizeof(Faculty)) == sizeof(Faculty)) {
        if (fac.id == 0 && strcmp(fac.name, username) == 0 && strcmp(fac.password, password) == 0) {
            unlockFile(fd);
            close(fd);
            strcpy(role, "admin");
            *user_id = 0;
            return 1;
        }
    }
    unlockFile(fd);
    close(fd);

    fd = open(STUDENTS_FILE, O_RDONLY);
    if (fd < 0) return 0;

    lockFile(fd, F_RDLCK);
    Student stu;
    while (read(fd, &stu, sizeof(Student)) == sizeof(Student)) {
        if (strcmp(stu.name, username) == 0 && strcmp(stu.password, password) == 0) {
            if (stu.active) {
                unlockFile(fd);
                close(fd);
                strcpy(role, "student");
                *user_id = stu.id;
                return 1;
            } else {
                unlockFile(fd);
                close(fd);
                strcpy(role, "inactive");
                *user_id = stu.id;
                return 1;
            }
        }
    }
    unlockFile(fd);
    close(fd);

    fd = open(FACULTY_FILE, O_RDONLY);
    if (fd < 0) return 0;

    lockFile(fd, F_RDLCK);
    while (read(fd, &fac, sizeof(Faculty)) == sizeof(Faculty)) {
        if (fac.id != 0 && strcmp(fac.name, username) == 0 && strcmp(fac.password, password) == 0) {
            unlockFile(fd);
            close(fd);
            strcpy(role, "faculty");
            *user_id = fac.id;
            return 1;
        }
    }
    unlockFile(fd);
    close(fd);

    return 0;
}

void handleAddStudent(int client_fd) {
    char buffer[BUFFER_SIZE];
    Student stu;
    int fd;

    sendToClient(client_fd, "Enter Student ID: ");
    readFromClient(client_fd, buffer, BUFFER_SIZE);
    stu.id = atoi(buffer);

    sendToClient(client_fd, "Enter Student Name: ");
    readFromClient(client_fd, buffer, BUFFER_SIZE);
    strncpy(stu.name, buffer, sizeof(stu.name)-1);
    stu.name[sizeof(stu.name) - 1] = '\0';

    sendToClient(client_fd, "Enter Password: ");
    readFromClient(client_fd, buffer, BUFFER_SIZE);
    strncpy(stu.password, buffer, sizeof(stu.password)-1);
    stu.password[sizeof(stu.password) - 1] = '\0';

    stu.active = 1;

    fd = open(STUDENTS_FILE, O_RDONLY);
    if (fd >= 0) {
        lockFile(fd, F_RDLCK);
        Student temp;
        while (read(fd, &temp, sizeof(Student)) == sizeof(Student)) {
            if (temp.id == stu.id) {
                unlockFile(fd);
                close(fd);
                sendToClient(client_fd, "Error: Student ID already exists.\n");
                return;
            }
        }
        unlockFile(fd);
        close(fd);
    }

    fd = open(STUDENTS_FILE, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd < 0) {
        sendToClient(client_fd, "Error: Could not open students file.\n");
        return;
    }

    lockFile(fd, F_WRLCK);
    if (write(fd, &stu, sizeof(Student)) != sizeof(Student)) {
        unlockFile(fd);
        close(fd);
        sendToClient(client_fd, "Error: Failed to add student record.\n");
        return;
    }
    unlockFile(fd);
    close(fd);

    sendToClient(client_fd, "Student added successfully.\n");
}

void handleAddFaculty(int client_fd) {
    char buffer[BUFFER_SIZE];
    Faculty fac;
    int fd;

    sendToClient(client_fd, "Enter Faculty ID: ");
    readFromClient(client_fd, buffer, BUFFER_SIZE);
    fac.id = atoi(buffer);

    sendToClient(client_fd, "Enter Faculty Name: ");
    readFromClient(client_fd, buffer, BUFFER_SIZE);
    strncpy(fac.name, buffer, sizeof(fac.name)-1);
    fac.name[sizeof(fac.name) - 1] = '\0';

    sendToClient(client_fd, "Enter Password: ");
    readFromClient(client_fd, buffer, BUFFER_SIZE);
    strncpy(fac.password, buffer, sizeof(fac.password)-1);
    fac.password[sizeof(fac.password) - 1] = '\0';

    fd = open(FACULTY_FILE, O_RDONLY);
    if (fd >= 0) {
        lockFile(fd, F_RDLCK);
        Faculty temp;
        while (read(fd, &temp, sizeof(Faculty)) == sizeof(Faculty)) {
            if (temp.id == fac.id) {
                unlockFile(fd);
                close(fd);
                sendToClient(client_fd, "Error: Faculty ID already exists.\n");
                return;
            }
        }
        unlockFile(fd);
        close(fd);
    }

    fd = open(FACULTY_FILE, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd < 0) {
        sendToClient(client_fd, "Error: Could not open faculty file.\n");
        return;
    }

    lockFile(fd, F_WRLCK);
    if (write(fd, &fac, sizeof(Faculty)) != sizeof(Faculty)) {
        unlockFile(fd);
        close(fd);
        sendToClient(client_fd, "Error: Failed to add faculty record.\n");
        return;
    }
    unlockFile(fd);
    close(fd);

    sendToClient(client_fd, "Faculty added successfully.\n");
}

void handleActivateDeactivate(int client_fd) {
    char buffer[BUFFER_SIZE];
    int student_id, action;

    sendToClient(client_fd, "Enter Student ID: ");
    readFromClient(client_fd, buffer, BUFFER_SIZE);
    student_id = atoi(buffer);

    sendToClient(client_fd, "Enter 1 to Activate, 0 to Deactivate: ");
    readFromClient(client_fd, buffer, BUFFER_SIZE);
    action = atoi(buffer);

    int fd = open(STUDENTS_FILE, O_RDWR);
    if (fd < 0) {
        sendToClient(client_fd, "Error: Could not open students file.\n");
        return;
    }

    lockFile(fd, F_WRLCK);
    Student stu;
    int found = 0;

    while (read(fd, &stu, sizeof(Student)) == sizeof(Student)) {
        if (stu.id == student_id) {
            stu.active = action;
            if (lseek(fd, -(off_t)sizeof(Student), SEEK_CUR) == (off_t)-1) {
                perror("lseek failed");
                break;
            }
            if (write(fd, &stu, sizeof(Student)) != sizeof(Student)) {
                perror("Failed to update student");
                break;
            }
            found = 1;
            break;
        }
    }

    unlockFile(fd);
    close(fd);

    if (found) {
        sendToClient(client_fd, action ? "Student activated successfully.\n" : "Student deactivated successfully.\n");
    } else {
        sendToClient(client_fd, "Error: Student not found.\n");
    }
}

void handleUpdateStudent(int client_fd) {
    char buffer[BUFFER_SIZE];
    int student_id;

    sendToClient(client_fd, "Enter Student ID to update: ");
    readFromClient(client_fd, buffer, BUFFER_SIZE);
    student_id = atoi(buffer);

    int fd = open(STUDENTS_FILE, O_RDWR);
    if (fd < 0) {
        sendToClient(client_fd, "Error: Could not open students file.\n");
        return;
    }

    lockFile(fd, F_WRLCK);
    Student stu;
    int found = 0;

    while (read(fd, &stu, sizeof(Student)) == sizeof(Student)) {
        if (stu.id == student_id) {
            sendToClient(client_fd, "Enter new Name (or press Enter to skip): ");
            readFromClient(client_fd, buffer, BUFFER_SIZE);
            if (strlen(buffer) > 0) {
                strncpy(stu.name, buffer, sizeof(stu.name)-1);
                stu.name[sizeof(stu.name) - 1] = '\0';
            }

            sendToClient(client_fd, "Enter new Password (or press Enter to skip): ");
            readFromClient(client_fd, buffer, BUFFER_SIZE);
            if (strlen(buffer) > 0) {
                strncpy(stu.password, buffer, sizeof(stu.password)-1);
                stu.password[sizeof(stu.password) - 1] = '\0';
            }

            if (lseek(fd, -(off_t)sizeof(Student), SEEK_CUR) == (off_t)-1) {
                perror("lseek failed");
                break;
            }
            if (write(fd, &stu, sizeof(Student)) != sizeof(Student)) {
                perror("Failed to update student");
                break;
            }
            found = 1;
            break;
        }
    }

    unlockFile(fd);
    close(fd);

    if (found) {
        sendToClient(client_fd, "Student details updated successfully.\n");
    } else {
        sendToClient(client_fd, "Error: Student not found.\n");
    }
}

void handleUpdateFaculty(int client_fd) {
    char buffer[BUFFER_SIZE];
    int faculty_id;

    sendToClient(client_fd, "Enter Faculty ID to update: ");
    readFromClient(client_fd, buffer, BUFFER_SIZE);
    faculty_id = atoi(buffer);

    int fd = open(FACULTY_FILE, O_RDWR);
    if (fd < 0) {
        sendToClient(client_fd, "Error: Could not open faculty file.\n");
        return;
    }

    lockFile(fd, F_WRLCK);
    Faculty fac;
    int found = 0;

    while (read(fd, &fac, sizeof(Faculty)) == sizeof(Faculty)) {
        if (fac.id == faculty_id) {
            sendToClient(client_fd, "Enter new Name (or press Enter to skip): ");
            readFromClient(client_fd, buffer, BUFFER_SIZE);
            if (strlen(buffer) > 0) {
                strncpy(fac.name, buffer, sizeof(fac.name)-1);
                fac.name[sizeof(fac.name) - 1] = '\0';
            }

            sendToClient(client_fd, "Enter new Password (or press Enter to skip): ");
            readFromClient(client_fd, buffer, BUFFER_SIZE);
            if (strlen(buffer) > 0) {
                strncpy(fac.password, buffer, sizeof(fac.password)-1);
                fac.password[sizeof(fac.password) - 1] = '\0';
            }

            if (lseek(fd, -(off_t)sizeof(Faculty), SEEK_CUR) == (off_t)-1) {
                perror("lseek failed");
                break;
            }
            if (write(fd, &fac, sizeof(Faculty)) != sizeof(Faculty)) {
                perror("Failed to update faculty");
                break;
            }
            found = 1;
            break;
        }
    }

    unlockFile(fd);
    close(fd);

    if (found) {
        sendToClient(client_fd, "Faculty details updated successfully.\n");
    } else {
        sendToClient(client_fd, "Error: Faculty not found.\n");
    }
}

void handleEnrollCourse(int client_fd, int student_id) {
    char buffer[BUFFER_SIZE];
    int course_id;
    int fd;

    fd = open(COURSES_FILE, O_RDONLY);
    if (fd < 0) {
        sendToClient(client_fd, "No courses available.\n");
        return;
    }

    lockFile(fd, F_RDLCK);
    Course course;
    char response[4096];
    int count = 0;
    size_t used = 0;

    used += snprintf(response + used, sizeof(response) - used,
                     "\n=== Available Courses ===\n");

    while (read(fd, &course, sizeof(Course)) == sizeof(Course)) {
        if (course.active) {
            if (used < sizeof(response)) {
                used += snprintf(response + used, sizeof(response) - used,
                                 "ID: %d | Name: %s | Seats: %d\n",
                                 course.id, course.name, course.max_seats);
            }
            count++;
        }
    }
    unlockFile(fd);
    close(fd);

    if (count == 0) {
        sendToClient(client_fd, "No courses available.\n");
        return;
    }

    if (used < sizeof(response)) {
        used += snprintf(response + used, sizeof(response) - used,
                         "=========================\n");
    }
    sendToClient(client_fd, response);

    sendToClient(client_fd, "Enter Course ID to enroll: ");
    readFromClient(client_fd, buffer, BUFFER_SIZE);
    course_id = atoi(buffer);

    pthread_mutex_lock(&enrollment_mutex);

    fd = open(ENROLLMENTS_FILE, O_RDONLY);
    if (fd >= 0) {
        lockFile(fd, F_RDLCK);
        Enrollment en;
        while (read(fd, &en, sizeof(Enrollment)) == sizeof(Enrollment)) {
            if (en.student_id == student_id && en.course_id == course_id) {
                unlockFile(fd);
                close(fd);
                pthread_mutex_unlock(&enrollment_mutex);
                sendToClient(client_fd, "Error: Already enrolled in this course.\n");
                return;
            }
        }
        unlockFile(fd);
        close(fd);
    }

    fd = open(COURSES_FILE, O_RDWR);
    if (fd < 0) {
        pthread_mutex_unlock(&enrollment_mutex);
        sendToClient(client_fd, "Error: Could not open courses file.\n");
        return;
    }

    lockFile(fd, F_WRLCK);
    Course targetCourse;
    int found = 0;

    while (read(fd, &targetCourse, sizeof(Course)) == sizeof(Course)) {
        if (targetCourse.id == course_id && targetCourse.active) {
            if (targetCourse.max_seats <= 0) {
                unlockFile(fd);
                close(fd);
                pthread_mutex_unlock(&enrollment_mutex);
                sendToClient(client_fd, "Error: No seats available in this course.\n");
                return;
            }

            targetCourse.max_seats--;
            if (lseek(fd, -(off_t)sizeof(Course), SEEK_CUR) == (off_t)-1) {
                unlockFile(fd);
                close(fd);
                pthread_mutex_unlock(&enrollment_mutex);
                sendToClient(client_fd, "Error: Failed to update course.\n");
                return;
            }
            if (write(fd, &targetCourse, sizeof(Course)) != sizeof(Course)) {
                unlockFile(fd);
                close(fd);
                pthread_mutex_unlock(&enrollment_mutex);
                sendToClient(client_fd, "Error: Failed to update course.\n");
                return;
            }
            found = 1;
            break;
        }
    }

    unlockFile(fd);
    close(fd);

    if (!found) {
        pthread_mutex_unlock(&enrollment_mutex);
        sendToClient(client_fd, "Error: Course not found or not active.\n");
        return;
    }

    Enrollment en;
    en.student_id = student_id;
    en.course_id = course_id;

    fd = open(ENROLLMENTS_FILE, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd < 0) {
        pthread_mutex_unlock(&enrollment_mutex);
        sendToClient(client_fd, "Error: Could not open enrollments file.\n");
        return;
    }

    lockFile(fd, F_WRLCK);
    if (write(fd, &en, sizeof(Enrollment)) != sizeof(Enrollment)) {
        unlockFile(fd);
        close(fd);
        pthread_mutex_unlock(&enrollment_mutex);
        sendToClient(client_fd, "Error: Failed to write enrollment record.\n");
        return;
    }
    unlockFile(fd);
    close(fd);

    pthread_mutex_unlock(&enrollment_mutex);
    sendToClient(client_fd, "Successfully enrolled in the course.\n");
}

void handleUnenrollCourse(int client_fd, int student_id) {
    char buffer[BUFFER_SIZE];
    int course_id;
    int fd;

    fd = open(ENROLLMENTS_FILE, O_RDONLY);
    if (fd < 0) {
        sendToClient(client_fd, "No enrollments found.\n");
        return;
    }

    lockFile(fd, F_RDLCK);
    Enrollment en;
    char response[4096];
    int count = 0;
    size_t used = 0;

    used += snprintf(response + used, sizeof(response) - used,
                     "\n=== Your Enrolled Courses ===\n");

    while (read(fd, &en, sizeof(Enrollment)) == sizeof(Enrollment)) {
        if (en.student_id == student_id) {
            Course course;
            int cfd = open(COURSES_FILE, O_RDONLY);
            if (cfd >= 0) {
                lockFile(cfd, F_RDLCK);
                while (read(cfd, &course, sizeof(Course)) == sizeof(Course)) {
                    if (course.id == en.course_id) {
                        if (used < sizeof(response)) {
                            used += snprintf(response + used, sizeof(response) - used,
                                             "Course ID: %d | Name: %s\n",
                                             course.id, course.name);
                        }
                        count++;
                        break;
                    }
                }
                unlockFile(cfd);
                close(cfd);
            }
        }
    }
    unlockFile(fd);
    close(fd);

    if (count == 0) {
        sendToClient(client_fd, "No enrollments found.\n");
        return;
    }

    if (used < sizeof(response)) {
        used += snprintf(response + used, sizeof(response) - used,
                         "=============================\n");
    }
    sendToClient(client_fd, response);

    sendToClient(client_fd, "Enter Course ID to unenroll: ");
    readFromClient(client_fd, buffer, BUFFER_SIZE);
    course_id = atoi(buffer);

    pthread_mutex_lock(&enrollment_mutex);

    fd = open(ENROLLMENTS_FILE, O_RDONLY);
    if (fd < 0) {
        pthread_mutex_unlock(&enrollment_mutex);
        sendToClient(client_fd, "Error: Could not open enrollments file.\n");
        return;
    }

    int temp_fd = open("data/enrollments.tmp", O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (temp_fd < 0) {
        close(fd);
        pthread_mutex_unlock(&enrollment_mutex);
        sendToClient(client_fd, "Error: Could not create temp file.\n");
        return;
    }

    lockFile(fd, F_RDLCK);
    lockFile(temp_fd, F_WRLCK);
    int found = 0;

    while (read(fd, &en, sizeof(Enrollment)) == sizeof(Enrollment)) {
        if (en.student_id == student_id && en.course_id == course_id) {
            found = 1;
            int cfd = open(COURSES_FILE, O_RDWR);
            if (cfd >= 0) {
                lockFile(cfd, F_WRLCK);
                Course c;
                while (read(cfd, &c, sizeof(Course)) == sizeof(Course)) {
                    if (c.id == course_id) {
                        c.max_seats++;
                        if (lseek(cfd, -(off_t)sizeof(Course), SEEK_CUR) == (off_t)-1) {
                            perror("lseek failed");
                            break;
                        }
                        if (write(cfd, &c, sizeof(Course)) != sizeof(Course)) {
                            perror("Failed to update course seats");
                            break;
                        }
                        break;
                    }
                }
                unlockFile(cfd);
                close(cfd);
            }
        } else {
            if (write(temp_fd, &en, sizeof(Enrollment)) != sizeof(Enrollment)) {
                perror("Failed to write enrollment to temp file");
            }
        }
    }

    unlockFile(fd);
    unlockFile(temp_fd);
    close(fd);
    close(temp_fd);

    remove(ENROLLMENTS_FILE);
    rename("data/enrollments.tmp", ENROLLMENTS_FILE);

    pthread_mutex_unlock(&enrollment_mutex);

    if (found) {
        sendToClient(client_fd, "Successfully unenrolled from the course.\n");
    } else {
        sendToClient(client_fd, "Error: Enrollment not found.\n");
    }
}

void handleViewEnrollments(int client_fd, int student_id) {
    int fd = open(ENROLLMENTS_FILE, O_RDONLY);
    if (fd < 0) {
        sendToClient(client_fd, "No enrollments found.\n");
        return;
    }

    lockFile(fd, F_RDLCK);
    Enrollment en;
    char response[4096];
    int count = 0;
    size_t used = 0;

    used += snprintf(response + used, sizeof(response) - used,
                     "\n=== Your Enrolled Courses ===\n");

    while (read(fd, &en, sizeof(Enrollment)) == sizeof(Enrollment)) {
        if (en.student_id == student_id) {
            Course course;
            int cfd = open(COURSES_FILE, O_RDONLY);
            if (cfd >= 0) {
                lockFile(cfd, F_RDLCK);
                while (read(cfd, &course, sizeof(Course)) == sizeof(Course)) {
                    if (course.id == en.course_id) {
                        if (used < sizeof(response)) {
                            used += snprintf(response + used, sizeof(response) - used,
                                             "Course ID: %d | Name: %s\n",
                                             course.id, course.name);
                        }
                        count++;
                        break;
                    }
                }
                unlockFile(cfd);
                close(cfd);
            }
        }
    }

    unlockFile(fd);
    close(fd);

    if (count == 0) {
        sendToClient(client_fd, "No enrollments found.\n");
    } else {
        if (used < sizeof(response)) {
            snprintf(response + used, sizeof(response) - used,
                     "=============================\n");
        }
        sendToClient(client_fd, response);
    }
}

void handleChangePassword(int client_fd, int user_id, const char *role) {
    char buffer[BUFFER_SIZE];
    char newPass[50];

    sendToClient(client_fd, "Enter new Password: ");
    readFromClient(client_fd, buffer, BUFFER_SIZE);
    strncpy(newPass, buffer, sizeof(newPass)-1);
    newPass[sizeof(newPass) - 1] = '\0';

    if (strcmp(role, "student") == 0) {
        int fd = open(STUDENTS_FILE, O_RDWR);
        if (fd < 0) {
            sendToClient(client_fd, "Error: Could not open students file.\n");
            return;
        }

        lockFile(fd, F_WRLCK);
        Student stu;
        int found = 0;

        while (read(fd, &stu, sizeof(Student)) == sizeof(Student)) {
            if (stu.id == user_id) {
                strncpy(stu.password, newPass, sizeof(stu.password)-1);
                stu.password[sizeof(stu.password) - 1] = '\0';
                if (lseek(fd, -(off_t)sizeof(Student), SEEK_CUR) == (off_t)-1) {
                    perror("lseek failed");
                    break;
                }
                if (write(fd, &stu, sizeof(Student)) != sizeof(Student)) {
                    perror("Failed to update password");
                    break;
                }
                found = 1;
                break;
            }
        }

        unlockFile(fd);
        close(fd);

        if (found) {
            sendToClient(client_fd, "Password changed successfully.\n");
        } else {
            sendToClient(client_fd, "Error: User not found.\n");
        }
    } else if (strcmp(role, "faculty") == 0) {
        int fd = open(FACULTY_FILE, O_RDWR);
        if (fd < 0) {
            sendToClient(client_fd, "Error: Could not open faculty file.\n");
            return;
        }

        lockFile(fd, F_WRLCK);
        Faculty fac;
        int found = 0;

        while (read(fd, &fac, sizeof(Faculty)) == sizeof(Faculty)) {
            if (fac.id == user_id) {
                strncpy(fac.password, newPass, sizeof(fac.password)-1);
                fac.password[sizeof(fac.password) - 1] = '\0';
                if (lseek(fd, -(off_t)sizeof(Faculty), SEEK_CUR) == (off_t)-1) {
                    perror("lseek failed");
                    break;
                }
                if (write(fd, &fac, sizeof(Faculty)) != sizeof(Faculty)) {
                    perror("Failed to update password");
                    break;
                }
                found = 1;
                break;
            }
        }

        unlockFile(fd);
        close(fd);

        if (found) {
            sendToClient(client_fd, "Password changed successfully.\n");
        } else {
            sendToClient(client_fd, "Error: User not found.\n");
        }
    }
}

void handleAddCourse(int client_fd, int faculty_id) {
    char buffer[BUFFER_SIZE];
    Course course;
    int fd;

    sendToClient(client_fd, "Enter Course ID: ");
    readFromClient(client_fd, buffer, BUFFER_SIZE);
    course.id = atoi(buffer);

    sendToClient(client_fd, "Enter Course Name: ");
    readFromClient(client_fd, buffer, BUFFER_SIZE);
    strncpy(course.name, buffer, sizeof(course.name)-1);
    course.name[sizeof(course.name) - 1] = '\0';

    sendToClient(client_fd, "Enter Max Seats: ");
    readFromClient(client_fd, buffer, BUFFER_SIZE);
    course.max_seats = atoi(buffer);

    course.faculty_id = faculty_id;
    course.active = 1;

    fd = open(COURSES_FILE, O_RDONLY);
    if (fd >= 0) {
        lockFile(fd, F_RDLCK);
        Course temp;
        while (read(fd, &temp, sizeof(Course)) == sizeof(Course)) {
            if (temp.id == course.id) {
                unlockFile(fd);
                close(fd);
                sendToClient(client_fd, "Error: Course ID already exists.\n");
                return;
            }
        }
        unlockFile(fd);
        close(fd);
    }

    fd = open(COURSES_FILE, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd < 0) {
        sendToClient(client_fd, "Error: Could not open courses file.\n");
        return;
    }

    lockFile(fd, F_WRLCK);
    if (write(fd, &course, sizeof(Course)) != sizeof(Course)) {
        unlockFile(fd);
        close(fd);
        sendToClient(client_fd, "Error: Failed to add course record.\n");
        return;
    }
    unlockFile(fd);
    close(fd);

    sendToClient(client_fd, "Course added successfully.\n");
}

void handleRemoveCourse(int client_fd, int faculty_id) {
    char buffer[BUFFER_SIZE];
    int course_id;

    int fd = open(COURSES_FILE, O_RDONLY);
    if (fd < 0) {
        sendToClient(client_fd, "No courses found.\n");
        return;
    }

    lockFile(fd, F_RDLCK);
    Course course;
    char response[4096];
    int count = 0;
    size_t used = 0;

    used += snprintf(response + used, sizeof(response) - used,
                     "\n=== Your Courses ===\n");

    while (read(fd, &course, sizeof(Course)) == sizeof(Course)) {
        if (course.faculty_id == faculty_id && course.active) {
            if (used < sizeof(response)) {
                used += snprintf(response + used, sizeof(response) - used,
                                 "ID: %d | Name: %s | Seats: %d\n",
                                 course.id, course.name, course.max_seats);
            }
            count++;
        }
    }
    unlockFile(fd);
    close(fd);

    if (count == 0) {
        sendToClient(client_fd, "No courses found.\n");
        return;
    }

    if (used < sizeof(response)) {
        snprintf(response + used, sizeof(response) - used,
                 "====================\n");
    }
    sendToClient(client_fd, response);

    sendToClient(client_fd, "Enter Course ID to remove: ");
    readFromClient(client_fd, buffer, BUFFER_SIZE);
    course_id = atoi(buffer);

    fd = open(COURSES_FILE, O_RDWR);
    if (fd < 0) {
        sendToClient(client_fd, "Error: Could not open courses file.\n");
        return;
    }

    lockFile(fd, F_WRLCK);
    Course targetCourse;
    int found = 0;

    while (read(fd, &targetCourse, sizeof(Course)) == sizeof(Course)) {
        if (targetCourse.id == course_id && targetCourse.faculty_id == faculty_id) {
            targetCourse.active = 0;
            if (lseek(fd, -(off_t)sizeof(Course), SEEK_CUR) == (off_t)-1) {
                perror("lseek failed");
                break;
            }
            if (write(fd, &targetCourse, sizeof(Course)) != sizeof(Course)) {
                perror("Failed to update course");
                break;
            }
            found = 1;
            break;
        }
    }

    unlockFile(fd);
    close(fd);

    if (found) {
        sendToClient(client_fd, "Course removed successfully.\n");
    } else {
        sendToClient(client_fd, "Error: Course not found or you don't have permission.\n");
    }
}

void handleViewCourseEnrollments(int client_fd, int faculty_id) {
    int fd = open(COURSES_FILE, O_RDONLY);
    if (fd < 0) {
        sendToClient(client_fd, "No courses found.\n");
        return;
    }

    lockFile(fd, F_RDLCK);
    Course course;
    char response[8192];
    int found = 0;
    size_t used = 0;

    used += snprintf(response + used, sizeof(response) - used,
                     "\n=== Course Enrollments ===\n");

    while (read(fd, &course, sizeof(Course)) == sizeof(Course)) {
        if (course.faculty_id == faculty_id && course.active) {
            int efd = open(ENROLLMENTS_FILE, O_RDONLY);
            int enrollCount = 0;

            if (efd >= 0) {
                lockFile(efd, F_RDLCK);
                Enrollment en;
                while (read(efd, &en, sizeof(Enrollment)) == sizeof(Enrollment)) {
                    if (en.course_id == course.id) {
                        enrollCount++;
                    }
                }
                unlockFile(efd);
                close(efd);
            }

            if (used < sizeof(response)) {
                used += snprintf(response + used, sizeof(response) - used,
                                 "Course: %s (ID: %d) - Enrolled: %d/%d\n",
                                 course.name, course.id, enrollCount, enrollCount + course.max_seats);
            }
            found = 1;
        }
    }
    unlockFile(fd);
    close(fd);

    if (!found) {
        sendToClient(client_fd, "No courses found.\n");
    } else {
        if (used < sizeof(response)) {
            snprintf(response + used, sizeof(response) - used,
                     "==========================\n");
        }
        sendToClient(client_fd, response);
    }
}

void handleAdminMenu(int client_fd) {
    char buffer[BUFFER_SIZE];
    int choice;

    while (1) {
        sendToClient(client_fd, "\n===== Admin Menu =====\n");
        sendToClient(client_fd, "1. Add Student\n");
        sendToClient(client_fd, "2. Add Faculty\n");
        sendToClient(client_fd, "3. Activate/Deactivate Student\n");
        sendToClient(client_fd, "4. Update Student Details\n");
        sendToClient(client_fd, "5. Update Faculty Details\n");
        sendToClient(client_fd, "6. Exit\n");
        sendToClient(client_fd, "Enter choice: ");

        readFromClient(client_fd, buffer, BUFFER_SIZE);
        choice = atoi(buffer);

        switch (choice) {
            case 1: handleAddStudent(client_fd); break;
            case 2: handleAddFaculty(client_fd); break;
            case 3: handleActivateDeactivate(client_fd); break;
            case 4: handleUpdateStudent(client_fd); break;
            case 5: handleUpdateFaculty(client_fd); break;
            case 6: sendToClient(client_fd, "Logging out...\n"); return;
            default: sendToClient(client_fd, "Invalid choice. Try again.\n");
        }
    }
}

void handleStudentMenu(int client_fd, int student_id) {
    char buffer[BUFFER_SIZE];
    int choice;

    while (1) {
        sendToClient(client_fd, "\n===== Student Menu =====\n");
        sendToClient(client_fd, "1. Enroll to new Course\n");
        sendToClient(client_fd, "2. Unenroll from Course\n");
        sendToClient(client_fd, "3. View Enrolled Courses\n");
        sendToClient(client_fd, "4. Change Password\n");
        sendToClient(client_fd, "5. Exit\n");
        sendToClient(client_fd, "Enter choice: ");

        readFromClient(client_fd, buffer, BUFFER_SIZE);
        choice = atoi(buffer);

        switch (choice) {
            case 1: handleEnrollCourse(client_fd, student_id); break;
            case 2: handleUnenrollCourse(client_fd, student_id); break;
            case 3: handleViewEnrollments(client_fd, student_id); break;
            case 4: handleChangePassword(client_fd, student_id, "student"); break;
            case 5: sendToClient(client_fd, "Logging out...\n"); return;
            default: sendToClient(client_fd, "Invalid choice. Try again.\n");
        }
    }
}

void handleFacultyMenu(int client_fd, int faculty_id) {
    char buffer[BUFFER_SIZE];
    int choice;

    while (1) {
        sendToClient(client_fd, "\n===== Faculty Menu =====\n");
        sendToClient(client_fd, "1. Add new Course\n");
        sendToClient(client_fd, "2. Remove offered Course\n");
        sendToClient(client_fd, "3. View Enrollments in Courses\n");
        sendToClient(client_fd, "4. Change Password\n");
        sendToClient(client_fd, "5. Exit\n");
        sendToClient(client_fd, "Enter choice: ");

        readFromClient(client_fd, buffer, BUFFER_SIZE);
        choice = atoi(buffer);

        switch (choice) {
            case 1: handleAddCourse(client_fd, faculty_id); break;
            case 2: handleRemoveCourse(client_fd, faculty_id); break;
            case 3: handleViewCourseEnrollments(client_fd, faculty_id); break;
            case 4: handleChangePassword(client_fd, faculty_id, "faculty"); break;
            case 5: sendToClient(client_fd, "Logging out...\n"); return;
            default: sendToClient(client_fd, "Invalid choice. Try again.\n");
        }
    }
}

void* handleClient(void *arg) {
    int client_fd = *(int*)arg;
    free(arg);

    char username[100], password[50];
    int user_id;
    char role[20];

    sendToClient(client_fd, "===== Welcome to Academia - Course Registration Portal =====\n");

    while (1) {
        sendToClient(client_fd, "\nLogin\n");
        sendToClient(client_fd, "Username: ");
        readFromClient(client_fd, username, sizeof(username));

        sendToClient(client_fd, "Password: ");
        readFromClient(client_fd, password, sizeof(password));

        if (!authenticate(username, password, &user_id, role)) {
            sendToClient(client_fd, "Invalid credentials. Please try again.\n");
            continue;
        }

        if (strcmp(role, "inactive") == 0) {
            sendToClient(client_fd, "Account is deactivated. Contact admin.\n");
            continue;
        }

        break;
    }

    sendToClient(client_fd, "Login successful!\n");

    if (strcmp(role, "admin") == 0) {
        handleAdminMenu(client_fd);
    } else if (strcmp(role, "student") == 0) {
        handleStudentMenu(client_fd, user_id);
    } else if (strcmp(role, "faculty") == 0) {
        handleFacultyMenu(client_fd, user_id);
    }

    close(client_fd);
    return NULL;
}

int main() {
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);

    mkdir("data", 0777);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("Setsockopt failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server started on port %d\n", PORT);
    printf("Waiting for connections...\n");

    while (1) {
        int *client_fd = malloc(sizeof(int));
        *client_fd = accept(server_fd, (struct sockaddr *)&address, &addrlen);

        if (*client_fd < 0) {
            perror("Accept failed");
            free(client_fd);
            continue;
        }

        printf("New client connected\n");

        pthread_t thread;
        if (pthread_create(&thread, NULL, handleClient, client_fd) != 0) {
            perror("Thread creation failed");
            close(*client_fd);
            free(client_fd);
        }

        pthread_detach(thread);
    }

    close(server_fd);
    return 0;
}
