/*
 * [XRT] TODO: Copyright header
 */

#ifdef __APPLE__

#include "MachoUtils.h"

#include <mach-o/dyld.h>
#include <mach-o/dyld_images.h>
#include <mach-o/getsect.h>

void* GetARTData(unsigned long* bytesCount)
{
  return GetSectionData("__ARTDATA", "__artdata", bytesCount);
}

void* GetOATData(unsigned long* bytesCount)
{
  return GetSectionData("__OATDATA", "__oatdata", bytesCount);
}

void* GetSectionData(const char *segmentName, const char *sectionName, unsigned long *bytesCount)
{
  void* result = nullptr;
  
  int imagesCount;
  unsigned int imageIndex;
  
#ifdef __LP64__
  const mach_header_64* imageHeader;
#else
  const mach_header* imageHeader;
#endif
  
  uint32_t desiredType = MH_EXECUTE;
  
  imagesCount = _dyld_image_count();
  
  if(imagesCount > 0)
  {
    imageIndex = 0;
    
    for(int i = 0; i < imagesCount; i++)
    {
#ifdef __LP64__
      imageHeader = (struct mach_header_64 *)_dyld_get_image_header(imageIndex);
#else
      imageHeader = _dyld_get_image_header(imageIndex);
#endif
      
      if(imageHeader->filetype == desiredType)
      {
        result = getsectiondata(imageHeader, segmentName, sectionName, bytesCount);
        
        break;
      }
    }
  }
  
  return result;
}

#endif /* __APPLE__ */
