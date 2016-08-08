/*
 * [XRT] TODO: Copyright header
 */

#ifdef __APPLE__

#include <TargetConditionals.h>
#if TARGET_OS_IPHONE && TARGET_OS_IOS

#ifndef MACH_EXCEPTION_H_
#define MACH_EXCEPTION_H_

bool InstallMachExceptionHandler();

#endif /* MACH_EXCEPTION_H_ */

#else /* TARGET_OS_IPHONE && TARGET_OS_IOS */

#error Unsupported target, this feature requires TARGET_OS_IPHONE && TARGET_OS_IOS

#endif /* TARGET_OS_IPHONE && TARGET_OS_IOS */

#endif /* __APPLE__ */
