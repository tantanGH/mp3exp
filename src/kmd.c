#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <jstring.h>
#include <iocslib.h>
#include "himem.h"
#include "kmd.h"

static uint8_t BLANKS[] = "                                                                                ";

//
//  initialize kmd handle
//
int32_t kmd_init(KMD_HANDLE* kmd, FILE* fp) {

  // default return code
  int32_t rc = -1;

  // reset attributes
  if (kmd == NULL) goto exit;
  kmd->current_event_ofs = 0;
  kmd->num_events = 0;
  kmd->events = NULL;

  // KMD file header check
  if (fp == NULL) goto exit;
  static uint8_t line[ KMD_MAX_LINE_LEN ];
  if (fgets(line, KMD_MAX_LINE_LEN, fp) == NULL) goto exit;
  if (memcmp(line, "KMD100", 6) != 0) goto exit;

  // pass 1 - count events
  size_t num_events = 0;
  while (fgets(line, KMD_MAX_LINE_LEN, fp) != NULL) {
    if (line[0] == 'x') num_events++;
  }

  // allocate the necessary memory
  kmd->num_events = num_events;
  kmd->events = (KMD_EVENT*)himem_malloc(sizeof(KMD_EVENT) * kmd->num_events, 0);
  if (kmd->events == NULL) goto exit;

  // pass 2 - read events
  size_t i = 0;
  fseek(fp, 0, SEEK_SET);
  while (fgets(line, KMD_MAX_LINE_LEN, fp) != NULL) {
    if (line[0] == 'x') {
      KMD_EVENT* e = &(kmd->events[i]);
      int16_t x,y,s0,s1,s2,e0,e1,e2;
      if (sscanf(line, "x%hd,y%hd,s%hd:%hd:%hd,e%hd:%hd:%hd,", &x, &y, &s0, &s1, &s2, &e0, &e1, &e2) == 8) {
        uint8_t* m0 = jstrchr(line,'"');
        uint8_t* m1 = jstrrchr(line,'"');
        if (m0 != NULL && m1 != NULL && m0 < m1) {
          e->pos_x = x;
          e->pos_y = y;
          e->active = 0;
          e->start_msec = s0 * 60000 + s1 * 1000 + s2 * 10;
          e->end_msec = e0 * 60000 + e1 * 1000 + e2 * 10;
          size_t m_len = m1 - m0 - 1;
          if (m_len > KMD_MAX_MESSAGE_LEN) m_len = KMD_MAX_MESSAGE_LEN;
          memcpy(e->message, m0 + 1, m_len);
          e->message[ m_len ] = '\0';
        }
        i++;
      }
    }
  }

  rc = 0;

exit:
  return rc;
}

//
//  close kmd handle
//
void kmd_close(KMD_HANDLE* kmd) {
  // reclaim buffer
  if (kmd->events != NULL) {
    himem_free(kmd->events, 0);
    kmd->events = NULL;
  }
}

//
//  get next kmd event
//
KMD_EVENT* kmd_next_event(KMD_HANDLE* kmd) {
  if (kmd == NULL || kmd->events == NULL || kmd->current_event_ofs >= kmd->num_events) return NULL;
  KMD_EVENT* next_event = &(kmd->events[ kmd->current_event_ofs ]);
  kmd->current_event_ofs++;
  return next_event;
}

//
//  erase event message
//
void kmd_erase_event_message(KMD_HANDLE* kmd, KMD_EVENT* event) {
  static uint8_t xs[128];
  if (kmd != NULL && event != NULL) {
    if (event->pos_y == 0) {
      sprintf(xs, "\r\x1b[%dC%s\r", event->pos_x + 1, BLANKS + strlen(BLANKS) - strlen(event->message));
      B_PRINT(xs);
    } else if (event->pos_y == 1) {
      sprintf(xs, "\r\x1b[%dC%s\r", event->pos_x + 1, BLANKS + strlen(BLANKS) - strlen(event->message));
      B_PRINT(xs);
    } else if (event->pos_y == 2) {
      sprintf(xs, "\n\r\x1b[%dC%s\x1b[1A\r", event->pos_x + 1, BLANKS + strlen(BLANKS) - strlen(event->message));
      B_PRINT(xs);
    }
  }
}

//
//  print event message
//
void kmd_print_event_message(KMD_HANDLE* kmd, KMD_EVENT* event) {
  static uint8_t xs[128];
  if (kmd != NULL && event != NULL) {
    if (event->pos_y == 0) {
      sprintf(xs, "\r\x1b[%dC%s\r", event->pos_x + 1, event->message);
      B_PRINT(xs);
    } else if (event->pos_y == 1) {
      sprintf(xs, "\r\x1b[%dC%s\r", event->pos_x + 1, event->message);
      B_PRINT(xs);
    } else if (event->pos_y == 2) {
      sprintf(xs, "\n\r\x1b[%dC%s\x1b[1A\r", event->pos_x + 1, event->message);
      B_PRINT(xs);
    }
  }
}

//
//  deactivate events
//
void kmd_deactivate_events(KMD_HANDLE* kmd, uint32_t elapsed) {
  if (kmd != NULL) {
    for (int16_t i = 0; i < kmd->current_event_ofs; i++) {
      KMD_EVENT* event = &(kmd->events[i]);
      if (event->active && event->end_msec <= elapsed) {
        kmd_erase_event_message(kmd, event);
        event->active = 0;
      }
    }
  }
}

//
//  activate current event
//
void kmd_activate_current_event(KMD_HANDLE* kmd, uint32_t elapsed) {
  if (kmd != NULL && kmd->current_event_ofs < kmd->num_events) {
    KMD_EVENT* event = &(kmd->events[ kmd->current_event_ofs ]);
    if (!event->active && event->start_msec <= elapsed) {
      kmd_print_event_message(kmd, event);
      event->active = 1;
      kmd->current_event_ofs++;
    }
  }
}
