

#ifndef APP_H
#define APP_H

void app_hrs_update(uint16_t val);

void app_ndn_update(const char *data, size_t len);

int app_ndn_send_interest(const char *name);

void app_ndn_init(void);


#endif /* APP_H */
