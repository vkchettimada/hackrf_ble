#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include <hackrf.h>

static uint8_t do_exit = 0;

static void sfn_signal_callback(int signum)
{
  printf("sfn_signal_callback: %d\n", signum);

  do_exit = 1;
}

static uint8_t bit_get(uint8_t *p_bytes, uint8_t bit_index)
{
  return ((p_bytes[bit_index>>3] >> (bit_index & 0x07)) & 0x01);
}

static void bit_set(uint8_t *p_bytes, uint8_t bit_index, uint8_t bit_value)
{
  if (bit_value)
  {
    p_bytes[bit_index >> 3] |= (1 << (bit_index & 0x07));
  }
  else
  {
    p_bytes[bit_index >> 3] &= ~(1 << (bit_index & 0x07));
  }
}

static uint32_t rb_free_get(uint32_t size, uint32_t head, uint32_t tail, uint32_t *p_free0)
{
  uint32_t free0, free1;

  if (tail >= head)
  {
    free0 = size - tail;
    free1 = head;
  }
  else
  {
    free0 = head - tail;
    free1 = 0;
  }

  if (p_free0)
  {
    *p_free0 = free0;
  }

  return(free0 + free1 - 1);
}

static uint32_t rb_write(uint8_t *p_dst, uint32_t dst_size, uint32_t head, uint32_t *p_tail
                        , uint8_t *p_src, uint32_t src_size)
{
  uint32_t free0;

  if (src_size > rb_free_get(dst_size, head, *p_tail, &free0))
  {
    return(0);
  }

  if (src_size > free0)
  {
    uint8_t tail;

    memcpy(p_dst + *p_tail, p_src, free0);
    tail = src_size - free0;
    memcpy(p_dst, p_src + free0, tail);
    *p_tail = tail;
  }
  else
  {
    memcpy(p_dst + *p_tail, p_src, src_size);
    *p_tail += src_size;
  }

  return(src_size);
}

static uint32_t rb_avail_get(uint32_t size, uint32_t head, uint32_t tail, uint32_t *p_avail0)
{
  uint32_t avail0, avail1;

  if (tail >= head)
  {
    avail0 = tail - head;
    avail1 = 0;
  }
  else
  {
    avail0 = size - head;
    avail1 = tail;
  }

  if (p_avail0)
  {
    *p_avail0 = avail0;
  }

  return(avail0 + avail1);
}

static uint32_t rb_read(uint8_t *p_dst, uint32_t dst_size
                        , uint8_t *p_src, uint32_t src_size, uint32_t *p_head, uint32_t tail)
{
  uint32_t avail0;

  if (dst_size > rb_avail_get(src_size, *p_head, tail, &avail0))
  {
    return(0);
  }

  if (dst_size > avail0)
  {
    uint32_t head;

    head = dst_size - avail0;

    if (p_dst)
    {
      memcpy(p_dst, p_src + *p_head, avail0);
      memcpy(p_dst + avail0, p_src, head);
    }

    *p_head = head;
  }
  else
  {
    if (p_dst)
    {
      memcpy(p_dst, p_src + *p_head, dst_size);
    }

    *p_head += dst_size;
  }

  return(dst_size);
}

static int gfsk_demod(uint8_t sps, int8_t *p_sample, uint32_t size
                      , uint8_t match, uint8_t *p_bytes, uint32_t bit_count
                      , int8_t **pp_sample, uint32_t *p_size)
{
  int retcode = 1;

  if (size >= (2 * sps * bit_count)) /* 2 bytes IQ sample at samples per symbol for match_bit_count */
  {
    uint32_t bit_index = 0;
    int8_t *p_sample_next;
    uint32_t size_next;
    int I0, Q0;

    p_sample_next = p_sample + 2;
    size_next = size - 2;

    I0 = *p_sample++;
    Q0 = *p_sample++;

    size -= 2;
    while (size >= ((2 * sps) - 2))
    {
      int I1, Q1;
      uint8_t bit_value;

      I1 = *p_sample++;
      Q1 = *p_sample++;

      bit_value = (( (I0 * Q1) - (I1 * Q0) ) > 0 ) ? 1 : 0;
      //printf("%d", bit_value);
      if (!match || bit_get(p_bytes, bit_index) == bit_value)
      {
        if (!match)
        {
          bit_set(p_bytes, bit_index, bit_value);
        }

        p_sample += ((2 * sps) - 4);
        size -= ((2 * sps) - 2);

        if ((++bit_index) == bit_count)
        {
          retcode = 0;

          break;
        }
      }
      else
      {
        bit_index = 0;

        p_sample = p_sample_next;
        size = size_next;
        p_sample_next = p_sample + 2;
        size_next = size - 2;
      }

      I0 = *p_sample++;
      Q0 = *p_sample++;

      size -= 2;
    }
  }

  *pp_sample = p_sample;
  *p_size = size;

  return(retcode);
}

uint8_t whiten(uint8_t * p_lfsr, uint8_t in)
{
  uint8_t i;
  uint8_t out;

  out = 0;
  for (i = 0; i < 8; i++)
  {
    out >>= 1;
    out |= (((in ^ *p_lfsr) & 1) << 7);
    in >>= 1;

    *p_lfsr = ((*p_lfsr >> 1) | ((*p_lfsr & 1) << 6)) ^ ((*p_lfsr & 1) << 2);
  }

  return(out);
}

static uint32_t ble_crc(uint32_t crc_init, uint8_t *p_data, uint16_t data_len)
{
  uint32_t state = crc_init;
  uint32_t lfsr_mask = 0x5a6000; // 010110100110000000000000
  uint32_t i, j;

  for (i = 0; i < data_len; i++)
  {
    uint8_t cur = p_data[i];

    for (j = 0; j < 8; j++)
    {
      uint32_t next_bit = (state ^ cur) & 1;
      cur >>= 1;
      state >>= 1;
      if (next_bit)
      {
        state |= 1 << 23;
        state ^= lfsr_mask;
      }
    }
  }

  return state;
}

static int sfn_hackrf_sample_block_cb(hackrf_transfer *p_transfer)
{
  #define SPS (2) /* Samples per symbol */
  #define RB_SIZE ((262144) + (265 * 8 * 2 * SPS) + 1) /* HackRF buffer length + max sample for air interface of 265 bytes + 1 byte ring buffer roll-over */
  static uint8_t rb[RB_SIZE];
  static uint32_t rb_head = 0, rb_tail = 0;
  static uint32_t s_elapsed_us = 0;
  static uint32_t s_elapsed_match_us = 0;
  static uint32_t s_elapsed_end_us = 0;
  uint32_t access_address = 0x8E89BED6;
  int retcode;
  uint32_t rb_head_tmp;
  int8_t buffer[RB_SIZE - 1];
  uint32_t buffer_length;
  int8_t *p_sample_pdu, *p_sample_pdu_prev, *p_sample_pdu_end;
  uint32_t sample_pdu_size;
  uint8_t p_pdu[258];

  //#define DEBUG (1)
  #if DEBUG
  printf("buffer_length: (%d, %d)"
    , p_transfer->buffer_length
    , p_transfer->valid_length);
  #endif

  if (p_transfer->valid_length > rb_free_get(RB_SIZE, rb_head, rb_tail, 0))
  {
    do_exit = 1;

    return(0);
  }

  #if DEBUG
  printf(", write: (%d, %d)", rb_head, rb_tail);
  #endif

  (void) rb_write(rb, RB_SIZE, rb_head, &rb_tail, p_transfer->buffer, p_transfer->valid_length);

  #if DEBUG
  printf(", read: (%d, %d)", rb_head, rb_tail);
  #endif

  buffer_length = rb_avail_get(RB_SIZE, rb_head, rb_tail, 0);
  rb_head_tmp = rb_head;
  (void) rb_read(buffer, buffer_length, rb, RB_SIZE, &rb_head_tmp, rb_tail);

  #if DEBUG
  printf(", read: (%d, %d)\n", rb_head, rb_tail);
  #endif

  p_sample_pdu = buffer;
  sample_pdu_size = buffer_length;
  p_sample_pdu_prev = p_sample_pdu;
  p_sample_pdu_end = p_sample_pdu;

  /** @todo instead of size, take head-tail-rb_size */
  while (0 == (retcode = gfsk_demod(SPS, p_sample_pdu, sample_pdu_size
                    , 1, (uint8_t *) &access_address, sizeof(access_address) * 8
                    , &p_sample_pdu, &sample_pdu_size)))

  {
    uint32_t elapsed_us, delta_us, delta_end_start_us;

    /* calc elapsed time */
    elapsed_us = s_elapsed_us + ((p_sample_pdu - buffer) / (2 * SPS));

    /* calc delta from previous address match */
    delta_us = s_elapsed_match_us + ((p_sample_pdu - p_sample_pdu_prev) / (2 * SPS));
    s_elapsed_match_us = 0;

    /* calc delta from previous pdu end */
    delta_end_start_us = s_elapsed_end_us + ((p_sample_pdu - p_sample_pdu_end) / (2 * SPS));
    s_elapsed_end_us = 0;
    printf("%010dus %010dus %010dus", elapsed_us, delta_us, delta_end_start_us);

    /* remember current address as previous value for next iteration */
    p_sample_pdu_prev = p_sample_pdu;

    /* decode the address matched PDU */
    retcode = gfsk_demod(SPS, p_sample_pdu, sample_pdu_size
                        , 0, p_pdu, 16
                        , &p_sample_pdu, &sample_pdu_size);

    if (0 == retcode)
    {
      uint8_t lfsr;
      uint8_t len;

      lfsr = 37 | (1<<6);
      p_pdu[0] = whiten(&lfsr, p_pdu[0]);
      p_pdu[1] = whiten(&lfsr, p_pdu[1]);
      len = p_pdu[1] & 0x3F;

      printf(" Header: %02X %02X, len: %02d.", p_pdu[0], p_pdu[1], len);

      retcode = gfsk_demod(SPS, p_sample_pdu, sample_pdu_size
                          , 0, &p_pdu[2], (len + 3) * 8
                          , &p_sample_pdu, &sample_pdu_size);

      if (0 == retcode)
      {
        uint8_t *p_payload;
        uint8_t l;
        uint32_t crc;
        uint32_t crc_pdu;

        p_sample_pdu_end = p_sample_pdu;

        printf(" Payload:");
        p_payload = &p_pdu[2];
        l = len + 3;
        while(l--)
        {
          *p_payload = whiten(&lfsr, *p_payload);

          printf(" %02X", *p_payload);

          p_payload++;
        }

        crc_pdu = (p_pdu[len + 2 + 2] << 16) | (p_pdu[len + 2 + 1] << 8) | p_pdu[len + 2];

        crc = ble_crc(0xAAAAAA, p_pdu, len + 2);
        printf(" CRC: %06X. %s", crc, (crc == crc_pdu) ? "OK.": "");
      }
    }

    putchar('\n');

    if (0 != retcode)
    {
      p_sample_pdu = p_sample_pdu_prev - (sizeof(access_address) * 8 * 2 * SPS);

      break;
    }
  }

  (void) rb_read(0, (p_sample_pdu - buffer), rb, RB_SIZE, &rb_head, rb_tail);

  s_elapsed_us += (p_sample_pdu - buffer) / (2 * SPS);
  s_elapsed_match_us += (p_sample_pdu - p_sample_pdu_prev) / (2 * SPS);
  s_elapsed_end_us += (p_sample_pdu - p_sample_pdu_end) / (2 * SPS);

  return(0);
}

int main(int argc, char **argv)
{
  int retcode;
  hackrf_device *p_device;
  uint64_t freq;
  double sps;
  double bandwidth;

  signal(SIGINT, &sfn_signal_callback);

  retcode = hackrf_init();
  if (retcode)
  {
    printf("hackrf_init: %d.\n", retcode);
    goto exit;
  }

  p_device = NULL;
  retcode = hackrf_open(&p_device);
  if (retcode)
  {
    printf("hackrf_open: %d.\n", retcode);
    goto exit_exit;
  }

  freq = 2402000000ul;
  retcode = hackrf_set_freq(p_device, freq);
  if (retcode)
  {
    printf("hackrf_set_freq: %d.\n", retcode);
    goto exit_close;
  }

  sps = 2000000ul;
  retcode = hackrf_set_sample_rate(p_device, sps);
  if (retcode)
  {
    printf("hackrf_set_sample_rate: %d.\n", retcode);
    goto exit_close;
  }

  bandwidth = 1000000ul;
  retcode = hackrf_set_baseband_filter_bandwidth(p_device, bandwidth);
  if (retcode)
  {
    printf("hackrf_set_baseband_filter_bandwidth: %d.\n", retcode);
    goto exit_close;
  }

  retcode = hackrf_start_rx(p_device, sfn_hackrf_sample_block_cb, NULL);
  if (retcode)
  {
    printf("hackrf_start_rx: %d.\n", retcode);
    goto exit_close;
  }

  while(!do_exit) {
    sleep(1);
    // putchar('.');
  }
  putchar('\n');

  retcode = hackrf_stop_rx(p_device);
  if (retcode)
  {
    printf("hackrf_stop_rx: %d.\n", retcode);
  }

exit_close:
  retcode = hackrf_close(p_device);
  if (retcode)
  {
    printf("hackrf_close: %d.\n", retcode);
  }

exit_exit:
  retcode = hackrf_exit();
  if (retcode)
  {
    printf("hackrf_exit: %d.\n", retcode);
  }

exit:
  return(retcode);
}
