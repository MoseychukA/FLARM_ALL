#include "adsb_decoder_io.h"

static QueueHandle_t s_q = nullptr;

bool packet_queue_init(size_t depth){
  if (s_q) return true;
  s_q = xQueueCreate(depth, sizeof(adsb_packet_t));
  return s_q != nullptr;
}

bool enqueue_packet_from_core1(const adsb_packet_t& pkt){
  if (!s_q) return false;
  return xQueueSend(s_q, &pkt, 0) == pdTRUE;
}

bool dequeue_packet_for_core0(adsb_packet_t* out, uint32_t timeout_ms){
  if (!s_q) return false;
  return xQueueReceive(s_q, out, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}