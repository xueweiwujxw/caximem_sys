#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <pthread.h>

#include "caximem_ioctl.h"

#define BUFF_SIZE 200

struct file_read
{
    int fd;
    int read_ret;
    void *sbuf;
};

void *read_thread(void *arg) {
    struct file_read *file = (struct file_read *)arg;
    char rbuf[BUFF_SIZE] = {0};
    while (1) {
        int ret = read(file->fd, rbuf, sizeof(rbuf));
        if (ret < 0) {
            perror("read caximem\n");
            pthread_exit(NULL);
        } else if (ret == 0) {
            break;
        } else {
            if (memcmp(rbuf, file->sbuf, ret)) {
                printf("compare failed\n");
            } else {
                printf("check success\n");
            }
        }
        file->read_ret = ret;
    }
    pthread_exit(file);
}

int main(int argc, char const *argv[]) {

    // open
    int fd = open("/dev/caximem_0", O_RDWR | O_EXCL);
    if (fd < 0) {
        printf("open failed. %s.\n", strerror(errno));
        return 0;
    }

    // ready to write and read
    int ret = 0;
    char sbuf[BUFF_SIZE] = {0};
    for (int i = 0; i < BUFF_SIZE; ++i) {
        sbuf[i] = i;
    }

    // create a thread to read
    struct file_read file_args = {
        .fd = fd,
        .read_ret = 0,
        .sbuf = sbuf};
    pthread_t pid;
    if (pthread_create(&pid, NULL, &read_thread, &file_args) == -1) {
        perror("read pthread create");
        return 1;
    }

    // write
    ret = write(fd, sbuf, BUFF_SIZE);
    if (ret < 0)
        printf("write failed. %s.\n", strerror(errno));

    // cancel all transfer with reset
    sleep(1);
    ret = ioctl(fd, CAXIMEM_CANCEL);
    if (ret < 0)
        printf("ioctl failed. %s.\n", strerror(errno));

    void *pret;
    pthread_join(pid, &pret);
    if (pret != NULL) {
        printf("%d read ret %d.\n", ((struct file_read *)pret)->fd, ((struct file_read *)pret)->read_ret);
    }
    close(fd);
    return 0;
}
