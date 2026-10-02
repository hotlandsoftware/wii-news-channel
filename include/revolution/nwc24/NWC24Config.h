#ifndef RVL_SDK_NWC24_CONFIG_H
#define RVL_SDK_NWC24_CONFIG_H
#include <types.h>
#include <macros.h>
#include <stddef.h>

#include <revolution/nwc24/NWC24Types.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef enum { NWC24_IDCS_INITIAL, NWC24_IDCS_GENERATED, NWC24_IDCS_REGISTERED } NWC24IDCreationStage;

NWC24Err NWC24GetMyUserId(NWC24UserId* pUserId);
NWC24Err NWC24GenerateNewUserId(NWC24UserId* pUserId);
const char* NWC24GetAccountDomain(void);
const char* NWC24GetMBoxDir(void);
u32 NWC24GetAppId(void);
u16 NWC24GetGroupId(void);
NWC24Err NWC24GetIdCreationStage(NWC24IDCreationStage* pStage);

#ifdef __cplusplus
}
#endif
#endif
