#ifndef WIFI_TCP_MGR_H
#define WIFI_TCP_MGR_H

// Respostas possíveis para o remetente
typedef enum {
    TCP_REPLY_ACK,   // "mensagem recebida (ACK)"
    TCP_REPLY_RECD,  // "reconhecido (recd)"
    TCP_REPLY_NRECD, // "não reconhecido (nrecd)"
    TCP_REPLY_NPLT   // "not a planet (nplt)"
} tcp_reply_t;

void wifi_tcp_init(void);
void tcp_send_reply(tcp_reply_t reply_type);

#endif