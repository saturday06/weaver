#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/file.h>

#define BUFFER_SIZE 1024

struct buffer_data {
    int fd;
};

static int read_packet(void *opaque, uint8_t *buf, int buf_size)
{    
    struct buffer_data *bd = (struct buffer_data *)opaque;
    int r = recv(bd->fd, buf, buf_size, 0);
    int i;
    printf("--- read ---");
    for (i = 0; i < r; i++) {
        if (i % 16 == 0) {
            printf("\n");
        }
        printf("%02x ", 0x00ff & buf[i]);
    }
    printf("\n");
    return r;
}

static int write_packet(void *opaque, uint8_t *buf, int buf_size)
{
    int i;
    printf("--- write ---");
    for (i = 0; i < buf_size; i++) {
        if (i % 16 == 0) {
            printf("\n");
        }
        printf("%02x ", 0x00ff & buf[i]);
    }
    printf("\n");

    struct buffer_data *bd = (struct buffer_data *)opaque;
    return send(bd->fd, buf, buf_size, 0);
}

int session(int fd)
{
    AVFormatContext *fmt_ctx = NULL;
    AVIOContext *avio_ctx = NULL;
    uint8_t *buffer = NULL, *avio_ctx_buffer = NULL;
    size_t buffer_size, avio_ctx_buffer_size = 4096;
    char input_filename[] = "rtmp://127.0.0.1:12348/?listen";
    int ret = 0;
    struct buffer_data bd = { 0 };
    bd.fd = fd;

    if (!(fmt_ctx = avformat_alloc_context())) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    avio_ctx_buffer = av_malloc(avio_ctx_buffer_size);
    if (!avio_ctx_buffer) {
        ret = AVERROR(ENOMEM);
        goto end;
    }
    avio_ctx = avio_alloc_context(avio_ctx_buffer, avio_ctx_buffer_size,
                                  0, &bd, &read_packet, &write_packet, NULL);
    if (!avio_ctx) {
        ret = AVERROR(ENOMEM);
        goto end;
    }
    fmt_ctx->pb = avio_ctx;

    AVInputFormat *input_fmt = av_find_input_format("live_flv");
    if (input_fmt == NULL) {
        fprintf(stderr, "Could not open input format\n");
        exit(1);
    }
    ret = avformat_open_input(&fmt_ctx, input_filename, input_fmt, NULL);
    if (ret < 0) {
        fprintf(stderr, "Could not open input\n");
        goto end;
    }

    ret = avformat_find_stream_info(fmt_ctx, NULL);
    if (ret < 0) {
        fprintf(stderr, "Could not find stream information\n");
        goto end;
    }

    av_dump_format(fmt_ctx, 0, input_filename, 0);

end:
    avformat_close_input(&fmt_ctx);
    /* note: the internal buffer could have changed, and be != avio_ctx_buffer */
    if (avio_ctx) {
        av_freep(&avio_ctx->buffer);
        av_freep(&avio_ctx);
    }
    av_file_unmap(buffer, buffer_size);

    if (ret < 0) {
        fprintf(stderr, "Error occurred: %s\n", av_err2str(ret));
        return 1;
    }

    return 0;
}

int main (int argc, char *argv[]) {
  //av_register_all();
  avformat_network_init();

  int port = 12348;

  int server_fd, client_fd, err;
  struct sockaddr_in server, client;
  char buf[BUFFER_SIZE];

  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    perror("Could not create socket");
    return EXIT_FAILURE;
  }

  server.sin_family = AF_INET;
  server.sin_port = htons(port);
  server.sin_addr.s_addr = htonl(INADDR_ANY);

  int opt_val = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof opt_val);

  err = bind(server_fd, (struct sockaddr *) &server, sizeof(server));
  if (err < 0) {
    perror("Could not bind socket");
    return EXIT_FAILURE;
  }

  err = listen(server_fd, 128);
  if (err < 0) {
    perror("Could not listen on socket");
    return EXIT_FAILURE;
  }

  printf("Server is listening on %d\n", port);

  while (1) {
    socklen_t client_len = sizeof(client);
    client_fd = accept(server_fd, (struct sockaddr *) &client, &client_len);

    if (client_fd < 0) {
      perror("Could not establish new connection"); 
      return EXIT_FAILURE;
    }

    return session(client_fd);
    /*
    while (1) {
      int read = recv(client_fd, buf, BUFFER_SIZE, 0);

      if (!read) {
        break;
      } else if (read < 0) {
        perror("Client read failed"); 
        return EXIT_FAILURE;
      }
      
      int i;
      for (i = 0; i < read; i++) {
        printf("%02x ", 0x00ff & buf[i]);
      }
      fflush(stdout);

      err = send(client_fd, buf, read, 0);
      if (err < 0) {
        perror("Client write failed\n");
        return EXIT_FAILURE;
      }
    }*/
  }

  return EXIT_SUCCESS;
}

