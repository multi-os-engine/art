/*
 * [XRT] TODO: Copyright header
 */

#ifdef __APPLE__

#ifndef __xrtSDKiOS__MachoUtils__
#define __xrtSDKiOS__MachoUtils__

#ifdef __cplusplus
extern "C" {
#endif
    
    
void* GetARTData(unsigned long* bytesCount);
void* GetOATData(unsigned long* bytesCount);
    
void* GetSectionData(const char* segmentName, const char* sectionName, unsigned long* bytesCount);

#ifdef __cplusplus
}
#endif

#endif

#endif /* __APPLE__ */
