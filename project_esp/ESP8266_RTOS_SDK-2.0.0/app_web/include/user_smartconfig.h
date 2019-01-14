#ifndef __USER_SMARTCONFIG_H__
#define __USER_SMARTCONFIG_H__

#ifdef __cplusplus
extern "C" {
#endif

void smartconfig_done(sc_status status, void *pdata);
void SmartConfigTaskCreate( void );
void ICACHE_FLASH_ATTR init_done_cb_init(void);


#ifdef __cplusplus
}
#endif

#endif
