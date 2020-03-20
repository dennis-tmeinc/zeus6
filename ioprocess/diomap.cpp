
#include <fcntl.h>
#include <sched.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "../cfg.h"
#include "diomap.h"

struct dio_mmap* p_dio_mmap;

int dio_trylock()
{
    return __sync_lock_test_and_set(&(p_dio_mmap->lock), 1) == 0;
}

void dio_lock()
{
    while (__sync_lock_test_and_set(&(p_dio_mmap->lock), 1)) {
        static struct timespec ts = { 0, 1 };
        nanosleep(&ts, NULL); // or sched_yield?
    }
}

void dio_unlock()
{
    __sync_lock_release(&(p_dio_mmap->lock));
}

struct dio_mmap* dio_mmap()
{
    if (p_dio_mmap == NULL) {
        //int fd = open(DVRIOMAP, O_RDWR | O_CREAT, S_IRUSR|S_IWUSR);
        int fd = shm_open(DVRIOMAP, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
        if (fd <= 0) {
            return NULL;
        }
        ftruncate(fd, sizeof(struct dio_mmap));
        void* p = mmap(NULL, sizeof(struct dio_mmap), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        close(fd); // no more needed
        if (p != MAP_FAILED) {
            p_dio_mmap = (struct dio_mmap*)p;
        }
    }
    return p_dio_mmap;
}

void dio_munmap()
{
    if (p_dio_mmap != NULL) {
        munmap(p_dio_mmap, sizeof(struct dio_mmap));
        p_dio_mmap = NULL;
    }
}
