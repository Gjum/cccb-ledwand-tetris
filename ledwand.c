#include "ledwand.h"

#include <zlib.h>

int ledwand_init(Ledwand *ledwand) {
	if (ledwand == NULL) {
		ledwand = malloc(sizeof(*ledwand));
	}

	// open socket
	bzero(&ledwand->s_addr, sizeof(ledwand->s_addr));
	ledwand->s_addr.sin_family = AF_INET;
	ledwand->s_addr.sin_port = htons(2342);
	if (!inet_aton(LEDWAND_IP, &ledwand->s_addr.sin_addr)) {
		perror("inet_aton failed\n");
		return -1;
	}
	if ((ledwand->s_sock = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, IPPROTO_UDP)) == -1) {
		perror("socket failed\n");
		return -1;
	}

	return 0;
}

void ledwand_destroy(const Ledwand *ledwand) {
	close(ledwand->s_sock);
}

void ledwand_send(const Ledwand *ledwand,
                  const uint16_t cmd,
                  const uint16_t xpos,
                  const uint16_t ypos,
                  const uint16_t xsize,
                  const uint16_t ysize,
                  const uint8_t *text,
                  const uint32_t text_len)
{
	uint8_t buf[LEDWAND_BUFSIZE];

	Request request;
	request.cmd = htons(cmd);
	request.xpos = htons(xpos);
	request.ypos = htons(ypos);
	request.xsize = htons(xsize);
	request.ysize = htons(ysize);

	if (text_len > (LEDWAND_BUFSIZE - text_len)) {
		perror("ERROR: Textlen bigger than buflen\n");
		return;
	}

	size_t len = sizeof(request);
	memcpy(&buf, &request, len);
	if (text != NULL) {
		memcpy(&buf[len], text, text_len);
		len += text_len;
	}

	if (sendto(ledwand->s_sock, buf, len, 0, (struct sockaddr*)&ledwand->s_addr, sizeof(ledwand->s_addr)) <= 0) {
		perror("Send failed\n");
	}
}

void ledwand_clear(const Ledwand *ledwand) {
	ledwand_send(ledwand, CLEAR, 0, 0, 0, 0, NULL, 0);
}

void ledwand_set_brightness(const Ledwand *ledwand, const uint8_t brightness) {
	ledwand_send(ledwand, SET_BRIGHTNESS, 0, 0, 0, 0, &brightness, sizeof(brightness));
}

void ledwand_draw_image(const Ledwand *ledwand, uint8_t *tmpbuf, const uint32_t buf_len){
    uint8_t buf[8985]; // 10 + 56*8*20 + compress overhead
    uint16_t *b16 = (uint16_t *)buf;
    z_stream zs;

    if (buf_len != (LEDWAND_PIXEL_X * LEDWAND_PIXEL_Y)) {
        printf("Buffer size (%d) should be 448*240\n", buf_len);
        return;
    }

    memset(buf, 0, 10);
    b16[0] = ntohs(LED_DRAW);
    b16[2] = ntohs((sizeof buf) - 10);
    b16[3] = ntohs(0x677a);

    memset(&zs, 0, sizeof zs);
    if (Z_OK != deflateInit(&zs, 9)) {
       perror("deflateInit");
       return;
    }

    zs.next_in = tmpbuf;
    zs.next_out = buf + 10;
    zs.avail_out = sizeof buf - 10;
    zs.data_type = Z_BINARY;

    for (int i = 0; i < 19; i++) {
        zs.avail_in = 448;
        if (Z_OK != deflate(&zs, Z_NO_FLUSH)) {
            printf("deflate error\n");
            return;
        }
        zs.next_in += 4*LEDWAND_MODULE_X;
    }

    zs.avail_in = 448;
    if (Z_STREAM_END != deflate(&zs, Z_FINISH)) {
        printf("deflate error\n");
        return;
    }

    if (Z_OK != deflateEnd(&zs)) {
       perror("deflateEnd");
       return;
    }

    if (-1 == sendto(ledwand->s_sock, buf, sizeof buf - zs.avail_out, 0, (struct sockaddr*)&ledwand->s_addr, sizeof ledwand->s_addr))
        perror("send");
}

