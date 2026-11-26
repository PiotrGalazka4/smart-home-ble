#ifndef CLIENT_LINK_H_
#define CLIENT_LINK_H_

int client_link_init(void);
int client_link_start(void);
void client_link_pairing_confirm(void);
void client_link_pairing_reject(void);
void client_link_set_pairing_start(void);

#endif /* CLIENT_LINK_H_ */