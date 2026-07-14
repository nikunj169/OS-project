#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

// Data file paths
#define STUDENTS_FILE "data/students.dat"
#define FACULTY_FILE "data/faculty.dat"
#define COURSES_FILE "data/courses.dat"
#define ENROLLMENTS_FILE "data/enrollments.dat"

// Record structures
typedef struct {
    int id;
    char name[100];
    char password[50];
    int active; // 1 = active, 0 = deactivated
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
    int active; // 1 = active, 0 = removed
} Course;

typedef struct {
    int student_id;
    int course_id;
} Enrollment;

void initDataFiles() {
    int fd;

    // Create data directory
    mkdir("data", 0777);

    // Create students file
    fd = open(STUDENTS_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        perror("Error creating students file");
        exit(1);
    }
    close(fd);

    // Create faculty file
    fd = open(FACULTY_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        perror("Error creating faculty file");
        exit(1);
    }
    close(fd);

    // Create courses file
    fd = open(COURSES_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        perror("Error creating courses file");
        exit(1);
    }
    close(fd);

    // Create enrollments file
    fd = open(ENROLLMENTS_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        perror("Error creating enrollments file");
        exit(1);
    }
    close(fd);

    // Add default admin (stored as faculty with id=0)
    Faculty admin;
    admin.id = 0;
    strcpy(admin.name, "Admin");
    strcpy(admin.password, "admin123");

    fd = open(FACULTY_FILE, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd < 0) {
        perror("Error opening faculty file");
        exit(1);
    }
    write(fd, &admin, sizeof(Faculty));
    close(fd);

    printf("Data files initialized successfully.\n");
    printf("Default admin credentials:\n");
    printf("  Username: Admin\n");
    printf("  Password: admin123\n");
}

int main() {
    initDataFiles();
    return 0;
}
