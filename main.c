#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <string.h>

#include <errno.h>
#include <stdbool.h>

#define SO_EOF -1
#define SEEK_SET	0	/* Seek from beginning of file.  */
#define SEEK_CUR	1	/* Seek from current position.  */
#define SEEK_END	2	/* Seek from end of file.  */

#define DEFAULT_BUF_SIZE 4096

struct _so_file{
    char* pathname;
    char mode;
    int fd;
    char buf[DEFAULT_BUF_SIZE];
    int size;
    int buf_cursor;
    int cursor;
    bool can_flush;
    bool error;
    bool eof;
};

typedef struct _so_file SO_FILE;

int min(int a, int b){
    if(a < b){
        return a;
    }

    return b;
}

int max(int a, int b){
    if(a > b){
        return a;
    }

    return b;
}

SO_FILE *so_fopen(const char *pathname, const char *mode){

    int fd = -1;
    int mode_len = strlen(mode);
    
    //check the open mode
    if(mode[0] == 'r'){
        if(mode_len > 1 && mode[1] == '+'){
            fd = open(pathname, O_RDWR);   
        }else{
            fd = open(pathname, O_RDONLY);  
        }
    }else if(mode[0] == 'w'){
        if(mode_len > 1 && mode[1] == '+'){
            fd = open(pathname, O_RDWR | O_CREAT | O_TRUNC, 0644);
        }else{
            fd = open(pathname, O_WRONLY | O_CREAT | O_TRUNC, 0644);  
        }
    }else if(mode[0] == 'a'){
        if(mode_len > 1 && mode[1] == '+'){
            fd = open(pathname, O_RDWR | O_APPEND | O_CREAT, 0644);
        }else{
            fd = open(pathname, O_WRONLY | O_APPEND | O_CREAT, 0644);
        }
    }else{
        printf("Invalid mode!");
        return NULL;
    } 

    if(fd == -1){
        return NULL;
    }

    //initialize the structure
    SO_FILE* file = malloc(sizeof(SO_FILE));
    int path_len = strlen(pathname);
    file->pathname = malloc(sizeof(char) * path_len + 1);
    memcpy(file->pathname, pathname, path_len);
    file->pathname[path_len] = '\0';
    file->fd = fd;
    file->size = 0;
    file->buf_cursor = 0;
    file->can_flush = false;
    file->mode = mode[0];
    file->cursor = 0;
    file->error = false;
    file->eof = false;

    return file;
}

int so_fclose(SO_FILE *stream){

    int flush_err = 0;

    //flush the buffer before closing the file
    if(stream->can_flush){
        flush_err = so_fflush(stream);
    }

    int close_err = close(stream->fd);
    free(stream->pathname);
    free(stream);

    //returns an error from the flushing/closing process
    return -(close_err || flush_err);
}

int so_fgetc(SO_FILE *stream){

    //check the buffer's state
    if(stream->size == 0 || stream->buf_cursor == stream->size){
        int err = read(stream->fd, stream->buf, sizeof(unsigned char) * BUF_FILL_SIZE);

        if(err <= 0){
            stream->error = true;
            if(err == 0){
                stream->eof = true;
            }
            return SO_EOF;
        }

        stream->size = err;
        //move the cursor to the beginning of the buffer
        stream->buf_cursor = 0;
    }
    
    //move the file's cursor
    ++stream->cursor;

    return (int)stream->buf[stream->buf_cursor++];
}

int so_fputc(int c, SO_FILE *stream){

    stream->buf[stream->size++] = (char)c;
    ++stream->cursor;
    stream->can_flush = true;

    //if the buffer got full flush it
    if(stream->size == BUF_FILL_SIZE){
        so_fflush(stream);
    }

    return c;
}

size_t so_fread(void *ptr, size_t size, size_t nmemb, SO_FILE *stream){
    
    //how many bytes we already read
    int processed = 0;
    //count how many bytes we have to read
    int total = nmemb * size;

    while(total - processed > 0){
        //if the buffer is full
        if(stream->buf_cursor == stream->size){
            int err = read(stream->fd, stream->buf, sizeof(char) * BUF_FILL_SIZE);

            if(err <= 0){
                stream->error = true;
                if(err == 0){
                    stream->eof = true;
                }
                return processed;            
            }

            //if the buffer won't get fully filled make sure it won t use data from previous buffers
            stream->buf[err] = '\0';
            stream->size = err;
            stream->buf_cursor = 0;
        }

        //count how many bytes I will copy depending on the buffer size and on my needs
        int copy_len = min(total - processed, stream->size - stream->buf_cursor);
        memcpy(&((char*)ptr)[processed], &stream->buf[stream->buf_cursor], copy_len);

        //move the cursor to the next byte
        stream->buf_cursor += copy_len;

        //count how many bytes we processed
        processed += copy_len;
    }

    //move the file's cursor
    stream->cursor += processed;

    return processed;
}

size_t so_fwrite(const void *ptr, size_t size, size_t nmemb, SO_FILE *stream){

    //how many bytes I already wrote
    int processed = 0;
    //count how many bytes I have to write
    int total = nmemb * size;
    int err;

    while(total - processed > 0){

        //flush the buffer if full
        if(stream->size == BUF_FILL_SIZE){
            err = so_fflush(stream);
            stream->can_flush = false;
        }

        //count how many bytes I will write depending on the buffer size and on my needs
        int copy_len = min(total - processed, BUF_FILL_SIZE - stream->size);
        memcpy(&stream->buf[stream->size], &((char*)ptr)[processed], copy_len);
        stream->size += copy_len;

        //move the file's cursor
        stream->cursor += copy_len;
        stream->can_flush = true;

        //count the bytes we've processed
        processed += copy_len;
    }

    return processed;
}

int so_fseek(SO_FILE *stream, long offset, int whence){
    
    //flush the buffer if we wrote last time
    if(stream->can_flush){
        so_fflush(stream);
    }

    int err = lseek(stream->fd, offset, whence);

    //move the cursor to the new possition
    stream->cursor = err;
    stream->size = 0;
    stream->buf_cursor = 0;

    if(err == -1){
        stream->error = true;
        return SO_EOF;
    }

    return 0;
}

long so_ftell(SO_FILE *stream){
    return stream->cursor;
}

int so_fileno(SO_FILE *stream){
    return stream->fd;
}

int so_feof(SO_FILE *stream){
    return (int)stream->eof;
}

int so_fflush(SO_FILE *stream){

    int err = write(stream->fd, stream->buf, sizeof(char) * (stream->size));

    stream->size = 0;
    stream->can_flush = false;

    if(err <= 0){
        stream->error = true;
        return SO_EOF;
    }

    return 0;
}

int so_ferror(SO_FILE *stream){
    return (int)stream->error;
}

int main(){

    SO_FILE* file = so_fopen("test.txt", "r+");
    so_fwrite(file, sizeof(char), 4, file);
    so_fclose(file);

    return 0;
}